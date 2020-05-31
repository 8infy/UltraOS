#include "Core/Logger.h"
#include "Core/IO.h"

#include "ProgrammableIntervalTimer.h"

namespace kernel {

    ProgrammableIntervalTimer::ProgrammableIntervalTimer()
        : InterruptRequestHandler(timer_irq)
    {
        set_frequency(default_ticks_per_second);
    }

    void ProgrammableIntervalTimer::set_frequency(u32 ticks_per_second)
    {
        if (ticks_per_second > timer_frequency)
        {
            error() << "Cannot set the timer to frequency " << ticks_per_second;
            hang();
        }

        m_frequency = ticks_per_second;

        u32 divisor = timer_frequency / ticks_per_second;

        IO::out8<timer_command>(write_word | square_wave_mode | timer_0);

        IO::out8<timer_data>(divisor & 0x000000FF);
        IO::out8<timer_data>((divisor & 0x0000FF00) >> 8);
    }

    void ProgrammableIntervalTimer::on_irq(const RegisterState&)
    {
        static u32 tick = 0;
        static u32 seconds_elapsed = 0;

        ++tick;

        auto display_write = [] (const char* string, bool carrige_return = false)
                             {
                                 static u16* memory = reinterpret_cast<u16*>(0xB8000);

                                 if (carrige_return)
                                     memory = reinterpret_cast<u16*>(0xB8000);

                                 while (*string)
                                 {
                                     u16 colored_char = *(string++);
                                     colored_char |= 0x2 << 8;

                                     *(memory++) = colored_char;
                                 }
                             };

        if (tick / m_frequency)
        {
            info() << "Timer: " << ++seconds_elapsed << " seconds uptime";
            tick = 0;

            display_write("Uptime: ", true);

            bool ok;
            char number[11];

            to_string_buffer(seconds_elapsed, number, 11, ok);

            display_write(number);

            display_write(" seconds");
        }
    }
}
