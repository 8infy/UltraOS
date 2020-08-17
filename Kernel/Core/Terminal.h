#pragma once

#include "Common/DynamicArray.h"
#include "Common/String.h"
#include "Drivers/Video/Utilities.h"

namespace kernel {

class Terminal {
public:
    static void initialize();
    static bool is_initialized() { return s_instance != nullptr; }

    static Terminal& the()
    {
        ASSERT(s_instance != nullptr);
        return *s_instance;
    }

    void write(StringView text, RGBA color = RGBA::white());
    void write(char);

    void write_at(size_t column, size_t row, StringView text, RGBA color = RGBA::white());
    void write_at(size_t column, size_t row, char, RGBA color = RGBA::white());

    size_t columns() const;
    size_t rows() const;

    enum class ScrollDirection {
        UP,
        DOWN
    };

    void scroll(ScrollDirection);

private:
    static size_t row_as_offset(size_t row);
    static size_t column_as_offset(size_t column);

private:
    static Terminal* s_instance;

    size_t m_current_row    { 0 };
    size_t m_current_column { 0 };

    size_t m_viewing_row { 0 };

    DynamicArray<String> m_scrollback_buffer;

    static constexpr size_t font_height = 16;
    static constexpr size_t font_width  = 8;

    static const u8 s_font[256][font_height];
};
}
