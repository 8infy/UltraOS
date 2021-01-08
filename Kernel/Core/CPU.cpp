#include "GDT.h"

#include "Common/Utilities.h"

#include "Interrupts/IDT.h"
#include "Interrupts/InterruptController.h"
#include "Interrupts/LAPIC.h"

#include "Memory/MemoryManager.h"
#include "Memory/PAT.h"

#include "Multitasking/Process.h"
#include "Multitasking/TSS.h"

#include "CPU.h"

namespace kernel {

Atomic<size_t> CPU::s_alive_counter;
InterruptSafeSpinLock CPU::s_processors_lock;
List<CPU::LocalData> CPU::s_processors;

Thread& CPU::LocalData::idle_task()
{
    return *m_idle_process->threads().first();
}

CPU::ID::ID(u32 function)
{
    asm volatile("cpuid"
                 : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                 : "a"(function), "c"(0));
}

CPU::MSR CPU::MSR::read(u32 index)
{
    MSR out;

    asm volatile("rdmsr"
                 : "=d"(out.upper), "=a"(out.lower)
                 : "c"(index));

    return out;
}

void CPU::MSR::write(u32 index)
{
    asm volatile("wrmsr" ::"d"(upper), "a"(lower), "c"(index));
}

void CPU::initialize()
{
    if (supports_smp()) {
        s_processors.append_back(LAPIC::my_id());
    } else {
        s_processors.append_back(0);
    }

    ++s_alive_counter;
}

CPU::FLAGS CPU::flags()
{
    FLAGS flags;
    asm volatile("pushf\n"
                 "pop %0\n"
                 : "=a"(flags)::"memory");

    return flags;
}

bool CPU::supports_smp()
{
    return InterruptController::supports_smp();
}

void CPU::start_all_processors()
{
    if (!InterruptController::supports_smp())
        return;

    // We have to do this here because otherwise we have a race condition
    // with each cpu emplacing their local data upon entering ap_entrypoint.
    // Locking is also not an option because CPU::current would have to use this lock
    // as well, which would lead to recursively calling current() everywhere.
    auto& apic_ids = InterruptController::smp_data().application_processor_apic_ids;
    for (auto processor_id : apic_ids)
        s_processors.append_back(processor_id);

    for (auto processor_id : InterruptController::smp_data().application_processor_apic_ids) {
        LAPIC::start_processor(processor_id);

        auto& this_processor = CPU::at_id(processor_id);
        
        while (!this_processor.is_online())
            ;
    }

    // now LAPIC is the primary timer
    Timer::get_specific(Timer::Type::LAPIC).make_primary();
    Timer::primary().enable(); // enable for the BSP
}

CPU::LocalData& CPU::at_id(u32 id)
{
    auto cpu = linear_search(s_processors.begin(), s_processors.end(), id,
                             [] (const LocalData& cpu_data, u32 id_to_find)
                             {
                                 return cpu_data.id() == id_to_find;
                             });

    if (cpu == s_processors.end()) {
        String error_string;
        error_string << "CPU: Couldn't find the local data for cpu " << id;
        runtime::panic(error_string.data());
    }

    return *cpu;
}

CPU::LocalData& CPU::current()
{
    if (InterruptController::is_initialized() && supports_smp())
        return at_id(LAPIC::my_id());
    else
        return at_id(0);
}

u32 CPU::current_id()
{
    if (is_initialized())
        return current().id();

    return 0;
}

void CPU::LocalData::bring_online()
{
    ASSERT(m_is_online == false);
    ASSERT(LAPIC::my_id() == m_id);

    m_is_online = true;
}

void CPU::ap_entrypoint()
{
    GDT::the().install();
    IDT::the().install();
    PAT::the().synchronize();

    LAPIC::initialize_for_this_processor();
    auto& timer = Timer::get_specific(Timer::Type::LAPIC);
    timer.calibrate_for_this_processor();
    timer.enable();
    
    auto& current_cpu = CPU::current();
    current_cpu.set_idle_task(Process::create_idle_for_this_processor());
    current_cpu.set_tss(new TSS);

    current_cpu.bring_online();
    ++s_alive_counter;

    Interrupts::enable();

    for (;;)
        hlt();
}
}
