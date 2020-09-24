#include "Memory/MemoryManager.h"

#include "Multitasking/Process.h"
#include "Multitasking/Scheduler.h"

#include "IPICommunicator.h"
#include "IRQManager.h"
#include "Timer.h"

#include "LAPIC.h"

namespace kernel {

Address       LAPIC::s_base;
LAPIC::Timer* LAPIC::s_timer;

void LAPIC::set_base_address(Address physical_base)
{
#ifdef ULTRA_32
    s_base = AddressSpace::of_kernel().allocator().allocate_range(4096).begin();

    AddressSpace::of_kernel().map_page(s_base, physical_base);
#elif defined(ULTRA_64)
    s_base = MemoryManager::physical_to_virtual(physical_base);
#endif
}

void LAPIC::initialize_for_this_processor()
{
    auto current_value = read_register(Register::SPURIOUS_INTERRUPT_VECTOR);

    static constexpr u32 enable_bit = 0b100000000;

    write_register(Register::SPURIOUS_INTERRUPT_VECTOR, current_value | enable_bit | spurious_irq_index);

    if (!s_timer)
        s_timer = new Timer();
}

void LAPIC::Timer::initialize_for_this_processor()
{
    // We can't use PIT simulatneously from multiple processors
    bool interrupt_state = false;
    ::kernel::Timer::the().lock(interrupt_state);

    static constexpr u32 divider_16 = 0b11;
    // TODO: replace with numeric_limits<u32>::max()
    static constexpr u32 initial_counter = 0xFFFFFFFF;

    static constexpr u32 sleep_delay = Time::milliseconds_in_second / ticks_per_second;

    write_register(Register::DIVIDE_CONFIGURATION, divider_16);
    write_register(Register::INITIAL_COUNT, initial_counter);

    ::kernel::Timer::the().mili_delay(sleep_delay);

    auto total_ticks = initial_counter - read_register(Register::CURRENT_COUNT);

    write_register(Register::LVT_TIMER, irq_number | periodic_mode);
    write_register(Register::INITIAL_COUNT, total_ticks);

    ::kernel::Timer::the().unlock(interrupt_state);
}

void LAPIC::Timer::handle_irq(const RegisterState& registers)
{
    end_of_interrupt();

    if (CPU::current().is_bsp()) {
        // If the timer doesn't have an internal counter than we have to manually increment it
        if (!::kernel::Timer::the().has_internal_counter())
            ::kernel::Timer::the().increment_time_since_boot(Time::nanoseconds_in_second / ticks_per_second);

        Time::increment_by(Time::nanoseconds_in_second / ticks_per_second);
    }

    Scheduler::on_tick(registers);
}

void LAPIC::Timer::enable_irq()
{
    auto current_value = read_register(Register::LVT_TIMER);
    write_register(Register::LVT_TIMER, current_value & ~masked_bit);
}

void LAPIC::Timer::disable_irq()
{
    auto current_value = read_register(Register::LVT_TIMER);
    write_register(Register::LVT_TIMER, current_value | masked_bit);
}

void LAPIC::write_register(Register reg, u32 value)
{
    *Address(s_base + static_cast<size_t>(reg)).as_pointer<volatile u32>() = value;
}

u32 LAPIC::read_register(Register reg)
{
    return *Address(s_base + static_cast<size_t>(reg)).as_pointer<volatile u32>();
}

void LAPIC::end_of_interrupt()
{
    write_register(Register::END_OF_INTERRUPT, 0);
}

u32 LAPIC::my_id()
{
    return (read_register(Register::ID) >> 24) & 0xFF;
}

void LAPIC::send_ipi(DestinationType destination_type, u32 destination)
{
    if (destination_type == DestinationType::SPECIFIC && destination == invalid_destination) {
        error() << "LAPIC: invalid destination with destination type == DEFAULT";
        hang();
    }

    ICR icr {};

    icr.vector_number    = IPICommunicator::vector_number;
    icr.delivery_mode    = DeliveryMode::NORMAL;
    icr.destination_mode = DestinationMode::PHYSICAL;
    icr.level            = Level::ASSERT;
    icr.trigger_mode     = TriggerMode::EDGE;
    icr.destination_type = destination_type;
    icr.destination_id   = destination;

    volatile auto* icr_pointer = Address(&icr).as_pointer<volatile u32>();

    write_register(Register::INTERRUPT_COMMAND_REGISTER_HIGHER, *(icr_pointer + 1));
    write_register(Register::INTERRUPT_COMMAND_REGISTER_LOWER, *icr_pointer);
}

void LAPIC::send_init_to(u8 id)
{
    ICR icr {};

    icr.delivery_mode    = DeliveryMode::INIT;
    icr.destination_mode = DestinationMode::PHYSICAL;
    icr.level            = Level::ASSERT;
    icr.trigger_mode     = TriggerMode::EDGE;
    icr.destination_type = DestinationType::SPECIFIC;
    icr.destination_id   = id;

    volatile auto* icr_pointer = Address(&icr).as_pointer<volatile u32>();

    write_register(Register::INTERRUPT_COMMAND_REGISTER_HIGHER, *(icr_pointer + 1));
    write_register(Register::INTERRUPT_COMMAND_REGISTER_LOWER, *icr_pointer);
}

void LAPIC::send_startup_to(u8 id)
{
    static constexpr u8 entrypoint_page_number = 1;

    ICR icr {};

    icr.entrypoint_page  = entrypoint_page_number;
    icr.delivery_mode    = DeliveryMode::SIPI;
    icr.destination_mode = DestinationMode::PHYSICAL;
    icr.level            = Level::ASSERT;
    icr.trigger_mode     = TriggerMode::EDGE;
    icr.destination_type = DestinationType::SPECIFIC;
    icr.destination_id   = id;

    volatile auto* icr_pointer = Address(&icr).as_pointer<volatile u32>();

    write_register(Register::INTERRUPT_COMMAND_REGISTER_HIGHER, *(icr_pointer + 1));
    write_register(Register::INTERRUPT_COMMAND_REGISTER_LOWER, *icr_pointer);
}

// defined in Architecture/X/APTrampoline.asm
extern "C" void application_processor_entrypoint();

void LAPIC::start_processor(u8 id)
{
    log() << "LAPIC: Starting application processor " << id << "...";

    static constexpr Address entrypoint_base        = 0x1000;
    static constexpr Address entrypoint_base_linear = MemoryManager::physical_to_virtual(entrypoint_base);

    static bool entrypoint_relocated = false;

    static constexpr ptr_t address_of_alive        = 0x500;
    static constexpr ptr_t address_of_acknowldeged = 0x510;
    static constexpr ptr_t address_of_stack        = 0x520;

#ifdef ULTRA_64
    static constexpr ptr_t   address_of_pml4 = 0x530;
    static constexpr Address pml4_linear     = MemoryManager::physical_to_virtual(address_of_pml4);

    // Identity map the first pdpt for the AP
    AddressSpace::of_kernel().entry_at(0) = AddressSpace::of_kernel().entry_at(256);
#endif

    volatile auto* is_ap_alive     = MemoryManager::physical_to_virtual(address_of_alive).as_pointer<bool>();
    volatile auto* ap_acknowledegd = MemoryManager::physical_to_virtual(address_of_acknowldeged).as_pointer<bool>();

    *is_ap_alive     = false;
    *ap_acknowledegd = false;

    // TODO: add a literal allocate_stack() function
    // allocate the initial AP stack
    auto ap_stack = AddressSpace::of_kernel().allocator().allocate_range(Process::default_kernel_stack_size);
    *MemoryManager::physical_to_virtual(address_of_stack).as_pointer<ptr_t>() = ap_stack.end();
    MemoryManager::the().force_preallocate(ap_stack, true);

    if (!entrypoint_relocated) {
        copy_memory(reinterpret_cast<void*>(&application_processor_entrypoint),
                    entrypoint_base_linear.as_pointer<void>(),
                    Page::size);

#ifdef ULTRA_64
        *pml4_linear.as_pointer<ptr_t>() = AddressSpace::of_kernel().physical_address();
#endif
        entrypoint_relocated = true;
    }

    bool interrupt_state = false;
    ::kernel::Timer::the().lock(interrupt_state);

    send_init_to(id);

    ::kernel::Timer::the().mili_delay(10);

    send_startup_to(id);

    ::kernel::Timer::the().mili_delay(1);

    if (*is_ap_alive) {
        log() << "LAPIC: Application processor " << id << " started successfully";

        // allow the ap to boot further
        *ap_acknowledegd = true;

        ::kernel::Timer::the().unlock(interrupt_state);
        return;
    }

    send_startup_to(id);

    // if we're here we're probably dealing with an old CPU that needs two SIPIs

    // wait for 1 second
    for (size_t i = 0; i < 20; ++i)
        ::kernel::Timer::the().mili_delay(50);

    if (*is_ap_alive) {
        log() << "LAPIC: Application processor " << id << " started successfully after second SIPI";

        // allow the ap to boot further
        *ap_acknowledegd = true;

    } else {
        error() << "LAPIC: Application processor " << id << " failed to start after 2 SIPIs";
        hang();
    }

    ::kernel::Timer::the().unlock(interrupt_state);
}
}
