#include "APIC.h"
#include "Core/CPU.h"
#include "IOAPIC.h"
#include "LAPIC.h"
#include "PIC.h"

namespace kernel {

APIC::APIC()
{
    PIC::ensure_disabled();
    LAPIC::set_base_address(CPU::smp_data().local_apic_address);
    LAPIC::initialize_for_this_processor();
    IOAPIC::set_base_address(CPU::smp_data().io_apic_address);
}

void APIC::end_of_interrupt(u8)
{
    LAPIC::end_of_interrupt();
}

void APIC::clear_all() { }

void APIC::enable_irq(u8 index)
{
    // TODO: Fix this hack, its super dirty.
    //       (it assumes there's an override entry for the PIT)
    if (index == 0) {
        IOAPIC::map_legacy_irq(2, IRQManager::irq_base_index, CPU::smp_data().bootstrap_processor_apic_id);
        return;
    }

    IOAPIC::map_legacy_irq(index, IRQManager::irq_base_index + index, CPU::smp_data().bootstrap_processor_apic_id);
}

void APIC::disable_irq(u8 index)
{
    (void)index;
}

bool APIC::is_spurious(u8 request_number)
{
    return request_number == LAPIC::spurious_irq_index;
}

void APIC::handle_spurious_irq(u8)
{
    // do nothing for APIC, it doesn't need an EOI
}
}