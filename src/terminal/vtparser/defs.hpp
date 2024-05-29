#pragma once

#include "../parser/IStateMachineEngine.hpp"

using namespace Microsoft::Console::VirtualTerminal;

namespace defs
{
    // The DEC STD 070 reference recommends supporting up to at least 16384
    // for parameter values. 65535 is what XTerm and VTE support.
    // GH#12977: We must use 65535 to properly parse win32-input-mode
    // sequences, which transmit the UTF-16 character value as a parameter.
    constexpr VTInt MAX_PARAMETER_VALUE = 65535;

    // The DEC STD 070 reference requires that a minimum of 16 parameter values
    // are supported, but most modern terminal emulators will allow around twice
    // that number.
    constexpr size_t MAX_PARAMETER_COUNT = 32;

    // Sub parameter limit for each parameter.
    constexpr size_t MAX_SUBPARAMETER_COUNT = 6;
    // we limit ourself to 256 sub parameters because we use bytes to store
    // the their indexes.
    static_assert(MAX_PARAMETER_COUNT * MAX_SUBPARAMETER_COUNT <= 256);

    enum class ParserMode : size_t
    {
        AcceptC1,
        AlwaysAcceptC1,
        Ansi,
    };
}
