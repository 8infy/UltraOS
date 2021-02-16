#include "MP.h"
#include "Common/Logger.h"
#include "Memory/MemoryManager.h"

namespace kernel {

MP::FloatingPointer* MP::s_floating_pointer;

SMPData* MP::parse()
{
    ASSERT(s_floating_pointer == nullptr);

    s_floating_pointer = find_floating_pointer_table();

    if (s_floating_pointer == nullptr)
        return nullptr;

    log() << "MP: Found the floating pointer table at "
          << MemoryManager::virtual_to_physical(s_floating_pointer);

    return parse_configuration_table(s_floating_pointer);
}

MP::FloatingPointer* MP::find_floating_pointer_table()
{
    static constexpr auto mp_floating_pointer_signature = "_MP_"_sv;
    static constexpr size_t table_alignment = 16;

    static constexpr Address ebda_base = 0x80000;
    static constexpr Address ebda_end = 0x9FFFF;
    auto ebda_range = Range::from_two_pointers(MemoryManager::physical_to_virtual(ebda_base), MemoryManager::physical_to_virtual(ebda_end));

    log() << "MP: Trying to find the floating pointer table in the EBDA...";

    auto string = mp_floating_pointer_signature.find_in_range(ebda_range, table_alignment);

    if (!string.empty())
        return Address(string.begin()).as_pointer<FloatingPointer>();

    static constexpr Address bios_rom_base = 0xF0000;
    static constexpr Address bios_rom_end = 0xFFFFF;
    auto bios_rom_range = Range::from_two_pointers(MemoryManager::physical_to_virtual(bios_rom_base), MemoryManager::physical_to_virtual(bios_rom_end));

    log() << "MP: Couldn't find the floating pointer table in the EBDA, trying ROM...";

    string = mp_floating_pointer_signature.find_in_range(bios_rom_range, table_alignment);

    if (!string.empty())
        return Address(string.begin()).as_pointer<FloatingPointer>();

    static constexpr Address bda_address = 0x400;
    static constexpr Address bda_address_linear = MemoryManager::physical_to_virtual(bda_address);
    static constexpr size_t kilobytes_before_ebda_offset = 0x13;

    auto kilobytes_before_ebda = *Address(bda_address_linear + kilobytes_before_ebda_offset).as_pointer<u16>();
    auto last_base_kilobyte = MemoryManager::physical_to_virtual((kilobytes_before_ebda - 1) * KB);
    auto last_base_kilobyte_range = Range(last_base_kilobyte, 1 * KB);

    log() << "MP: Couldn't find the floating pointer table in ROM, trying last 1KB of base memory...";

    string = mp_floating_pointer_signature.find_in_range(last_base_kilobyte_range, table_alignment);

    if (!string.empty())
        return Address(string.begin()).as_pointer<FloatingPointer>();

    warning() << "MP: Couldn't find the floating pointer table, reverting to single core configuration.";

    return nullptr;
}

SMPData* MP::parse_configuration_table(FloatingPointer* fp_table)
{
    static constexpr u8 imcr_register = SET_BIT(7);

    if (fp_table->features & imcr_register) {
        log() << "MP: IMCR register found, switching to APIC mode...";

        static constexpr u8 imcr = 0x70;
        static constexpr u8 select = 0x22;
        static constexpr u8 setting = 0x23;
        static constexpr u8 apic_mode = 1;

        IO::out8<select>(imcr);
        IO::out8<setting>(IO::in8<setting>() | apic_mode);
    }

    // TODO: handle this case
    if (fp_table->default_configuration)
        return nullptr;

    auto configuration_table_linear = MemoryManager::physical_to_virtual(fp_table->configuration_table_pointer);

    auto& configuration_table = *configuration_table_linear.as_pointer<MP::ConfigurationTable>();

    static constexpr auto mp_configuration_table_signature = "PCMP"_sv;

    if (configuration_table.signature != mp_configuration_table_signature) {
        warning() << "MP: incorrect configuration table signature, ignoring the rest of the table...";
        return nullptr;
    }

    auto* smp_info = new SMPData;
    smp_info->lapic_address = configuration_table.lapic_address;

    log() << "MP: LAPIC @ " << smp_info->lapic_address;

    Address entry_address = &configuration_table + 1;

    Optional<u8> isa_bus_id {};
    Optional<u8> pci_bus_id {};

    DynamicArray<Pair<u8, LAPICInfo::NMI>> lapic_id_to_nmi;

    for (size_t i = 0; i < configuration_table.entry_count; ++i) {
        EntryType type = *entry_address.as_pointer<MP::EntryType>();

        switch (type) {
        case EntryType::PROCESSOR: {
            auto& processor = *entry_address.as_pointer<ProcessorEntry>();

            bool is_bsp = processor.flags & ProcessorEntry::Flags::BSP;
            bool is_ok = processor.flags & ProcessorEntry::Flags::OK;

            log() << "MP: CPU " << processor.lapic_id
                  << " is bsp: " << is_bsp << " | online capable: " << is_ok;

            if (is_bsp)
                smp_info->bsp_lapic_id = processor.lapic_id;

            smp_info->lapics.append({ processor.lapic_id, 0xFF, {} });

            break;
        }
        case EntryType::BUS: {
            auto& bus = *entry_address.as_pointer<BusEntry>();

            log() << "MP: Bus id " << bus.id << " type " << StringView(bus.type_string, 6);

            auto isa_bus = "ISA"_sv;
            auto pci_bus = "PCI"_sv;

            if (StringView(bus.type_string, 3) == isa_bus)
                isa_bus_id = bus.id;
            if (StringView(bus.type_string, 3) == pci_bus)
                pci_bus_id = bus.id;

            break;
        }
        case EntryType::IO_APIC: {
            auto& io_apic = *entry_address.as_pointer<IOAPICEntry>();

            bool is_ok = io_apic.flags & IOAPICEntry::Flags::OK;

            log() << "MP: I/O APIC at " << format::as_hex << io_apic.ioapic_address
                  << " is_ok:" << is_ok << " id:" << io_apic.id;

            if (!is_ok)
                break;

            // This is mostly because they don't specify GSI base for IOAPICs but instead give you
            // the IOAPIC id in interrupt assigment entries, this is very inconvenient and would require
            // a few annoying workarounds to combine this with ACPI MADT IOAPIC entries. Since MP tables
            // are mostly obsolete anyway I decided not to do that.
            if (!smp_info->ioapics.empty())
                FAILED_ASSERTION("Multiple IOAPICs are not supported for intel MP");

            // We assume GSI base of 0 for single IOAPIC configuration
            smp_info->ioapics.append({ io_apic.id, { 0, 0 }, io_apic.ioapic_address });

            break;
        }
        case EntryType::IO_INTERRUPT_ASSIGNMENT:
        case EntryType::LOCAL_INTERRUPT_ASSIGNMENT: {
            ASSERT(isa_bus_id.has_value());

            auto& interrupt = *entry_address.as_pointer<InterruptEntry>();

            const char* str_type = type == EntryType::IO_INTERRUPT_ASSIGNMENT ? "I/O Interrupt Assignment"
                                                                              : "Local Interrupt Assignment";

            const char* str_apic = type == EntryType::IO_INTERRUPT_ASSIGNMENT ? "IOAPIC" : "LAPIC";

            log() << "MP: " << str_type << " entry:"
                  << "\n----> Type: " << InterruptEntry::to_string(interrupt.interrupt_type) << " Interrupt"
                  << "\n----> Polarity: " << to_string(interrupt.polarity)
                  << "\n----> Trigger mode: " << to_string(interrupt.trigger_mode)
                  << "\n----> Source bus ID: " << interrupt.source_bus_id
                  << "\n----> Source bus IRQ: " << interrupt.source_bus_irq << "\n----> Destination " << str_apic
                  << " id: " << interrupt.destination_ioapic_id << "\n----> Destination " << str_apic
                  << " pin: " << interrupt.destination_ioapic_pin << "\n";

            if (type == EntryType::LOCAL_INTERRUPT_ASSIGNMENT && interrupt.interrupt_type == InterruptEntry::Type::NMI) {
                Pair<u8, LAPICInfo::NMI> nmi;
                nmi.set_first(interrupt.destination_lapic_id);
                auto& info = nmi.second();
                info.polarity = interrupt.polarity == Polarity::CONFORMING ? Polarity::ACTIVE_HIGH : interrupt.polarity;
                info.trigger_mode = interrupt.trigger_mode == TriggerMode::CONFORMING ? TriggerMode::EDGE : interrupt.trigger_mode;
                info.lint = interrupt.destination_lapic_lint;
                lapic_id_to_nmi.append(nmi);

                break;
            }

            // TODO: 1. handle lapic assignemnts separetely (LINT0/LINT1 stuff)
            //       2. handle interrupt types other than the default vectored
            //       3. handle interrupts that are coming from the PCI bus
            if (type == EntryType::LOCAL_INTERRUPT_ASSIGNMENT
                || interrupt.interrupt_type != InterruptEntry::Type::INT
                || interrupt.source_bus_id != isa_bus_id.value())
                break;

            // TODO: take care of the possibility of having multiple IOAPICs
            IRQ info {};
            info.irq_index = interrupt.source_bus_irq;
            info.gsi = interrupt.destination_ioapic_pin;

            if (interrupt.polarity != Polarity::CONFORMING) {
                info.polarity = interrupt.polarity;
            } else if (interrupt.source_bus_id == isa_bus_id.value()) {
                info.polarity = default_polarity_for_bus(Bus::ISA);
            } else if (interrupt.source_bus_id == pci_bus_id.value()) {
                info.polarity = default_polarity_for_bus(Bus::PCI);
            } else {
                warning()
                    << "MP: interrupt polarity was declared conforming but the default mode for bus "
                    << interrupt.source_bus_id << " is unknown.\n";
                break;
            }

            if (interrupt.trigger_mode != TriggerMode::CONFORMING) {
                info.trigger_mode = interrupt.trigger_mode;
            } else if (interrupt.source_bus_id == isa_bus_id.value()) {
                info.trigger_mode = default_trigger_mode_for_bus(Bus::ISA);
            } else if (interrupt.source_bus_id == pci_bus_id.value()) {
                info.trigger_mode = default_trigger_mode_for_bus(Bus::PCI);
            } else {
                warning() << "MP: interrupt trigger mode was declared conforming\n"
                             "          but the default mode for bus "
                          << interrupt.source_bus_id << " is unknown.\n";
                break;
            }

            smp_info->legacy_irqs_to_info[info.irq_index] = info;
            break;
        }
        default:
            warning() << "MP: unknown type of entry " << static_cast<u8>(type) << ", skipping the entire table";
            delete smp_info;
            return nullptr;
        }

        entry_address += sizeof_entry(type);
    }

    static constexpr u8 all_processors_uid = 0xFF;

    if (!lapic_id_to_nmi.empty()) {
        if (lapic_id_to_nmi[0].first() == all_processors_uid) { // single NMI entry for all LAPICs
            ASSERT(lapic_id_to_nmi.size() == 1);

            for (auto& lapic : smp_info->lapics)
                lapic.nmi_connection = lapic_id_to_nmi[0].second();
        } else {
            for (auto& nmi_entry : lapic_id_to_nmi) {
                auto& lapics = smp_info->lapics;

                auto lapic = linear_search(lapics.begin(), lapics.end(), nmi_entry.first(),
                    [](const LAPICInfo& info, u8 id) {
                        return info.id == id;
                    });

                // This isn't an error when it happens in ACPI so I guess it isn't here either
                if (lapic == lapics.end())
                    continue;

                lapic->nmi_connection = nmi_entry.second();
            }
        }
    }

    return smp_info;
}

}
