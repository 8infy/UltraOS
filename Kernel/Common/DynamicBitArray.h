#pragma once

#include "DynamicArray.h"

namespace kernel {

    class DynamicBitArray
    {
    public:
        DynamicBitArray()
        {
        }

        DynamicBitArray(size_t initial_size)
        {
            set_size(initial_size);
        }

        size_t size() const
        {
            return m_bit_count;
        }

        void set_size(size_t bit_count)
        {
            m_bit_count = bit_count;

            if (m_bits.size() < m_bit_count / 8)
                m_bits.expand_to(m_bit_count / 8);
        }

        void set_bit(size_t at_index, bool to)
        {
            ASSERT(at_index < m_bit_count);

            auto& byte = m_bits[byte_of_bit(at_index)];

            if (to)
                byte |= bit_as_mask(index_of_bit(at_index));
            else
                byte &= ~bit_as_mask(index_of_bit(at_index));
        }

        void set_range_to(size_t at_index, size_t count, size_t number)
        {
            ASSERT(number <= ((static_cast<decltype(number)>(1) << count) - 1));

            for (size_t i = 0; i < count; ++i)
                set_bit(at_index + i, ((1 << i) & number) >> i);
        }

        bool bit_at(size_t index)
        {
            auto& byte = m_bits[byte_of_bit(index)];

            return byte & bit_as_mask(index_of_bit(index));
        }

        size_t range_as_number(size_t index, size_t length)
        {
            ASSERT(index + length <= m_bit_count);

            size_t number = 0;

            for (size_t i = 0; i < length; ++i)
                number |= bit_at(index + i) << i;

            return number;
        }

    private:
        size_t byte_of_bit(size_t bit)
        {
            return bit / 8;
        }

        size_t index_of_bit(size_t bit_index)
        {
            return bit_index - byte_of_bit(bit_index) * 8;
        }

        u8 bit_as_mask(size_t bit)
        {
            return 1 << bit;
        }

    private:
        size_t           m_bit_count{ 0 };
        DynamicArray<u8> m_bits;
    };
}