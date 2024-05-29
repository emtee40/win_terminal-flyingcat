#pragma once
#include "../precomp.h"

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

namespace v2
{

template<class EngineT, bool isEngineForInput>
class Parser : public ParserGenerated<Parser<EngineT, isEngineForInput>>
{
public:
    Parser(auto&&... args) :
        _engine(std::forward<decltype(args)>(args)...)
    {
        _ActionClear();
    }

    void ProcessString(const std::wstring_view string)
    {
        size_t i = 0;
        _currentString = string;
        _runOffset = 0;
        _runSize = 0;

        const wchar_t* stringEnd = string.data() + string.size();

        if (this->_state != VTStates::Ground)
        {
            // Jump straight to where we need to.
    #pragma warning(suppress : 26438) // Avoid 'goto'(es .76).
            goto processStringLoopVtStart;
        }

        while (i < string.size())
        {
            {
                _runOffset = i;
                // Pointer arithmetic is perfectly fine for our hot path.
    #pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).)
                _runSize = findActionableFromGround(string.data() + i, string.size() - i);

                if (_runSize)
                {
                    _ActionPrintString(_CurrentRun());

                    i += _runSize;
                    _runOffset = i;
                    _runSize = 0;
                }
            }

        processStringLoopVtStart:
            if (i >= string.size())
            {
                break;
            }

            _curPos = string.data() + i;
            do
            {
                _runSize++;
                _processingLastCharacter = _curPos + 1 >= stringEnd;
                // If we're processing characters individually, send it to the state machine.
                ProcessCharacter(*_curPos);
                ++_curPos;
            } while (_curPos < stringEnd && this->_state != VTStates::Ground);
            i = _curPos - string.data();
        }

        // If we're at the end of the string and have remaining un-printed characters,
        if (this->_state != VTStates::Ground)
        {
            // One of the "weird things" in VT input is the case of something like
            // <kbd>alt+[</kbd>. In VT, that's encoded as `\x1b[`. However, that's
            // also the start of a CSI, and could be the start of a longer sequence,
            // there's no way to know for sure. For an <kbd>alt+[</kbd> keypress,
            // the parser originally would just sit in the `CsiEntry` state after
            // processing it, which would pollute the following keypress (e.g.
            // <kbd>alt+[</kbd>, <kbd>A</kbd> would be processed like `\x1b[A`,
            // which is _wrong_).
            //
            // Fortunately, for VT input, each keystroke comes in as an individual
            // write operation. So, if at the end of processing a string for the
            // InputEngine, we find that we're not in the Ground state, that implies
            // that we've processed some input, but not dispatched it yet. This
            // block at the end of `ProcessString` will then re-process the
            // undispatched string, but it will ensure that it dispatches on the
            // last character of the string. For our previous `\x1b[` scenario, that
            // means we'll make sure to call `_ActionEscDispatch('[')`., which will
            // properly decode the string as <kbd>alt+[</kbd>.
            const auto run = _CurrentRun();

            if (_isEngineForInput)
            {
                // Reset our state, and put all but the last char in again.
                ResetState();
                _processingLastCharacter = false;
                // Chars to flush are [pwchSequenceStart, pwchCurr)
                auto wchIter = run.cbegin();
                while (wchIter < run.cend() - 1)
                {
                    ProcessCharacter(*wchIter);
                    wchIter++;
                }
                // Manually execute the last char [pwchCurr]
                _processingLastCharacter = true;
                switch (this->_state)
                {
                case VTStates::Ground:
                    _ActionExecute(*wchIter);
                    break;
                case VTStates::Escape:
                case VTStates::EscapeIntermediate:
                    _ActionEscDispatch(*wchIter);
                    break;
                case VTStates::CsiEntry:
                case VTStates::CsiIntermediate:
                case VTStates::CsiIgnore:
                case VTStates::CsiParam:
                case VTStates::CsiSubParam:
                    _ActionCsiDispatch(*wchIter);
                    break;
                case VTStates::OscParam:
                case VTStates::OscString:
                case VTStates::OscTermination:
                    _ActionOscDispatch(*wchIter);
                    break;
                case VTStates::Ss3Entry:
                case VTStates::Ss3Param:
                    _ActionSs3Dispatch(*wchIter);
                    break;
                }
                // microsoft/terminal#2746: Make sure to return to the ground state
                // after dispatching the characters
                this->EnterGround();
            }
            else if (this->_state != VTStates::SosPmApcString && this->_state != VTStates::DcsPassThrough && this->_state != VTStates::DcsIgnore)
            {
                // If the engine doesn't require flushing at the end of the string, we
                // want to cache the partial sequence in case we have to flush the whole
                // thing to the terminal later. There is no need to do this if we've
                // reached one of the string processing states, though, since that data
                // will be dealt with as soon as it is received.
                if (!_cachedSequence)
                {
                    _cachedSequence.emplace(std::wstring{});
                }

                auto& cachedSequence = *_cachedSequence;
                cachedSequence.append(run);
            }
        }
    }

    void ProcessCharacter(const wchar_t wch)
    {
        _trace.TraceCharInput(wch);

        if ((wch >= L'\x80' && wch <= L'\x9F')) [[unlikely]]
        {
            // But note that we only do this if C1 control code parsing has been
            // explicitly requested, since there are some code pages with "unmapped"
            // code points that get translated as C1 controls when that is not their
            // intended use. In order to avoid them triggering unintentional escape
            // sequences, we ignore these characters by default.
            if (_parserMode.any(defs::ParserMode::AcceptC1, defs::ParserMode::AlwaysAcceptC1))
            {
                this->Proceed<true>(0x1B);
                this->Proceed<true>(wch - L'\x40');
            }
        }
        else
        {
            this->Proceed<false>(wch);
        }
    }

    void ResetState() noexcept
    {
        this->EnterGround();
    }

    void OnProceed() noexcept
    {
        this->_trace.TraceOnEvent(this->_stateNames[(size_t)this->_state]);
    }

    static constexpr bool IsInput() { return _isEngineForInput; }
    bool IsAnsiMode() const { return _parserMode.test(defs::ParserMode::Ansi); }

    void ActionExecute() { _ActionExecute(this->_wch); }
    void ActionPrint() { _ActionPrint(this->_wch); }
    void ActionClear() { _ActionClear(); }
    void ActionVt52EscDispatch() { _ActionVt52EscDispatch(this->_wch); }
    void ActionExecuteFromEscape() { _ActionExecuteFromEscape(this->_wch); }
    void ActionEscDispatch() { _ActionEscDispatch(this->_wch); }
    void ActionIgnore() { _ActionIgnore(); }
    void ActionCollect() { _ActionCollect(this->_wch); }
    void ActionParam() { _ActionParam(this->_wch); }
    void ActionSubParam() { _ActionSubParam(this->_wch); }
    void ActionOscParam() { _ActionOscParam(this->_wch); }
    void ActionOscPut() { _ActionOscPut(this->_wch); }
    void ActionOscDispatch() { _ActionOscDispatch(this->_wch); }
    void ActionSs3Dispatch() { _ActionSs3Dispatch(this->_wch); }
    void ActionDcsDispatch() { _ActionDcsDispatch(this->_wch); }

    void ExecuteCsiCompleteCallback() { _ExecuteCsiCompleteCallback(); }

    void HandleVt52Param()
    {
        _parameters.push_back(this->_wch);
        if (_parameters.size() == 2)
        {
            // The command character is processed before the parameter values,
            // but it will always be 'Y', the Direct Cursor Address command.
            _ActionVt52EscDispatch(L'Y');
            this->EnterGround();
        }
    }

    void ExitDcsPassThrough()
    {
        _dcsStringHandler(0x1b);
        _dcsStringHandler = nullptr;
    }

    void HandleDcsPassThrough()
    {
        if (!_dcsStringHandler(this->_wch))
        {
            this->EnterDcsIgnore();
        }
    }

    void EraseCachedSequence() { _cachedSequence.reset(); }

    auto FinalizeIdentifier(const wchar_t wch)
    {
        return _identifier.Finalize(wch);
    }

    auto MakeVTParameters()
    {
        return VTParameters{ _parameters, _subParameters, _subParameterRanges };
    }

    template<size_t Length>
    static consteval auto MakeVTID(const char (&s)[Length])
    {
        return VTID(s);
    }

    auto& engine() { return _engine; }

    [[msvc::forceinline]]
    void ActionCsiDispatch(auto fn)
    {
        _trace.TraceOnAction(L"CsiDispatch");
        _trace.DispatchSequenceTrace(_SafeExecute(fn));
        //_SafeExecute(fn);
    }

    [[msvc::forceinline]]
    void ActionIllegalCsiDispatch()
    {
        _trace.TraceOnAction(L"CsiDispatch");
        _trace.DispatchSequenceTrace(false);
    }

    [[msvc::forceinline]]
    void ActionUnmatchedCsiDispatch()
    {
        _trace.TraceOnAction(L"CsiDispatch");
        _trace.DispatchSequenceTrace(false);
    }

    [[msvc::forceinline]]
    void ActionExecute_ENQ()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_ENQ();
        }));
        //_engine.ActionExecute_ENQ();
    }
    [[msvc::forceinline]]
    void ActionExecute_BEL()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_BEL();
        }));
        //_engine.ActionExecute_BEL();
    }
    [[msvc::forceinline]]
    void ActionExecute_BS()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_BS();
        }));
        //_engine.ActionExecute_BS();
    }
    [[msvc::forceinline]]
    void ActionExecute_TAB()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_TAB();
        }));
        //_engine.ActionExecute_TAB();
    }
    [[msvc::forceinline]]
    void ActionExecute_LF_FF_VT()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_LF_FF_VT();
        }));
        //_engine.ActionExecute_LF_FF_VT();
    }
    [[msvc::forceinline]]
    void ActionExecute_CR()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_CR();
        }));
        //_engine.ActionExecute_CR();
    }
    [[msvc::forceinline]]
    void ActionExecute_SO()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_SO();
        }));
        //_engine.ActionExecute_SO();
    }
    [[msvc::forceinline]]
    void ActionExecute_SI()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_SI();
        }));
        //_engine.ActionExecute_SI();
    }
    [[msvc::forceinline]]
    void ActionExecute_DEL()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_DEL();
        }));
        //_engine.ActionExecute_DEL();
    }
    [[msvc::forceinline]]
    void ActionExecute_SUB()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionExecute_SUB();
        }));
        //_engine.ActionExecute_SUB();
    }
    [[msvc::forceinline]]
    void ActionUnmatchedExecute()
    {
        _trace.TraceOnAction(L"Execute");
        _trace.DispatchSequenceTrace(_SafeExecute([=] {
            return _engine.ActionUnmatchedExecute();
        }));
        //_engine.ActionUnmatchedExecute();
    }



protected:
    template<typename TLambda>
    static bool _SafeExecute(TLambda&& lambda)
    try
    {
        return lambda();
    }
    catch (...)
    {
        return false;
    }

    static inline void _AccumulateTo(const wchar_t wch, VTInt& value) noexcept
    {
        const auto digit = wch - L'0';

        value = value * 10 + digit;

        // Values larger than the maximum should be mapped to the largest supported value.
        if (value > defs::MAX_PARAMETER_VALUE)
        {
            value = defs::MAX_PARAMETER_VALUE;
        }
    }

    static constexpr bool _isParameterDelimiter(const wchar_t wch) noexcept
    {
        return wch == L';'; // 0x3B
    }

    static constexpr bool _isSubParameterDelimiter(const wchar_t wch) noexcept
    {
        return wch == L':'; // 0x3A
    }

    [[msvc::forceinline]]
    void _ActionExecute(const wchar_t wch)
    {
        _trace.TraceOnExecute(wch);
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionExecute(wch);
        }));
    }

    [[msvc::forceinline]]
    void _ActionExecuteFromEscape(const wchar_t wch)
    {
        _trace.TraceOnExecuteFromEscape(wch);
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionExecuteFromEscape(wch);
        }));
    }

    [[msvc::forceinline]]
    void _ActionPrint(const wchar_t wch)
    {
        _trace.TraceOnAction(L"Print");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionPrint(wch);
        }));
    }

    [[msvc::forceinline]]
    void _ActionPrintString(const std::wstring_view string)
    {
        _SafeExecute([=]() {
            return _engine.ActionPrintString(string);
        });
        _trace.DispatchPrintRunTrace(string);
    }

    [[msvc::forceinline]]
    void _ActionEscDispatch(const wchar_t wch)
    {
        _trace.TraceOnAction(L"EscDispatch");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionEscDispatch(_identifier.Finalize(wch));
        }));
    }

    [[msvc::forceinline]]
    void _ActionVt52EscDispatch(const wchar_t wch)
    {
        _trace.TraceOnAction(L"Vt52EscDispatch");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionVt52EscDispatch(_identifier.Finalize(wch), { _parameters.data(), _parameters.size() });
        }));
    }

    [[msvc::forceinline]]
    void _ActionCollect(const wchar_t wch) noexcept
    {
        _trace.TraceOnAction(L"Collect");

        // store collect data
        _identifier.AddIntermediate(wch);
    }

    void _ActionParam(const wchar_t wch)
    {
        _trace.TraceOnAction(L"Param");

        // Once we've reached the parameter limit, additional parameters are ignored.
        if (!_parameterLimitOverflowed)
        {
            // If we have no parameters and we're about to add one, get the next value ready here.
            if (_parameters.empty())
            {
                _parameters.emplace_back();
                const auto rangeStart = gsl::narrow_cast<BYTE>(_subParameters.size());
                _subParameterRanges.emplace_back(rangeStart, rangeStart);
            }

            // On a delimiter, increase the number of params we've seen.
            // "Empty" params should still count as a param -
            //      eg "\x1b[0;;m" should be three params
            if (_isParameterDelimiter(wch))
            {
                // If we receive a delimiter after we've already accumulated the
                // maximum allowed parameters, then we need to set a flag to
                // indicate that further parameter characters should be ignored.
                if (_parameters.size() >= defs::MAX_PARAMETER_COUNT)
                {
                    _parameterLimitOverflowed = true;
                }
                else
                {
                    // Otherwise move to next param.
                    _parameters.emplace_back();
                    _subParameterCounter = 0;
                    _subParameterLimitOverflowed = false;
                    const auto rangeStart = gsl::narrow_cast<BYTE>(_subParameters.size());
                    _subParameterRanges.emplace_back(rangeStart, rangeStart);
                }
            }
            else
            {
                // Accumulate the character given into the last (current) parameter.
                // If the value hasn't been initialized yet, it'll start as 0.
                auto currentParameter = _parameters.back().value_or(0);

                _AccumulateTo(wch, currentParameter);
                auto p = _curPos + 1;
                for (auto end = _currentString.data() + _currentString.size(); p != end; p++)
                {
                    if (*p >= L'0' && *p <= '9')
                    {
                        _AccumulateTo(*p, currentParameter);
                    }
                    else
                    {
                        break;
                    }
                }

                _runSize += p - 1 - _curPos;
                _curPos = p - 1;
                _parameters.back() = currentParameter;
            }
        }
    }

    void _ActionSubParam(const wchar_t wch)
    {
        _trace.TraceOnAction(L"SubParam");

        // Once we've reached the sub parameter limit, sub parameters are ignored.
        if (!_subParameterLimitOverflowed)
        {
            // If we have no parameters and we're about to add a sub parameter, add an empty parameter here.
            if (_parameters.empty())
            {
                _parameters.emplace_back();
                const auto rangeStart = gsl::narrow_cast<BYTE>(_subParameters.size());
                _subParameterRanges.emplace_back(rangeStart, rangeStart);
            }

            // On a delimiter, increase the number of sub params we've seen.
            // "Empty" sub params should still count as a sub param -
            //      eg "\x1b[0:::m" should be three sub params
            if (_isSubParameterDelimiter(wch))
            {
                // If we receive a delimiter after we've already accumulated the
                // maximum allowed sub parameters for the parameter, then we need to
                // set a flag to indicate that further sub parameter characters
                // should be ignored.
                if (_subParameterCounter >= defs::MAX_SUBPARAMETER_COUNT)
                {
                    _subParameterLimitOverflowed = true;
                }
                else
                {
                    // Otherwise move to next sub-param.
                    _subParameters.emplace_back();
                    // increment current range's end index.
                    _subParameterRanges.back().second++;
                    // increment counter
                    _subParameterCounter++;
                }
            }
            else
            {
                // Accumulate the character given into the last (current) sub-parameter.
                // If the value hasn't been initialized yet, it'll start as 0.
                auto currentSubParameter = _subParameters.back().value_or(0);
                _AccumulateTo(wch, currentSubParameter);
                _subParameters.back() = currentSubParameter;
            }
        }
    }

    [[msvc::forceinline]]
    void _ActionCsiDispatch(const wchar_t wch)
    {
        _trace.TraceOnAction(L"CsiDispatch");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionCsiDispatch(_identifier.Finalize(wch),
                                              { _parameters, _subParameters, _subParameterRanges });
        }));
    }

    [[msvc::forceinline]]
    void _ActionOscParam(const wchar_t wch) noexcept
    {
        _trace.TraceOnAction(L"OscParamCollect");

        _AccumulateTo(wch, _oscParameter);
    }

    [[msvc::forceinline]]
    void _ActionOscPut(const wchar_t wch)
    {
        _trace.TraceOnAction(L"OscPut");

        _oscString.push_back(wch);
    }

    [[msvc::forceinline]]
    void _ActionOscDispatch(const wchar_t /*wch*/)
    {
        _trace.TraceOnAction(L"OscDispatch");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionOscDispatch(_oscParameter, _oscString);
        }));
    }

    [[msvc::forceinline]]
    void _ActionSs3Dispatch(const wchar_t wch)
    {
        _trace.TraceOnAction(L"Ss3Dispatch");
        _trace.DispatchSequenceTrace(_SafeExecute([=]() {
            return _engine.ActionSs3Dispatch(wch, { _parameters.data(), _parameters.size() });
        }));
    }

    [[msvc::forceinline]]
    void _ActionDcsDispatch(const wchar_t wch)
    {
        _trace.TraceOnAction(L"DcsDispatch");

        const auto success = _SafeExecute([=]() {
            _dcsStringHandler = _engine.ActionDcsDispatch(_identifier.Finalize(wch), { _parameters.data(), _parameters.size() });
            // If the returned handler is null, the sequence is not supported.
            return _dcsStringHandler != nullptr;
        });

        // Trace the result.
        _trace.DispatchSequenceTrace(success);

        if (success)
        {
            // If successful, enter the pass through state.
            this->EnterDcsPassThrough();
        }
        else
        {
            // Otherwise ignore remaining chars.
            this->EnterDcsIgnore();
        }
    }

    [[msvc::forceinline]]
    void _ActionClear()
    {
        _trace.TraceOnAction(L"Clear");

        // clear all internal stored state.
        _identifier.Clear();

        _parameters.clear();
        _parameterLimitOverflowed = false;

        _subParameters.clear();
        _subParameterRanges.clear();
        _subParameterCounter = 0;
        _subParameterLimitOverflowed = false;

        _oscString.clear();
        _oscParameter = 0;

        _dcsStringHandler = nullptr;

        _engine.ActionClear();
    }

    [[msvc::forceinline]]
    void _ActionIgnore() noexcept
    {
        // do nothing.
        _trace.TraceOnAction(L"Ignore");
    }

    [[msvc::forceinline]]
    void _ActionInterrupt()
    {
        // This is only applicable for DCS strings. OSC strings require a full
        // ST sequence to be received before they can be dispatched.
        if (this->_state == VTStates::DcsPassThrough)
        {
            // The ESC signals the end of the data string.
            _dcsStringHandler(0x1B);
            _dcsStringHandler = nullptr;
        }
    }

    [[msvc::forceinline]]
    void _ExecuteCsiCompleteCallback()
    {
        if (_onCsiCompleteCallback)
        {
            // We need to save the state of the string that we're currently
            // processing in case the callback injects another string.
            const auto savedCurrentString = _currentString;
            const auto savedRunOffset = _runOffset;
            const auto savedRunSize = _runSize;
            // We also need to take ownership of the callback function before
            // executing it so there's no risk of it being run more than once.
            const auto callback = std::move(_onCsiCompleteCallback);
            callback();
            // Once the callback has returned, we can restore the original state
            // and continue where we left off.
            _currentString = savedCurrentString;
            _runOffset = savedRunOffset;
            _runSize = savedRunSize;
        }
    }

    EngineT _engine;
    static constexpr bool _isEngineForInput = isEngineForInput;

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
