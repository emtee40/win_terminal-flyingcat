#pragma once

#include "../precomp.h"
#include <memory>
#include <string_view>
#include <functional>
#include <minwindef.h>

#include "../../adapter/termDispatch.hpp"
#include "../../parser/IStateMachineEngine.hpp"

#include "../../parser/ascii.hpp"
#include "../../parser/base64.hpp"
#include "../../parser/stateMachine.hpp"
#include "../../../types/inc/utils.hpp"
#include "../../../renderer/vt/vtrenderer.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::VirtualTerminal;

namespace Microsoft::Console::Render
{
    class VtEngine;
}

namespace v2
{

template<class TermDispatchT>
class OutputEngine : public IStateMachineEngine
{
public:
    static constexpr size_t MAX_URL_LENGTH = 2 * 1048576; // 2MB, like iTerm2

    OutputEngine(auto&&... args) :
        _dispatch(std::forward<decltype(args)>(args)...),
        _pfnFlushToTerminal(nullptr),
        _pTtyConnection(nullptr),
        _lastPrintedChar(AsciiChars::NUL)
    {
    }

    bool EncounteredWin32InputModeSequence() const noexcept
    {
        return false;
    }

    bool ActionExecute(const wchar_t wch)
    {
        switch (wch)
        {
        case AsciiChars::ENQ:
            // GH#11946: At some point we may want to add support for the VT
            // answerback feature, which requires responding to an ENQ control
            // with a user-defined reply, but until then we just ignore it.
            break;
        case AsciiChars::BEL:
            _dispatch.WarningBell();
            // microsoft/terminal#2952
            // If we're attached to a terminal, let's also pass the BEL through.
            if (_pfnFlushToTerminal != nullptr)
            {
                _pfnFlushToTerminal();
            }
            break;
        case AsciiChars::BS:
            _dispatch.CursorBackward(1);
            break;
        case AsciiChars::TAB:
            _dispatch.ForwardTab(1);
            break;
        case AsciiChars::CR:
            _dispatch.CarriageReturn();
            break;
        case AsciiChars::LF:
        case AsciiChars::FF:
        case AsciiChars::VT:
            // LF, FF, and VT are identical in function.
            _dispatch.LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
            break;
        case AsciiChars::SI:
            _dispatch.LockingShift(0);
            break;
        case AsciiChars::SO:
            _dispatch.LockingShift(1);
            break;
        case AsciiChars::SUB:
            // The SUB control is used to cancel a control sequence in the same
            // way as CAN, but unlike CAN it also displays an error character,
            // typically a reverse question mark (Unicode substitute form two).
            _dispatch.Print(L'\u2426');
            break;
        case AsciiChars::DEL:
            // The DEL control can sometimes be translated into a printable glyph
            // if a 96-character set is designated, so we need to pass it through
            // to the Print method. If not translated, it will be filtered out
            // there.
            _dispatch.Print(wch);
            break;
        default:
            // GH#1825, GH#10786: VT applications expect to be able to write other
            // control characters and have _nothing_ happen. We filter out these
            // characters here, so they don't fill the buffer.
            break;
        }

        _ClearLastChar();

        return true;
    }

    bool ActionExecuteFromEscape(const wchar_t wch)
    {
        return ActionExecute(wch);
    }

    #pragma region AutoGen ActionExecute
    [[msvc::forceinline]] bool ActionExecute_ENQ()
    {
        // GH#11946: At some point we may want to add support for the VT
        // answerback feature, which requires responding to an ENQ control
        // with a user-defined reply, but until then we just ignore it.
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_BEL()
    {
        _dispatch.WarningBell();
        // microsoft/terminal#2952
        // If we're attached to a terminal, let's also pass the BEL through.
        if (_pfnFlushToTerminal != nullptr)
        {
            _pfnFlushToTerminal();
        }
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_BS()
    {
        _dispatch.CursorBackward(1);
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_TAB()
    {
        _dispatch.ForwardTab(1);
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_CR()
    {
        _dispatch.CarriageReturn();
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_LF_FF_VT()
    {
        // LF, FF, and VT are identical in function.
        _dispatch.LineFeed(DispatchTypes::LineFeedType::DependsOnMode);
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_SI()
    {
        _dispatch.LockingShift(0);
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_SO()
    {
        _dispatch.LockingShift(1);
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_SUB()
    {
        // The SUB control is used to cancel a control sequence in the same
        // way as CAN, but unlike CAN it also displays an error character,
        // typically a reverse question mark (Unicode substitute form two).
        _dispatch.Print(L'\u2426');
        return true;
    }

    [[msvc::forceinline]] bool ActionExecute_DEL()
    {
        // The DEL control can sometimes be translated into a printable glyph
        // if a 96-character set is designated, so we need to pass it through
        // to the Print method. If not translated, it will be filtered out
        // there.
        _dispatch.Print(AsciiChars::DEL);
        return true;
    }
    #pragma endregion

    [[msvc::forceinline]] bool ActionUnmatchedExecute()
    {
        // GH#1825, GH#10786: VT applications expect to be able to write other
        // control characters and have _nothing_ happen. We filter out these
        // characters here, so they don't fill the buffer.
        _ClearLastChar();
        return true;
    }

    bool ActionPrint(const wchar_t wch)
    {
        // Stash the last character of the string, if it's a graphical character
        if (wch >= AsciiChars::SPC)
        {
            _lastPrintedChar = wch;
        }

        _dispatch.Print(wch); // call print

        return true;
    }

    bool ActionPrintString(const std::wstring_view string)
    {
        if (string.empty())
        {
            return true;
        }

        // Stash the last character of the string, if it's a graphical character
        const auto wch = string.back();
        if (wch >= AsciiChars::SPC)
        {
            _lastPrintedChar = wch;
        }

        _dispatch.PrintString(string); // call print

        return true;
    }

    bool ActionPassThroughString(const std::wstring_view string, const bool flush)
    {
        auto success = true;
        if (_pTtyConnection != nullptr)
        {
            const auto hr = _pTtyConnection->WriteTerminalW(string, flush);
            LOG_IF_FAILED(hr);
            success = SUCCEEDED(hr);
        }
        // If there's not a TTY connection, our previous behavior was to eat the string.

        return success;
    }

    bool ActionEscDispatch(const VTID id)
    {
        auto success = false;

        switch (id)
        {
        case EscActionCodes::ST_StringTerminator:
            // This is the 7-bit string terminator, which is essentially a no-op.
            success = true;
            break;
        case EscActionCodes::DECBI_BackIndex:
            success = _dispatch.BackIndex();
            break;
        case EscActionCodes::DECSC_CursorSave:
            success = _dispatch.CursorSaveState();
            break;
        case EscActionCodes::DECRC_CursorRestore:
            success = _dispatch.CursorRestoreState();
            break;
        case EscActionCodes::DECFI_ForwardIndex:
            success = _dispatch.ForwardIndex();
            break;
        case EscActionCodes::DECKPAM_KeypadApplicationMode:
            success = _dispatch.SetKeypadMode(true);
            break;
        case EscActionCodes::DECKPNM_KeypadNumericMode:
            success = _dispatch.SetKeypadMode(false);
            break;
        case EscActionCodes::NEL_NextLine:
            success = _dispatch.LineFeed(DispatchTypes::LineFeedType::WithReturn);
            break;
        case EscActionCodes::IND_Index:
            success = _dispatch.LineFeed(DispatchTypes::LineFeedType::WithoutReturn);
            break;
        case EscActionCodes::RI_ReverseLineFeed:
            success = _dispatch.ReverseLineFeed();
            break;
        case EscActionCodes::HTS_HorizontalTabSet:
            success = _dispatch.HorizontalTabSet();
            break;
        case EscActionCodes::DECID_IdentifyDevice:
            success = _dispatch.DeviceAttributes();
            break;
        case EscActionCodes::RIS_ResetToInitialState:
            success = _dispatch.HardReset();
            break;
        case EscActionCodes::SS2_SingleShift:
            success = _dispatch.SingleShift(2);
            break;
        case EscActionCodes::SS3_SingleShift:
            success = _dispatch.SingleShift(3);
            break;
        case EscActionCodes::LS2_LockingShift:
            success = _dispatch.LockingShift(2);
            break;
        case EscActionCodes::LS3_LockingShift:
            success = _dispatch.LockingShift(3);
            break;
        case EscActionCodes::LS1R_LockingShift:
            success = _dispatch.LockingShiftRight(1);
            break;
        case EscActionCodes::LS2R_LockingShift:
            success = _dispatch.LockingShiftRight(2);
            break;
        case EscActionCodes::LS3R_LockingShift:
            success = _dispatch.LockingShiftRight(3);
            break;
        case EscActionCodes::DECAC1_AcceptC1Controls:
            success = _dispatch.AcceptC1Controls(true);
            break;
        case EscActionCodes::ACS_AnsiLevel1:
            success = _dispatch.AnnounceCodeStructure(1);
            break;
        case EscActionCodes::ACS_AnsiLevel2:
            success = _dispatch.AnnounceCodeStructure(2);
            break;
        case EscActionCodes::ACS_AnsiLevel3:
            success = _dispatch.AnnounceCodeStructure(3);
            break;
        case EscActionCodes::DECDHL_DoubleHeightLineTop:
            success = _dispatch.SetLineRendition(LineRendition::DoubleHeightTop);
            break;
        case EscActionCodes::DECDHL_DoubleHeightLineBottom:
            success = _dispatch.SetLineRendition(LineRendition::DoubleHeightBottom);
            break;
        case EscActionCodes::DECSWL_SingleWidthLine:
            success = _dispatch.SetLineRendition(LineRendition::SingleWidth);
            break;
        case EscActionCodes::DECDWL_DoubleWidthLine:
            success = _dispatch.SetLineRendition(LineRendition::DoubleWidth);
            break;
        case EscActionCodes::DECALN_ScreenAlignmentPattern:
            success = _dispatch.ScreenAlignmentPattern();
            break;
        default:
            const auto commandChar = id[0];
            const auto commandParameter = id.SubSequence(1);
            switch (commandChar)
            {
            case '%':
                success = _dispatch.DesignateCodingSystem(commandParameter);
                break;
            case '(':
                success = _dispatch.Designate94Charset(0, commandParameter);
                break;
            case ')':
                success = _dispatch.Designate94Charset(1, commandParameter);
                break;
            case '*':
                success = _dispatch.Designate94Charset(2, commandParameter);
                break;
            case '+':
                success = _dispatch.Designate94Charset(3, commandParameter);
                break;
            case '-':
                success = _dispatch.Designate96Charset(1, commandParameter);
                break;
            case '.':
                success = _dispatch.Designate96Charset(2, commandParameter);
                break;
            case '/':
                success = _dispatch.Designate96Charset(3, commandParameter);
                break;
            default:
                // If no functions to call, overall dispatch was a failure.
                success = false;
                break;
            }
        }

        // If we were unable to process the string, and there's a TTY attached to us,
        //      trigger the state machine to flush the string to the terminal.
        if (_pfnFlushToTerminal != nullptr && !success)
        {
            success = _pfnFlushToTerminal();
        }

        _ClearLastChar();

        return success;
    }

    bool ActionVt52EscDispatch(const VTID id, const VTParameters parameters)
    {
        auto success = false;

        switch (id)
        {
        case Vt52ActionCodes::CursorUp:
            success = _dispatch.CursorUp(1);
            break;
        case Vt52ActionCodes::CursorDown:
            success = _dispatch.CursorDown(1);
            break;
        case Vt52ActionCodes::CursorRight:
            success = _dispatch.CursorForward(1);
            break;
        case Vt52ActionCodes::CursorLeft:
            success = _dispatch.CursorBackward(1);
            break;
        case Vt52ActionCodes::EnterGraphicsMode:
            success = _dispatch.Designate94Charset(0, DispatchTypes::CharacterSets::DecSpecialGraphics);
            break;
        case Vt52ActionCodes::ExitGraphicsMode:
            success = _dispatch.Designate94Charset(0, DispatchTypes::CharacterSets::ASCII);
            break;
        case Vt52ActionCodes::CursorToHome:
            success = _dispatch.CursorPosition(1, 1);
            break;
        case Vt52ActionCodes::ReverseLineFeed:
            success = _dispatch.ReverseLineFeed();
            break;
        case Vt52ActionCodes::EraseToEndOfScreen:
            success = _dispatch.EraseInDisplay(DispatchTypes::EraseType::ToEnd);
            break;
        case Vt52ActionCodes::EraseToEndOfLine:
            success = _dispatch.EraseInLine(DispatchTypes::EraseType::ToEnd);
            break;
        case Vt52ActionCodes::DirectCursorAddress:
            // VT52 cursor addresses are provided as ASCII characters, with
            // the lowest value being a space, representing an address of 1.
            success = _dispatch.CursorPosition(parameters.at(0).value() - ' ' + 1, parameters.at(1).value() - ' ' + 1);
            break;
        case Vt52ActionCodes::Identify:
            success = _dispatch.Vt52DeviceAttributes();
            break;
        case Vt52ActionCodes::EnterAlternateKeypadMode:
            success = _dispatch.SetKeypadMode(true);
            break;
        case Vt52ActionCodes::ExitAlternateKeypadMode:
            success = _dispatch.SetKeypadMode(false);
            break;
        case Vt52ActionCodes::ExitVt52Mode:
            success = _dispatch.SetMode(DispatchTypes::ModeParams::DECANM_AnsiMode);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            success = false;
            break;
        }

        _ClearLastChar();

        return success;
    }

    bool _PostCsiDispatch(bool success)
    {
        // If we were unable to process the string, and there's a TTY attached to us,
        //      trigger the state machine to flush the string to the terminal.
        if (_pfnFlushToTerminal != nullptr && !success)
        {
            success = _pfnFlushToTerminal();
        }

        _ClearLastChar();

        return success;
    }

    bool ActionCsiDispatch(const VTID id, const VTParameters parameters)
    {
        // Bail out if we receive subparameters, but we don't accept them in the sequence.
        if (parameters.hasSubParams() && !_CanSeqAcceptSubParam(id, parameters)) [[unlikely]]
        {
            return false;
        }

        auto success = false;

        switch (id)
        {
        case CsiActionCodes::CUU_CursorUp:
            success = _dispatch.CursorUp(parameters.at(0));
            break;
        case CsiActionCodes::CUD_CursorDown:
            success = _dispatch.CursorDown(parameters.at(0));
            break;
        case CsiActionCodes::CUF_CursorForward:
            success = _dispatch.CursorForward(parameters.at(0));
            break;
        case CsiActionCodes::CUB_CursorBackward:
            success = _dispatch.CursorBackward(parameters.at(0));
            break;
        case CsiActionCodes::CNL_CursorNextLine:
            success = _dispatch.CursorNextLine(parameters.at(0));
            break;
        case CsiActionCodes::CPL_CursorPrevLine:
            success = _dispatch.CursorPrevLine(parameters.at(0));
            break;
        case CsiActionCodes::CHA_CursorHorizontalAbsolute:
        case CsiActionCodes::HPA_HorizontalPositionAbsolute:
            success = _dispatch.CursorHorizontalPositionAbsolute(parameters.at(0));
            break;
        case CsiActionCodes::VPA_VerticalLinePositionAbsolute:
            success = _dispatch.VerticalLinePositionAbsolute(parameters.at(0));
            break;
        case CsiActionCodes::HPR_HorizontalPositionRelative:
            success = _dispatch.HorizontalPositionRelative(parameters.at(0));
            break;
        case CsiActionCodes::VPR_VerticalPositionRelative:
            success = _dispatch.VerticalPositionRelative(parameters.at(0));
            break;
        case CsiActionCodes::CUP_CursorPosition:
        case CsiActionCodes::HVP_HorizontalVerticalPosition:
            success = _dispatch.CursorPosition(parameters.at(0), parameters.at(1));
            break;
        case CsiActionCodes::DECSTBM_SetTopBottomMargins:
            success = _dispatch.SetTopBottomScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
            break;
        case CsiActionCodes::DECSLRM_SetLeftRightMargins:
            // Note that this can also be ANSISYSSC, depending on the state of DECLRMM.
            success = _dispatch.SetLeftRightScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
            break;
        case CsiActionCodes::ICH_InsertCharacter:
            success = _dispatch.InsertCharacter(parameters.at(0));
            break;
        case CsiActionCodes::DCH_DeleteCharacter:
            success = _dispatch.DeleteCharacter(parameters.at(0));
            break;
        case CsiActionCodes::ED_EraseDisplay:
            success = parameters.for_each([&](const auto eraseType) {
                return _dispatch.EraseInDisplay(eraseType);
            });
            break;
        case CsiActionCodes::DECSED_SelectiveEraseDisplay:
            success = parameters.for_each([&](const auto eraseType) {
                return _dispatch.SelectiveEraseInDisplay(eraseType);
            });
            break;
        case CsiActionCodes::EL_EraseLine:
            success = parameters.for_each([&](const auto eraseType) {
                return _dispatch.EraseInLine(eraseType);
            });
            break;
        case CsiActionCodes::DECSEL_SelectiveEraseLine:
            success = parameters.for_each([&](const auto eraseType) {
                return _dispatch.SelectiveEraseInLine(eraseType);
            });
            break;
        case CsiActionCodes::SM_SetMode:
            success = parameters.for_each([&](const auto mode) {
                return _dispatch.SetMode(DispatchTypes::ANSIStandardMode(mode));
            });
            break;
        case CsiActionCodes::DECSET_PrivateModeSet:
            success = parameters.for_each([&](const auto mode) {
                return _dispatch.SetMode(DispatchTypes::DECPrivateMode(mode));
            });
            break;
        case CsiActionCodes::RM_ResetMode:
            success = parameters.for_each([&](const auto mode) {
                return _dispatch.ResetMode(DispatchTypes::ANSIStandardMode(mode));
            });
            break;
        case CsiActionCodes::DECRST_PrivateModeReset:
            success = parameters.for_each([&](const auto mode) {
                return _dispatch.ResetMode(DispatchTypes::DECPrivateMode(mode));
            });
            break;
        case CsiActionCodes::SGR_SetGraphicsRendition:
            success = _dispatch.SetGraphicsRendition(parameters);
            break;
        case CsiActionCodes::DSR_DeviceStatusReport:
            success = _dispatch.DeviceStatusReport(DispatchTypes::ANSIStandardStatus(parameters.at(0)), parameters.at(1));
            break;
        case CsiActionCodes::DSR_PrivateDeviceStatusReport:
            success = _dispatch.DeviceStatusReport(DispatchTypes::DECPrivateStatus(parameters.at(0)), parameters.at(1));
            break;
        case CsiActionCodes::DA_DeviceAttributes:
            success = parameters.at(0).value_or(0) == 0 && _dispatch.DeviceAttributes();
            break;
        case CsiActionCodes::DA2_SecondaryDeviceAttributes:
            success = parameters.at(0).value_or(0) == 0 && _dispatch.SecondaryDeviceAttributes();
            break;
        case CsiActionCodes::DA3_TertiaryDeviceAttributes:
            success = parameters.at(0).value_or(0) == 0 && _dispatch.TertiaryDeviceAttributes();
            break;
        case CsiActionCodes::DECREQTPARM_RequestTerminalParameters:
            success = _dispatch.RequestTerminalParameters(parameters.at(0));
            break;
        case CsiActionCodes::SU_ScrollUp:
            success = _dispatch.ScrollUp(parameters.at(0));
            break;
        case CsiActionCodes::SD_ScrollDown:
            success = _dispatch.ScrollDown(parameters.at(0));
            break;
        case CsiActionCodes::NP_NextPage:
            success = _dispatch.NextPage(parameters.at(0));
            break;
        case CsiActionCodes::PP_PrecedingPage:
            success = _dispatch.PrecedingPage(parameters.at(0));
            break;
        case CsiActionCodes::ANSISYSRC_CursorRestore:
            success = _dispatch.CursorRestoreState();
            break;
        case CsiActionCodes::IL_InsertLine:
            success = _dispatch.InsertLine(parameters.at(0));
            break;
        case CsiActionCodes::DL_DeleteLine:
            success = _dispatch.DeleteLine(parameters.at(0));
            break;
        case CsiActionCodes::CHT_CursorForwardTab:
            success = _dispatch.ForwardTab(parameters.at(0));
            break;
        case CsiActionCodes::CBT_CursorBackTab:
            success = _dispatch.BackwardsTab(parameters.at(0));
            break;
        case CsiActionCodes::TBC_TabClear:
            success = parameters.for_each([&](const auto clearType) {
                return _dispatch.TabClear(clearType);
            });
            break;
        case CsiActionCodes::DECST8C_SetTabEvery8Columns:
            success = parameters.for_each([&](const auto setType) {
                return _dispatch.TabSet(setType);
            });
            break;
        case CsiActionCodes::ECH_EraseCharacters:
            success = _dispatch.EraseCharacters(parameters.at(0));
            break;
        case CsiActionCodes::DTTERM_WindowManipulation:
            success = _dispatch.WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
            break;
        case CsiActionCodes::REP_RepeatCharacter:
            // Handled w/o the dispatch. This function is unique in that way
            // If this were in the ITerminalDispatch, then each
            // implementation would effectively be the same, calling only
            // functions that are already part of the interface.
            // Print the last graphical character a number of times.
            if (_lastPrintedChar != AsciiChars::NUL)
            {
                const size_t repeatCount = parameters.at(0);
                std::wstring wstr(repeatCount, _lastPrintedChar);
                _dispatch.PrintString(wstr);
            }
            success = true;
            break;
        case CsiActionCodes::PPA_PagePositionAbsolute:
            success = _dispatch.PagePositionAbsolute(parameters.at(0));
            break;
        case CsiActionCodes::PPR_PagePositionRelative:
            success = _dispatch.PagePositionRelative(parameters.at(0));
            break;
        case CsiActionCodes::PPB_PagePositionBack:
            success = _dispatch.PagePositionBack(parameters.at(0));
            break;
        case CsiActionCodes::DECSCUSR_SetCursorStyle:
            success = _dispatch.SetCursorStyle(parameters.at(0));
            break;
        case CsiActionCodes::DECSTR_SoftReset:
            success = _dispatch.SoftReset();
            break;
        case CsiActionCodes::DECSCA_SetCharacterProtectionAttribute:
            success = _dispatch.SetCharacterProtectionAttribute(parameters);
            break;
        case CsiActionCodes::DECRQDE_RequestDisplayedExtent:
            success = _dispatch.RequestDisplayedExtent();
            break;
        case CsiActionCodes::XT_PushSgr:
        case CsiActionCodes::XT_PushSgrAlias:
            success = _dispatch.PushGraphicsRendition(parameters);
            break;
        case CsiActionCodes::XT_PopSgr:
        case CsiActionCodes::XT_PopSgrAlias:
            success = _dispatch.PopGraphicsRendition();
            break;
        case CsiActionCodes::DECRQM_RequestMode:
            success = _dispatch.RequestMode(DispatchTypes::ANSIStandardMode(parameters.at(0)));
            break;
        case CsiActionCodes::DECRQM_PrivateRequestMode:
            success = _dispatch.RequestMode(DispatchTypes::DECPrivateMode(parameters.at(0)));
            break;
        case CsiActionCodes::DECCARA_ChangeAttributesRectangularArea:
            success = _dispatch.ChangeAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
            break;
        case CsiActionCodes::DECRARA_ReverseAttributesRectangularArea:
            success = _dispatch.ReverseAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
            break;
        case CsiActionCodes::DECCRA_CopyRectangularArea:
            success = _dispatch.CopyRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.at(4), parameters.at(5), parameters.at(6), parameters.at(7));
            break;
        case CsiActionCodes::DECRQPSR_RequestPresentationStateReport:
            success = _dispatch.RequestPresentationStateReport(parameters.at(0));
            break;
        case CsiActionCodes::DECFRA_FillRectangularArea:
            success = _dispatch.FillRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2), parameters.at(3).value_or(0), parameters.at(4).value_or(0));
            break;
        case CsiActionCodes::DECERA_EraseRectangularArea:
            success = _dispatch.EraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
            break;
        case CsiActionCodes::DECSERA_SelectiveEraseRectangularArea:
            success = _dispatch.SelectiveEraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
            break;
        case CsiActionCodes::DECRQUPSS_RequestUserPreferenceSupplementalSet:
            success = _dispatch.RequestUserPreferenceCharset();
            break;
        case CsiActionCodes::DECIC_InsertColumn:
            success = _dispatch.InsertColumn(parameters.at(0));
            break;
        case CsiActionCodes::DECDC_DeleteColumn:
            success = _dispatch.DeleteColumn(parameters.at(0));
            break;
        case CsiActionCodes::DECSACE_SelectAttributeChangeExtent:
            success = _dispatch.SelectAttributeChangeExtent(parameters.at(0));
            break;
        case CsiActionCodes::DECRQCRA_RequestChecksumRectangularArea:
            success = _dispatch.RequestChecksumRectangularArea(parameters.at(0).value_or(0), parameters.at(1).value_or(0), parameters.at(2), parameters.at(3), parameters.at(4).value_or(0), parameters.at(5).value_or(0));
            break;
        case CsiActionCodes::DECINVM_InvokeMacro:
            success = _dispatch.InvokeMacro(parameters.at(0).value_or(0));
            break;
        case CsiActionCodes::DECAC_AssignColor:
            success = _dispatch.AssignColor(parameters.at(0), parameters.at(1).value_or(0), parameters.at(2).value_or(0));
            break;
        case CsiActionCodes::DECPS_PlaySound:
            success = _dispatch.PlaySounds(parameters);
            break;
        default:
            // If no functions to call, overall dispatch was a failure.
            success = false;
            break;
        }

        // If we were unable to process the string, and there's a TTY attached to us,
        //      trigger the state machine to flush the string to the terminal.
        if (_pfnFlushToTerminal != nullptr && !success)
        {
            success = _pfnFlushToTerminal();
        }

        _ClearLastChar();

        return success;
    }

    StringHandler ActionDcsDispatch(const VTID id, const VTParameters parameters)
    {
        StringHandler handler = nullptr;

        switch (id)
        {
        case DcsActionCodes::DECDLD_DownloadDRCS:
            handler = _dispatch.DownloadDRCS(parameters.at(0),
                                              parameters.at(1),
                                              parameters.at(2),
                                              parameters.at(3),
                                              parameters.at(4),
                                              parameters.at(5),
                                              parameters.at(6),
                                              parameters.at(7));
            break;
        case DcsActionCodes::DECAUPSS_AssignUserPreferenceSupplementalSet:
            handler = _dispatch.AssignUserPreferenceCharset(parameters.at(0));
            break;
        case DcsActionCodes::DECDMAC_DefineMacro:
            handler = _dispatch.DefineMacro(parameters.at(0).value_or(0), parameters.at(1), parameters.at(2));
            break;
        case DcsActionCodes::DECRSTS_RestoreTerminalState:
            handler = _dispatch.RestoreTerminalState(parameters.at(0));
            break;
        case DcsActionCodes::DECRQSS_RequestSetting:
            handler = _dispatch.RequestSetting();
            break;
        case DcsActionCodes::DECRSPS_RestorePresentationState:
            handler = _dispatch.RestorePresentationState(parameters.at(0));
            break;
        default:
            handler = nullptr;
            break;
        }

        _ClearLastChar();

        return handler;
    }

    bool ActionClear() noexcept
    {
        // do nothing.
        return true;
    }

    bool ActionIgnore() noexcept
    {
        // do nothing.
        return true;
    }

    bool ActionOscDispatch(const size_t parameter, const std::wstring_view string)
    {
        auto success = false;

        switch (parameter)
        {
        case OscActionCodes::SetIconAndWindowTitle:
        case OscActionCodes::SetWindowIcon:
        case OscActionCodes::SetWindowTitle:
        case OscActionCodes::DECSWT_SetWindowTitle:
        {
            success = _dispatch.SetWindowTitle(string);
            break;
        }
        case OscActionCodes::SetColor:
        {
            std::vector<size_t> tableIndexes;
            std::vector<DWORD> colors;
            success = _GetOscSetColorTable(string, tableIndexes, colors);
            for (size_t i = 0; i < tableIndexes.size(); i++)
            {
                const auto tableIndex = til::at(tableIndexes, i);
                const auto rgb = til::at(colors, i);
                success = success && _dispatch.SetColorTableEntry(tableIndex, rgb);
            }
            break;
        }
        case OscActionCodes::SetForegroundColor:
        case OscActionCodes::SetBackgroundColor:
        case OscActionCodes::SetCursorColor:
        {
            std::vector<DWORD> colors;
            success = _GetOscSetColor(string, colors);
            if (success)
            {
                auto commandIndex = parameter;
                size_t colorIndex = 0;

                if (commandIndex == OscActionCodes::SetForegroundColor && colors.size() > colorIndex)
                {
                    const auto color = til::at(colors, colorIndex);
                    if (color != INVALID_COLOR)
                    {
                        success = success && _dispatch.SetDefaultForeground(color);
                    }
                    commandIndex++;
                    colorIndex++;
                }

                if (commandIndex == OscActionCodes::SetBackgroundColor && colors.size() > colorIndex)
                {
                    const auto color = til::at(colors, colorIndex);
                    if (color != INVALID_COLOR)
                    {
                        success = success && _dispatch.SetDefaultBackground(color);
                    }
                    commandIndex++;
                    colorIndex++;
                }

                if (commandIndex == OscActionCodes::SetCursorColor && colors.size() > colorIndex)
                {
                    const auto color = til::at(colors, colorIndex);
                    if (color != INVALID_COLOR)
                    {
                        success = success && _dispatch.SetCursorColor(color);
                    }
                    commandIndex++;
                    colorIndex++;
                }
            }
            break;
        }
        case OscActionCodes::SetClipboard:
        {
            std::wstring setClipboardContent;
            auto queryClipboard = false;
            success = _GetOscSetClipboard(string, setClipboardContent, queryClipboard);
            if (success && !queryClipboard)
            {
                success = _dispatch.SetClipboard(setClipboardContent);
            }
            break;
        }
        case OscActionCodes::ResetCursorColor:
        {
            success = _dispatch.SetCursorColor(INVALID_COLOR);
            break;
        }
        case OscActionCodes::Hyperlink:
        {
            std::wstring params;
            std::wstring uri;
            success = _ParseHyperlink(string, params, uri);
            if (uri.empty())
            {
                success = success && _dispatch.EndHyperlink();
            }
            else
            {
                success = success && _dispatch.AddHyperlink(uri, params);
            }
            break;
        }
        case OscActionCodes::ConEmuAction:
        {
            success = _dispatch.DoConEmuAction(string);
            break;
        }
        case OscActionCodes::ITerm2Action:
        {
            success = _dispatch.DoITerm2Action(string);
            break;
        }
        case OscActionCodes::FinalTermAction:
        {
            success = _dispatch.DoFinalTermAction(string);
            break;
        }
        case OscActionCodes::VsCodeAction:
        {
            success = _dispatch.DoVsCodeAction(string);
            break;
        }
        default:
            // If no functions to call, overall dispatch was a failure.
            success = false;
            break;
        }

        // If we were unable to process the string, and there's a TTY attached to us,
        //      trigger the state machine to flush the string to the terminal.
        if (_pfnFlushToTerminal != nullptr && !success)
        {
            success = _pfnFlushToTerminal();
        }

        _ClearLastChar();

        return success;
    }

    bool ActionSs3Dispatch(const wchar_t /*wch*/, const VTParameters /*parameters*/) noexcept
    {
        // The output engine doesn't handle any SS3 sequences.
        _ClearLastChar();
        return false;
    }

    void SetTerminalConnection(Microsoft::Console::Render::VtEngine* const pTtyConnection,
                               std::function<bool()> pfnFlushToTerminal)
    {
        this->_pTtyConnection = pTtyConnection;
        this->_pfnFlushToTerminal = pfnFlushToTerminal;
    }

    const ITermDispatch& Dispatch() const noexcept { return _dispatch; }
    ITermDispatch& Dispatch() noexcept { return _dispatch; }

    #pragma region AutoGen Csi
    [[msvc::forceinline]] bool Csi_CUU_CursorUp(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorUp(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CUD_CursorDown(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorDown(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CUF_CursorForward(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorForward(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CUB_CursorBackward(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorBackward(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CNL_CursorNextLine(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorNextLine(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CPL_CursorPrevLine(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorPrevLine(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CHA_CursorHorizontalAbsolute_HPA_HorizontalPositionAbsolute(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorHorizontalPositionAbsolute(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_VPA_VerticalLinePositionAbsolute(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.VerticalLinePositionAbsolute(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_HPR_HorizontalPositionRelative(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.HorizontalPositionRelative(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_VPR_VerticalPositionRelative(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.VerticalPositionRelative(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CUP_CursorPosition_HVP_HorizontalVerticalPosition(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CursorPosition(parameters.at(0), parameters.at(1));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSTBM_SetTopBottomMargins(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SetTopBottomScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSLRM_SetLeftRightMargins(const VTParameters parameters)
    {
        bool success;
        // Note that this can also be ANSISYSSC, depending on the state of DECLRMM.
        success = _dispatch.SetLeftRightScrollingMargins(parameters.at(0).value_or(0), parameters.at(1).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_ICH_InsertCharacter(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.InsertCharacter(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DCH_DeleteCharacter(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.DeleteCharacter(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_ED_EraseDisplay(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch.EraseInDisplay(eraseType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSED_SelectiveEraseDisplay(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch.SelectiveEraseInDisplay(eraseType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_EL_EraseLine(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch.EraseInLine(eraseType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSEL_SelectiveEraseLine(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto eraseType) {
            return _dispatch.SelectiveEraseInLine(eraseType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_SM_SetMode(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto mode) {
            return _dispatch.SetMode(DispatchTypes::ANSIStandardMode(mode));
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSET_PrivateModeSet(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto mode) {
            return _dispatch.SetMode(DispatchTypes::DECPrivateMode(mode));
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_RM_ResetMode(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto mode) {
            return _dispatch.ResetMode(DispatchTypes::ANSIStandardMode(mode));
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRST_PrivateModeReset(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto mode) {
            return _dispatch.ResetMode(DispatchTypes::DECPrivateMode(mode));
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_SGR_SetGraphicsRendition(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SetGraphicsRendition(parameters);
        return success;
    }

    [[msvc::forceinline]] bool Csi_DSR_DeviceStatusReport(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.DeviceStatusReport(DispatchTypes::ANSIStandardStatus(parameters.at(0)), parameters.at(1));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DSR_PrivateDeviceStatusReport(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.DeviceStatusReport(DispatchTypes::DECPrivateStatus(parameters.at(0)), parameters.at(1));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DA_DeviceAttributes(const VTParameters parameters)
    {
        bool success;
        success = parameters.at(0).value_or(0) == 0 && _dispatch.DeviceAttributes();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DA2_SecondaryDeviceAttributes(const VTParameters parameters)
    {
        bool success;
        success = parameters.at(0).value_or(0) == 0 && _dispatch.SecondaryDeviceAttributes();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DA3_TertiaryDeviceAttributes(const VTParameters parameters)
    {
        bool success;
        success = parameters.at(0).value_or(0) == 0 && _dispatch.TertiaryDeviceAttributes();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECREQTPARM_RequestTerminalParameters(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.RequestTerminalParameters(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_SU_ScrollUp(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.ScrollUp(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_SD_ScrollDown(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.ScrollDown(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_NP_NextPage(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.NextPage(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_PP_PrecedingPage(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PrecedingPage(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_ANSISYSRC_CursorRestore(const VTParameters /*parameters*/)
    {
        bool success;
        success = _dispatch.CursorRestoreState();
        return success;
    }

    [[msvc::forceinline]] bool Csi_IL_InsertLine(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.InsertLine(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DL_DeleteLine(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.DeleteLine(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CHT_CursorForwardTab(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.ForwardTab(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_CBT_CursorBackTab(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.BackwardsTab(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_TBC_TabClear(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto clearType) {
            return _dispatch.TabClear(clearType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECST8C_SetTabEvery8Columns(const VTParameters parameters)
    {
        bool success;
        success = parameters.for_each([&](const auto setType) {
            return _dispatch.TabSet(setType);
        });
        return success;
    }

    [[msvc::forceinline]] bool Csi_ECH_EraseCharacters(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.EraseCharacters(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DTTERM_WindowManipulation(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.WindowManipulation(parameters.at(0), parameters.at(1), parameters.at(2));
        return success;
    }

    [[msvc::forceinline]] bool Csi_REP_RepeatCharacter(const VTParameters parameters)
    {
        bool success;
        // Handled w/o the dispatch. This function is unique in that way
        // If this were in the ITerminalDispatch, then each
        // implementation would effectively be the same, calling only
        // functions that are already part of the interface.
        // Print the last graphical character a number of times.
        if (_lastPrintedChar != AsciiChars::NUL)
        {
            const size_t repeatCount = parameters.at(0);
            std::wstring wstr(repeatCount, _lastPrintedChar);
            _dispatch.PrintString(wstr);
        }
        success = true;
        return success;
    }

    [[msvc::forceinline]] bool Csi_PPA_PagePositionAbsolute(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PagePositionAbsolute(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_PPR_PagePositionRelative(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PagePositionRelative(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_PPB_PagePositionBack(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PagePositionBack(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSCUSR_SetCursorStyle(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SetCursorStyle(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSTR_SoftReset(const VTParameters /*parameters*/)
    {
        bool success;
        success = _dispatch.SoftReset();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSCA_SetCharacterProtectionAttribute(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SetCharacterProtectionAttribute(parameters);
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQDE_RequestDisplayedExtent(const VTParameters /*parameters*/)
    {
        bool success;
        success = _dispatch.RequestDisplayedExtent();
        return success;
    }

    [[msvc::forceinline]] bool Csi_XT_PushSgr_XT_PushSgrAlias(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PushGraphicsRendition(parameters);
        return success;
    }

    [[msvc::forceinline]] bool Csi_XT_PopSgr_XT_PopSgrAlias(const VTParameters /*parameters*/)
    {
        bool success;
        success = _dispatch.PopGraphicsRendition();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQM_RequestMode(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.RequestMode(DispatchTypes::ANSIStandardMode(parameters.at(0)));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQM_PrivateRequestMode(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.RequestMode(DispatchTypes::DECPrivateMode(parameters.at(0)));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECCARA_ChangeAttributesRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.ChangeAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRARA_ReverseAttributesRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.ReverseAttributesRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.subspan(4));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECCRA_CopyRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.CopyRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0), parameters.at(4), parameters.at(5), parameters.at(6), parameters.at(7));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQPSR_RequestPresentationStateReport(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.RequestPresentationStateReport(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECFRA_FillRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.FillRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2), parameters.at(3).value_or(0), parameters.at(4).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECERA_EraseRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.EraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSERA_SelectiveEraseRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SelectiveEraseRectangularArea(parameters.at(0), parameters.at(1), parameters.at(2).value_or(0), parameters.at(3).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQUPSS_RequestUserPreferenceSupplementalSet(const VTParameters /*parameters*/)
    {
        bool success;
        success = _dispatch.RequestUserPreferenceCharset();
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECIC_InsertColumn(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.InsertColumn(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECDC_DeleteColumn(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.DeleteColumn(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECSACE_SelectAttributeChangeExtent(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.SelectAttributeChangeExtent(parameters.at(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECRQCRA_RequestChecksumRectangularArea(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.RequestChecksumRectangularArea(parameters.at(0).value_or(0), parameters.at(1).value_or(0), parameters.at(2), parameters.at(3), parameters.at(4).value_or(0), parameters.at(5).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECINVM_InvokeMacro(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.InvokeMacro(parameters.at(0).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECAC_AssignColor(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.AssignColor(parameters.at(0), parameters.at(1).value_or(0), parameters.at(2).value_or(0));
        return success;
    }

    [[msvc::forceinline]] bool Csi_DECPS_PlaySound(const VTParameters parameters)
    {
        bool success;
        success = _dispatch.PlaySounds(parameters);
        return success;
    }
    #pragma endregion

private:
    TermDispatchT _dispatch;
    Microsoft::Console::Render::VtEngine* _pTtyConnection;
    std::function<bool()> _pfnFlushToTerminal;
    wchar_t _lastPrintedChar;

    
        enum EscActionCodes : uint64_t
    {
        DECBI_BackIndex = VTID("6"),
        DECSC_CursorSave = VTID("7"),
        DECRC_CursorRestore = VTID("8"),
        DECFI_ForwardIndex = VTID("9"),
        DECKPAM_KeypadApplicationMode = VTID("="),
        DECKPNM_KeypadNumericMode = VTID(">"),
        IND_Index = VTID("D"),
        NEL_NextLine = VTID("E"),
        HTS_HorizontalTabSet = VTID("H"),
        RI_ReverseLineFeed = VTID("M"),
        SS2_SingleShift = VTID("N"),
        SS3_SingleShift = VTID("O"),
        DECID_IdentifyDevice = VTID("Z"),
        ST_StringTerminator = VTID("\\"),
        RIS_ResetToInitialState = VTID("c"),
        LS2_LockingShift = VTID("n"),
        LS3_LockingShift = VTID("o"),
        LS1R_LockingShift = VTID("~"),
        LS2R_LockingShift = VTID("}"),
        LS3R_LockingShift = VTID("|"),
        DECAC1_AcceptC1Controls = VTID(" 7"),
        ACS_AnsiLevel1 = VTID(" L"),
        ACS_AnsiLevel2 = VTID(" M"),
        ACS_AnsiLevel3 = VTID(" N"),
        DECDHL_DoubleHeightLineTop = VTID("#3"),
        DECDHL_DoubleHeightLineBottom = VTID("#4"),
        DECSWL_SingleWidthLine = VTID("#5"),
        DECDWL_DoubleWidthLine = VTID("#6"),
        DECALN_ScreenAlignmentPattern = VTID("#8")
    };

    enum CsiActionCodes : uint64_t
    {
        ICH_InsertCharacter = VTID("@"),
        CUU_CursorUp = VTID("A"),
        CUD_CursorDown = VTID("B"),
        CUF_CursorForward = VTID("C"),
        CUB_CursorBackward = VTID("D"),
        CNL_CursorNextLine = VTID("E"),
        CPL_CursorPrevLine = VTID("F"),
        CHA_CursorHorizontalAbsolute = VTID("G"),
        CUP_CursorPosition = VTID("H"),
        CHT_CursorForwardTab = VTID("I"),
        ED_EraseDisplay = VTID("J"),
        DECSED_SelectiveEraseDisplay = VTID("?J"),
        EL_EraseLine = VTID("K"),
        DECSEL_SelectiveEraseLine = VTID("?K"),
        IL_InsertLine = VTID("L"),
        DL_DeleteLine = VTID("M"),
        DCH_DeleteCharacter = VTID("P"),
        SU_ScrollUp = VTID("S"),
        SD_ScrollDown = VTID("T"),
        NP_NextPage = VTID("U"),
        PP_PrecedingPage = VTID("V"),
        DECST8C_SetTabEvery8Columns = VTID("?W"),
        ECH_EraseCharacters = VTID("X"),
        CBT_CursorBackTab = VTID("Z"),
        HPA_HorizontalPositionAbsolute = VTID("`"),
        HPR_HorizontalPositionRelative = VTID("a"),
        REP_RepeatCharacter = VTID("b"),
        DA_DeviceAttributes = VTID("c"),
        DA2_SecondaryDeviceAttributes = VTID(">c"),
        DA3_TertiaryDeviceAttributes = VTID("=c"),
        VPA_VerticalLinePositionAbsolute = VTID("d"),
        VPR_VerticalPositionRelative = VTID("e"),
        HVP_HorizontalVerticalPosition = VTID("f"),
        TBC_TabClear = VTID("g"),
        SM_SetMode = VTID("h"),
        DECSET_PrivateModeSet = VTID("?h"),
        RM_ResetMode = VTID("l"),
        DECRST_PrivateModeReset = VTID("?l"),
        SGR_SetGraphicsRendition = VTID("m"),
        DSR_DeviceStatusReport = VTID("n"),
        DSR_PrivateDeviceStatusReport = VTID("?n"),
        DECSTBM_SetTopBottomMargins = VTID("r"),
        DECSLRM_SetLeftRightMargins = VTID("s"),
        DTTERM_WindowManipulation = VTID("t"), // NOTE: Overlaps with DECSLPP. Fix when/if implemented.
        ANSISYSRC_CursorRestore = VTID("u"),
        DECREQTPARM_RequestTerminalParameters = VTID("x"),
        PPA_PagePositionAbsolute = VTID(" P"),
        PPR_PagePositionRelative = VTID(" Q"),
        PPB_PagePositionBack = VTID(" R"),
        DECSCUSR_SetCursorStyle = VTID(" q"),
        DECSTR_SoftReset = VTID("!p"),
        DECSCA_SetCharacterProtectionAttribute = VTID("\"q"),
        DECRQDE_RequestDisplayedExtent = VTID("\"v"),
        XT_PushSgrAlias = VTID("#p"),
        XT_PopSgrAlias = VTID("#q"),
        XT_PushSgr = VTID("#{"),
        XT_PopSgr = VTID("#}"),
        DECRQM_RequestMode = VTID("$p"),
        DECRQM_PrivateRequestMode = VTID("?$p"),
        DECCARA_ChangeAttributesRectangularArea = VTID("$r"),
        DECRARA_ReverseAttributesRectangularArea = VTID("$t"),
        DECCRA_CopyRectangularArea = VTID("$v"),
        DECRQPSR_RequestPresentationStateReport = VTID("$w"),
        DECFRA_FillRectangularArea = VTID("$x"),
        DECERA_EraseRectangularArea = VTID("$z"),
        DECSERA_SelectiveEraseRectangularArea = VTID("${"),
        DECSCPP_SetColumnsPerPage = VTID("$|"),
        DECRQUPSS_RequestUserPreferenceSupplementalSet = VTID("&u"),
        DECIC_InsertColumn = VTID("'}"),
        DECDC_DeleteColumn = VTID("'~"),
        DECSACE_SelectAttributeChangeExtent = VTID("*x"),
        DECRQCRA_RequestChecksumRectangularArea = VTID("*y"),
        DECINVM_InvokeMacro = VTID("*z"),
        DECAC_AssignColor = VTID(",|"),
        DECPS_PlaySound = VTID(",~")
    };

    enum DcsActionCodes : uint64_t
    {
        DECDLD_DownloadDRCS = VTID("{"),
        DECAUPSS_AssignUserPreferenceSupplementalSet = VTID("!u"),
        DECDMAC_DefineMacro = VTID("!z"),
        DECRSTS_RestoreTerminalState = VTID("$p"),
        DECRQSS_RequestSetting = VTID("$q"),
        DECRSPS_RestorePresentationState = VTID("$t"),
    };

    enum Vt52ActionCodes : uint64_t
    {
        CursorUp = VTID("A"),
        CursorDown = VTID("B"),
        CursorRight = VTID("C"),
        CursorLeft = VTID("D"),
        EnterGraphicsMode = VTID("F"),
        ExitGraphicsMode = VTID("G"),
        CursorToHome = VTID("H"),
        ReverseLineFeed = VTID("I"),
        EraseToEndOfScreen = VTID("J"),
        EraseToEndOfLine = VTID("K"),
        DirectCursorAddress = VTID("Y"),
        Identify = VTID("Z"),
        EnterAlternateKeypadMode = VTID("="),
        ExitAlternateKeypadMode = VTID(">"),
        ExitVt52Mode = VTID("<")
    };

    enum OscActionCodes : unsigned int
    {
        SetIconAndWindowTitle = 0,
        SetWindowIcon = 1,
        SetWindowTitle = 2,
        SetWindowProperty = 3, // Not implemented
        SetColor = 4,
        Hyperlink = 8,
        ConEmuAction = 9,
        SetForegroundColor = 10,
        SetBackgroundColor = 11,
        SetCursorColor = 12,
        DECSWT_SetWindowTitle = 21,
        SetClipboard = 52,
        ResetForegroundColor = 110, // Not implemented
        ResetBackgroundColor = 111, // Not implemented
        ResetCursorColor = 112,
        FinalTermAction = 133,
        VsCodeAction = 633,
        ITerm2Action = 1337,
    };

    bool _GetOscSetColorTable(const std::wstring_view string,
                              std::vector<size_t>& tableIndexes,
                              std::vector<DWORD>& rgbs) const
    {
        const auto parts = Utils::SplitString(string, L';');
        if (parts.size() < 2)
        {
            return false;
        }

        std::vector<size_t> newTableIndexes;
        std::vector<DWORD> newRgbs;

        for (size_t i = 0, j = 1; j < parts.size(); i += 2, j += 2)
        {
            unsigned int tableIndex = 0;
            const auto indexSuccess = Utils::StringToUint(til::at(parts, i), tableIndex);
            const auto colorOptional = Utils::ColorFromXTermColor(til::at(parts, j));
            if (indexSuccess && colorOptional.has_value())
            {
                newTableIndexes.push_back(tableIndex);
                newRgbs.push_back(colorOptional.value());
            }
        }

        tableIndexes.swap(newTableIndexes);
        rgbs.swap(newRgbs);

        return tableIndexes.size() > 0 && rgbs.size() > 0;
    }

    bool _GetOscSetColor(const std::wstring_view string,
                         std::vector<DWORD>& rgbs) const
    {
        const auto parts = Utils::SplitString(string, L';');
        if (parts.size() < 1)
        {
            return false;
        }

        std::vector<DWORD> newRgbs;
        for (size_t i = 0; i < parts.size(); i++)
        {
            const auto colorOptional = Utils::ColorFromXTermColor(til::at(parts, i));
            if (colorOptional.has_value())
            {
                newRgbs.push_back(colorOptional.value());
            }
            else
            {
                newRgbs.push_back(INVALID_COLOR);
            }
        }

        rgbs.swap(newRgbs);

        return rgbs.size() > 0;
    }

    bool _GetOscSetClipboard(const std::wstring_view string,
                             std::wstring& content,
                             bool& queryClipboard) const noexcept
    {
        const auto pos = string.find(L';');
        if (pos == std::wstring_view::npos)
        {
            return false;
        }

        const auto substr = string.substr(pos + 1);
        if (substr == L"?")
        {
            queryClipboard = true;
            return true;
        }

// Log_IfFailed has the following description: "Should be decorated WI_NOEXCEPT, but conflicts with forceinline."
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'Log_IfFailed()' which may throw exceptions (f.6).
            return SUCCEEDED_LOG(Base64::Decode(substr, content));

    }

    static constexpr std::wstring_view hyperlinkIDParameter{ L"id=" };
    bool _ParseHyperlink(const std::wstring_view string,
                         std::wstring& params,
                         std::wstring& uri) const
    {
        params.clear();
        uri.clear();

        if (string == L";")
        {
            return true;
        }

        const auto midPos = string.find(';');
        if (midPos != std::wstring::npos)
        {
            uri = string.substr(midPos + 1, MAX_URL_LENGTH);
            const auto paramStr = string.substr(0, midPos);
            const auto paramParts = Utils::SplitString(paramStr, ':');
            for (const auto& part : paramParts)
            {
                const auto idPos = part.find(hyperlinkIDParameter);
                if (idPos != std::wstring::npos)
                {
                    params = part.substr(idPos + hyperlinkIDParameter.size());
                }
            }
            return true;
        }
        return false;
    }

    bool _CanSeqAcceptSubParam(const VTID id, const VTParameters& parameters) noexcept
    {
        switch (id)
        {
        case SGR_SetGraphicsRendition:
            return true;
        case DECCARA_ChangeAttributesRectangularArea:
        case DECRARA_ReverseAttributesRectangularArea:
            return !parameters.hasSubParamsFor(0) && !parameters.hasSubParamsFor(1) && !parameters.hasSubParamsFor(2) && !parameters.hasSubParamsFor(3);
        default:
            return false;
        }
    }

    void _ClearLastChar() noexcept
    {
        _lastPrintedChar = AsciiChars::NUL;
    }
};

}
