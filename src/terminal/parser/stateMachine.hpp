// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
Module Name:
- stateMachine.hpp

Abstract:
- This declares the entire state machine for handling Virtual Terminal Sequences
- The design is based from the specifications at http://vt100.net
- The actual implementation of actions decoded by the StateMachine should be
  implemented in an IStateMachineEngine.
*/

#pragma once

#include "IStateMachineEngine.hpp"
#include "tracing.hpp"
#include <memory>

#include "../vtparser/v1/Parser_v1.h"

namespace Microsoft::Console::VirtualTerminal
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

    class StateMachine final : public v1::Parser
    {
#ifdef UNIT_TESTING
        friend class OutputEngineTest;
        friend class InputEngineTest;
#endif

    public:
        template<typename T>
        StateMachine(std::unique_ptr<T> engine) :
            StateMachine(std::move(engine), std::is_same_v<T, class InputStateMachineEngine>)
        {
        }
        StateMachine(std::unique_ptr<IStateMachineEngine> engine, const bool isEngineForInput);

        using Mode = defs::ParserMode;

        void SetParserMode(const Mode mode, const bool enabled) noexcept;
        bool GetParserMode(const Mode mode) const noexcept;

        bool IsProcessingLastCharacter() const noexcept;

        void OnCsiComplete(const std::function<void()> callback);

        bool FlushToTerminal();

        const IStateMachineEngine& Engine() const noexcept;
        IStateMachineEngine& Engine() noexcept;

        class ShutdownException : public wil::ResultException
        {
        public:
            ShutdownException() noexcept :
                ResultException(E_ABORT) {}
        };

    private:

        template<typename TLambda>
        bool _SafeExecute(TLambda&& lambda);
    };
}
