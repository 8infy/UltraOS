#pragma once

#include "Core/Types.h"
#include "Core/Macros.h"

namespace kernel {
    class InterruptDescriptorTable
    {
    public:
        enum attributes : u8
        {
            RING_0         = 0b0,
            RING_3         = SET_BIT(5) | SET_BIT(6),
            TASK_GATE      = SET_BIT(0) | SET_BIT(2),
            INTERRUPT_GATE = SET_BIT(1) | SET_BIT(2) | SET_BIT(3),
            TRAP_GATE      = SET_BIT(0) | SET_BIT(1) | SET_BIT(2) | SET_BIT(3),
            PRESENT        = SET_BIT(7),
        };

        static constexpr u16 entry_count = 256;
        using isr = void(*)();

        InterruptDescriptorTable& register_isr(u16 index, attributes attrs, isr handler);
        InterruptDescriptorTable& register_interrupt_handler(u16 index, isr handler);
        InterruptDescriptorTable& register_user_interrupt_handler(u16 index, isr handler);

        void install();

        static InterruptDescriptorTable& the();
    private:
        static constexpr u16 gdt_selector = 0x8;

        InterruptDescriptorTable();

        struct PACKED entry
        {
            u16 address_lower;
            u16 selector;
            u8  unused;
            u8  attributes;
            u16 address_higher;
        } m_entries[entry_count];

        struct PACKED pointer
        {
            u16     size;
            entry*  address;
        } m_pointer;

        static InterruptDescriptorTable s_instance;
    };
}