#pragma once

#include "Common/Macros.h"
#include "Common/Types.h"

#include "Core/Registers.h"

namespace kernel {

class IPICommunicator {
public:
    static constexpr u16 vector_number = 254;

    static void initialize();

    static void send_ipi(u8 dest);

private:
    static void on_ipi(RegisterState*) USED;
};
}