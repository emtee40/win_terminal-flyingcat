#pragma once

#include <string_view>
#include <optional>
#include <functional>
#include <minwindef.h>

#include "../defs.hpp"
#include "../shared.h"
#include "../inc/til/enumset.h"
#include "../inc/til/small_vector.h"
#include "../../parser/tracing.hpp"

#include "./Generated Files/ParserGenerated.g.h"

using namespace Microsoft::Console::VirtualTerminal;

namespace v1 {

class Parser : public ParserGenerated<Parser>
{
public:
    Parser(std::unique_ptr<IStateMachineEngine> engine, const bool isEngineForInput);

    void ProcessString(const std::wstring_view string);
    void ProcessCharacter(const wchar_t wch);

    void ResetState() noexcept;

    void OnProceed() noexcept
    {
        _trace.TraceOnEvent(_stateNames[(size_t)_state]);
    }

    bool IsInput() const { return _isEngineForInput; }
    bool IsAnsiMode() const { return _parserMode.test(defs::ParserMode::Ansi); }

    void ActionExecute() { _ActionExecute(_wch); }
    void ActionPrint() { _ActionPrint(_wch); }
    void ActionClear() { _ActionClear(); }
    void ActionVt52EscDispatch() { _ActionVt52EscDispatch(_wch); }
    void ActionExecuteFromEscape() { _ActionExecuteFromEscape(_wch); }
    void ActionEscDispatch() { _ActionEscDispatch(_wch); }
    void ActionIgnore() { _ActionIgnore(); }
    void ActionCollect() { _ActionCollect(_wch); }
    void ActionParam() { _ActionParam(_wch); }
    void ActionCsiDispatch() { _ActionCsiDispatch(_wch); }
    void ActionSubParam() { _ActionSubParam(_wch); }
    void ActionOscParam() { _ActionOscParam(_wch); }
    void ActionOscPut() { _ActionOscPut(_wch); }
    void ActionOscDispatch() { _ActionOscDispatch(_wch); }
    void ActionSs3Dispatch() { _ActionSs3Dispatch(_wch); }
    void ActionDcsDispatch() { _ActionDcsDispatch(_wch); }

    void ExecuteCsiCompleteCallback() { _ExecuteCsiCompleteCallback(); }

    void HandleVt52Param();

    void ExitDcsPassThrough();

    void HandleDcsPassThrough();

    void EraseCachedSequence() { _cachedSequence.reset(); }

protected:
    void _ActionExecute(const wchar_t wch);
    void _ActionExecuteFromEscape(const wchar_t wch);
    void _ActionPrint(const wchar_t wch);
    void _ActionPrintString(const std::wstring_view string);
    void _ActionEscDispatch(const wchar_t wch);
    void _ActionVt52EscDispatch(const wchar_t wch);
    void _ActionCollect(const wchar_t wch) noexcept;
    void _ActionParam(const wchar_t wch);
    void _ActionSubParam(const wchar_t wch);
    void _ActionCsiDispatch(const wchar_t wch);
    void _ActionOscParam(const wchar_t wch) noexcept;
    void _ActionOscPut(const wchar_t wch);
    void _ActionOscDispatch(const wchar_t wch);
    void _ActionSs3Dispatch(const wchar_t wch);
    void _ActionDcsDispatch(const wchar_t wch);

    void _ActionClear();
    void _ActionIgnore() noexcept;
    void _ActionInterrupt();

    void _ExecuteCsiCompleteCallback();

    std::unique_ptr<IStateMachineEngine> _engine;
    const bool _isEngineForInput;

    til::enumset<defs::ParserMode> _parserMode{ defs::ParserMode::Ansi };

    Microsoft::Console::VirtualTerminal::ParserTracing _trace;

    std::wstring_view _currentString;
    size_t _runOffset;
    size_t _runSize;
    const wchar_t* _curPos;

    // Construct current run.
    //
    // Note: We intentionally use this method to create the run lazily for better performance.
    //       You may find the usage of offset & size unsafe, but under heavy load it shows noticeable performance benefit.
    std::wstring_view _CurrentRun() const
    {
        return _currentString.substr(_runOffset, _runSize);
    }

    VTIDBuilder _identifier;
    //std::vector<VTParameter> _parameters;
    til::small_vector<VTParameter, defs::MAX_PARAMETER_COUNT> _parameters;
    bool _parameterLimitOverflowed;
    //std::vector<VTParameter> _subParameters;
    til::small_vector<VTParameter, defs::MAX_SUBPARAMETER_COUNT> _subParameters;
    til::small_vector<std::pair<BYTE /*range start*/, BYTE /*range end*/>, defs::MAX_PARAMETER_COUNT> _subParameterRanges;
    bool _subParameterLimitOverflowed;
    BYTE _subParameterCounter;

    std::wstring _oscString;
    VTInt _oscParameter;

    IStateMachineEngine::StringHandler _dcsStringHandler;

    std::optional<std::wstring> _cachedSequence;

    // This is tracked per state machine instance so that separate calls to Process*
    //   can start and finish a sequence.
    bool _processingLastCharacter;

    std::function<void()> _onCsiCompleteCallback;
};

}
