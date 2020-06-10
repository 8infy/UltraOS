#include "Core/Logger.h"

#include "GDT.h"

namespace kernel {
    GDT GDT::s_instance;

    GDT::GDT()
        : m_entries(), m_pointer{m_active_entries, m_entries}
    {
    }

    GDT& GDT::the()
    {
        return s_instance;
    }

    void GDT::create_basic_descriptors()
    {
        create_descriptor(0, 0, NULL_SELECTOR, flag_attributes(NULL_SELECTOR));
        create_descriptor(0, 0xFFFFFFFF, PRESENT | CODE_OR_DATA | EXECUTABLE | READABLE, GRANULARITY_4KB | MODE_32_BIT);
        create_descriptor(0, 0xFFFFFFFF, PRESENT | CODE_OR_DATA |              WRITABLE, GRANULARITY_4KB | MODE_32_BIT);
    }

    GDT::entry& GDT::new_entry()
    {
       if (m_active_entries >= entry_count)
       {
           error() << "GDT is out of free entries!";
           hang();
       }

       m_active_entries++;
       m_pointer.size += sizeof(entry);

       return m_entries[m_active_entries - 1];
    }

    void GDT::create_descriptor(u32 base, u32 size, access_attributes access, flag_attributes flags)
    {
        auto& this_entry = new_entry();

        this_entry.base_lower  =  base & 0x0000FFFF;
        this_entry.base_middle = (base & 0x00FF0000) >> 16;
        this_entry.base_upper  = (base & 0xFF000000) >> 24;

        this_entry.access = access;
        this_entry.flags  = flags;

        this_entry.limit_lower =  size & 0x0000FFFF;
        this_entry.limit_upper = (size & 0x000F0000) >> 16;
    }

    void GDT::install()
    {
        m_pointer.size -= 1;
        asm("lgdt %0" ::"m"(m_pointer));
        m_pointer.size += 1;
    }
}