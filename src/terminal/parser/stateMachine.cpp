// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "stateMachine.hpp"

#include "ascii.hpp"

using namespace Microsoft::Console::VirtualTerminal;

//Takes ownership of the pEngine.
StateMachine::StateMachine(std::unique_ptr<IStateMachineEngine> engine, const bool isEngineForInput) :
    Parser(std::move(engine), isEngineForInput)
{
}

void StateMachine::SetParserMode(const Mode mode, const bool enabled) noexcept
{
    _parserMode.set(mode, enabled);
}

bool StateMachine::GetParserMode(const Mode mode) const noexcept
{
    return _parserMode.test(mode);
}

const IStateMachineEngine& StateMachine::Engine() const noexcept
{
    return *_engine;
}

IStateMachineEngine& StateMachine::Engine() noexcept
{
    return *_engine;
}




// Method Description:
// - Pass the current string we're processing through to the engine. It may eat
//      the string, it may write it straight to the input unmodified, it might
//      write the string to the tty application. A pointer to this function will
//      get handed to the OutputStateMachineEngine, so that it can write strings
//      it doesn't understand to the tty.
//  This does not modify the state of the state machine. Callers should be in
//      the Action*Dispatch state, and upon completion, the state's handler (eg
//      _EventCsiParam) should move us into the ground state.
// Arguments:
// - <none>
// Return Value:
// - true if the engine successfully handled the string.
bool StateMachine::FlushToTerminal()
{
    auto success{ true };

    if (success && _cachedSequence.has_value())
    {
        // Flush the partial sequence to the terminal before we flush the rest of it.
        // We always want to clear the sequence, even if we failed, so we don't accumulate bad state
        // and dump it out elsewhere later.
        success = _SafeExecute([=]() {
            return _engine->ActionPassThroughString(*_cachedSequence);
        });
        _cachedSequence.reset();
    }

    if (success)
    {
        // _pwchCurr is incremented after a call to ProcessCharacter to indicate
        //      that pwchCurr was processed.
        // However, if we're here, then the processing of pwchChar triggered the
        //      engine to request the entire sequence get passed through, including pwchCurr.
        success = _SafeExecute([=]() {
            return _engine->ActionPassThroughString(_CurrentRun());
        });
    }

    return success;
}



// Routine Description:
// - Determines whether the character being processed is the last in the
//   current output fragment, or there are more still to come. Other parts
//   of the framework can use this information to work more efficiently.
// Arguments:
// - <none>
// Return Value:
// - True if we're processing the last character. False if not.
bool StateMachine::IsProcessingLastCharacter() const noexcept
{
    return _processingLastCharacter;
}

// Routine Description:
// - Registers a function that will be called once the current CSI action is
//   complete and the state machine has returned to the ground state.
// Arguments:
// - callback - The function that will be called
// Return Value:
// - <none>
void StateMachine::OnCsiComplete(const std::function<void()> callback)
{
    _onCsiCompleteCallback = callback;
}


template<typename TLambda>
bool StateMachine::_SafeExecute(TLambda&& lambda)
try
{
    return lambda();
}
catch (const ShutdownException&)
{
    throw;
}
catch (...)
{
    LOG_HR(wil::ResultFromCaughtException());
    return false;
}

