#pragma once

#include <span>
#include <vector>
#include <string>
#include <string_view>

#include "../parser/IStateMachineEngine.hpp"
#include "../adapter/ITermDispatch.hpp"

using namespace Microsoft::Console::VirtualTerminal;

struct LogData
{
    bool operator==(const LogData&) const = default;

    void push(auto&&... v)
    {
        ((_push(v)), ...);
    }

    void reset()
    {
        _vec.clear();
        _str.clear();
    }

private:
    static_assert(std::is_convertible_v<wil::zwstring_view, std::wstring_view>);

    template<class T>
    void _push(T v)
    {
        if constexpr (std::is_convertible_v<T, std::wstring_view>)
        {
            _str.append(static_cast<std::wstring_view>(v));
        }
        else if constexpr (std::is_same_v<T, VTParameters>)
        {
            for (size_t index = 0, size = v.size(); index < size; index++)
            {
                auto p = v.at(index);
                _push(p);
                if (v.hasSubParamsFor(index))
                {
                    auto sub = v.subParamsFor(index);
                    for (size_t sindex = 0, ssize = v.size(); sindex < ssize; sindex++)
                    {
                        auto sp = sub.at(sindex);
                        _push(sp);
                    }
                }
            }
        }
        else
        {
            const auto s = std::as_bytes(std::span{ &v, 1 });
            _vec.insert(_vec.end(), s.begin(), s.end());
        }
    }

    std::vector<std::byte> _vec;
    std::wstring _str;
};


struct DispLogger final : public ITermDispatch
{
    DispLogger(LogData& data) :
        _data(data)
    {
        _data.reset();
    }

    bool operator==(const DispLogger&) const = default;

    [[msvc::noinline]] void Print(const wchar_t wchPrintable) override
    {
        _data.push(Action::Print, wchPrintable);
    }

    [[msvc::noinline]] void PrintString(const std::wstring_view string) override
    {
        _data.push(Action::PrintString, string);
    }

    [[msvc::noinline]] bool CursorUp(const VTInt distance) override // CUU
    {
        _data.push(Action::CursorUp, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorDown(const VTInt distance) override // CUD
    {
        _data.push(Action::CursorDown, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorForward(const VTInt distance) override // CUF
    {
        _data.push(Action::CursorForward, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorBackward(const VTInt distance) override // CUB, BS
    {
        _data.push(Action::CursorBackward, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorNextLine(const VTInt distance) override // CNL
    {
        _data.push(Action::CursorNextLine, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorPrevLine(const VTInt distance) override // CPL
    {
        _data.push(Action::CursorPrevLine, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorHorizontalPositionAbsolute(const VTInt column) override // HPA, CHA
    {
        _data.push(Action::CursorHorizontalPositionAbsolute, column);
        return true;
    }

    [[msvc::noinline]] bool VerticalLinePositionAbsolute(const VTInt line) override // VPA
    {
        _data.push(Action::VerticalLinePositionAbsolute, line);
        return true;
    }

    [[msvc::noinline]] bool HorizontalPositionRelative(const VTInt distance) override // HPR
    {
        _data.push(Action::HorizontalPositionRelative, distance);
        return true;
    }

    [[msvc::noinline]] bool VerticalPositionRelative(const VTInt distance) override // VPR
    {
        _data.push(Action::VerticalPositionRelative, distance);
        return true;
    }

    [[msvc::noinline]] bool CursorPosition(const VTInt line, const VTInt column) override // CUP, HVP
    {
        _data.push(Action::CursorPosition, line, column);
        return true;
    }

    [[msvc::noinline]] bool CursorSaveState() override // DECSC
    {
        _data.push(Action::CursorSaveState);
        return true;
    }

    [[msvc::noinline]] bool CursorRestoreState() override // DECRC
    {
        _data.push(Action::CursorRestoreState);
        return true;
    }

    [[msvc::noinline]] bool InsertCharacter(const VTInt count) override // ICH
    {
        _data.push(Action::InsertCharacter, count);
        return true;
    }

    [[msvc::noinline]] bool DeleteCharacter(const VTInt count) override // DCH
    {
        _data.push(Action::DeleteCharacter, count);
        return true;
    }

    [[msvc::noinline]] bool ScrollUp(const VTInt distance) override // SU
    {
        _data.push(Action::ScrollUp, distance);
        return true;
    }

    [[msvc::noinline]] bool ScrollDown(const VTInt distance) override // SD
    {
        _data.push(Action::ScrollDown, distance);
        return true;
    }

    [[msvc::noinline]] bool NextPage(const VTInt pageCount) override // NP
    {
        _data.push(Action::NextPage, pageCount);
        return true;
    }

    [[msvc::noinline]] bool PrecedingPage(const VTInt pageCount) override // PP
    {
        _data.push(Action::PrecedingPage, pageCount);
        return true;
    }

    [[msvc::noinline]] bool PagePositionAbsolute(const VTInt page) override // PPA
    {
        _data.push(Action::PagePositionAbsolute, page);
        return true;
    }

    [[msvc::noinline]] bool PagePositionRelative(const VTInt pageCount) override // PPR
    {
        _data.push(Action::PagePositionRelative, pageCount);
        return true;
    }

    [[msvc::noinline]] bool PagePositionBack(const VTInt pageCount) override // PPB
    {
        _data.push(Action::PagePositionBack, pageCount);
        return true;
    }

    [[msvc::noinline]] bool RequestDisplayedExtent() override // DECRQDE
    {
        _data.push(Action::RequestDisplayedExtent);
        return true;
    }

    [[msvc::noinline]] bool InsertLine(const VTInt distance) override // IL
    {
        _data.push(Action::InsertLine, distance);
        return true;
    }

    [[msvc::noinline]] bool DeleteLine(const VTInt distance) override // DL
    {
        _data.push(Action::DeleteLine, distance);
        return true;
    }

    [[msvc::noinline]] bool InsertColumn(const VTInt distance) override // DECIC
    {
        _data.push(Action::InsertColumn, distance);
        return true;
    }

    [[msvc::noinline]] bool DeleteColumn(const VTInt distance) override // DECDC
    {
        _data.push(Action::DeleteColumn, distance);
        return true;
    }

    [[msvc::noinline]] bool SetKeypadMode(const bool applicationMode) override // DECKPAM, DECKPNM
    {
        _data.push(Action::SetKeypadMode, applicationMode);
        return true;
    }

    [[msvc::noinline]] bool SetAnsiMode(const bool ansiMode) override // DECANM
    {
        _data.push(Action::SetAnsiMode, ansiMode);
        return true;
    }

    [[msvc::noinline]] bool SetTopBottomScrollingMargins(const VTInt topMargin, const VTInt bottomMargin) override // DECSTBM
    {
        _data.push(Action::SetTopBottomScrollingMargins, topMargin, bottomMargin);
        return true;
    }

    [[msvc::noinline]] bool SetLeftRightScrollingMargins(const VTInt leftMargin, const VTInt rightMargin) override // DECSLRM
    {
        _data.push(Action::SetLeftRightScrollingMargins, leftMargin, rightMargin);
        return true;
    }

    [[msvc::noinline]] bool WarningBell() override // BEL
    {
        _data.push(Action::WarningBell);
        return true;
    }

    [[msvc::noinline]] bool CarriageReturn() override // CR
    {
        _data.push(Action::CarriageReturn);
        return true;
    }

    [[msvc::noinline]] bool LineFeed(const DispatchTypes::LineFeedType lineFeedType) override // IND, NEL, LF, FF, VT
    {
        _data.push(Action::LineFeed, lineFeedType);
        return true;
    }

    [[msvc::noinline]] bool ReverseLineFeed() override // RI
    {
        _data.push(Action::ReverseLineFeed);
        return true;
    }

    [[msvc::noinline]] bool BackIndex() override // DECBI
    {
        _data.push(Action::BackIndex);
        return true;
    }

    [[msvc::noinline]] bool ForwardIndex() override // DECFI
    {
        _data.push(Action::ForwardIndex);
        return true;
    }

    [[msvc::noinline]] bool SetWindowTitle(std::wstring_view title) override // DECSWT, OscWindowTitle
    {
        _data.push(Action::SetWindowTitle, title);
        return true;
    }

    [[msvc::noinline]] bool HorizontalTabSet() override // HTS
    {
        _data.push(Action::HorizontalTabSet);
        return true;
    }

    [[msvc::noinline]] bool ForwardTab(const VTInt numTabs) override // CHT, HT
    {
        _data.push(Action::ForwardTab, numTabs);
        return true;
    }

    [[msvc::noinline]] bool BackwardsTab(const VTInt numTabs) override // CBT
    {
        _data.push(Action::BackwardsTab, numTabs);
        return true;
    }

    [[msvc::noinline]] bool TabClear(const DispatchTypes::TabClearType clearType) override // TBC
    {
        _data.push(Action::TabClear, clearType);
        return true;
    }

    [[msvc::noinline]] bool TabSet(const VTParameter setType) override // DECST8C
    {
        _data.push(Action::TabSet, setType);
        return true;
    }

    [[msvc::noinline]] bool SetColorTableEntry(const size_t tableIndex, const DWORD color) override // OSCColorTable
    {
        _data.push(Action::SetColorTableEntry, tableIndex, color);
        return true;
    }

    [[msvc::noinline]] bool SetDefaultForeground(const DWORD color) override // OSCDefaultForeground
    {
        _data.push(Action::SetDefaultForeground, color);
        return true;
    }

    [[msvc::noinline]] bool SetDefaultBackground(const DWORD color) override // OSCDefaultBackground
    {
        _data.push(Action::SetDefaultBackground, color);
        return true;
    }

    [[msvc::noinline]] bool AssignColor(const DispatchTypes::ColorItem item, const VTInt fgIndex, const VTInt bgIndex) override // DECAC
    {
        _data.push(Action::AssignColor, item, fgIndex, bgIndex);
        return true;
    }

    [[msvc::noinline]] bool EraseInDisplay(const DispatchTypes::EraseType eraseType) override // ED
    {
        _data.push(Action::EraseInDisplay, eraseType);
        return true;
    }

    [[msvc::noinline]] bool EraseInLine(const DispatchTypes::EraseType eraseType) override // EL
    {
        _data.push(Action::EraseInLine, eraseType);
        return true;
    }

    [[msvc::noinline]] bool EraseCharacters(const VTInt numChars) override // ECH
    {
        _data.push(Action::EraseCharacters, numChars);
        return true;
    }

    [[msvc::noinline]] bool SelectiveEraseInDisplay(const DispatchTypes::EraseType eraseType) override // DECSED
    {
        _data.push(Action::SelectiveEraseInDisplay, eraseType);
        return true;
    }

    [[msvc::noinline]] bool SelectiveEraseInLine(const DispatchTypes::EraseType eraseType) override // DECSEL
    {
        _data.push(Action::SelectiveEraseInLine, eraseType);
        return true;
    }

    [[msvc::noinline]] bool ChangeAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override // DECCARA
    {
        _data.push(Action::ChangeAttributesRectangularArea, top, left, bottom, right, attrs);
        return true;
    }

    [[msvc::noinline]] bool ReverseAttributesRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTParameters attrs) override // DECRARA
    {
        _data.push(Action::ReverseAttributesRectangularArea, top, left, bottom, right, attrs);
        return true;
    }

    [[msvc::noinline]] bool CopyRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right, const VTInt page, const VTInt dstTop, const VTInt dstLeft, const VTInt dstPage) override // DECCRA
    {
        _data.push(Action::CopyRectangularArea, top, left, bottom, right, page, dstTop, dstLeft, dstPage);
        return true;
    }

    [[msvc::noinline]] bool FillRectangularArea(const VTParameter ch, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override // DECFRA
    {
        _data.push(Action::FillRectangularArea, ch, top, left, bottom, right);
        return true;
    }

    [[msvc::noinline]] bool EraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override // DECERA
    {
        _data.push(Action::EraseRectangularArea, top, left, bottom, right);
        return true;
    }

    [[msvc::noinline]] bool SelectiveEraseRectangularArea(const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override // DECSERA
    {
        _data.push(Action::SelectiveEraseRectangularArea, top, left, bottom, right);
        return true;
    }

    [[msvc::noinline]] bool SelectAttributeChangeExtent(const DispatchTypes::ChangeExtent changeExtent) override // DECSACE
    {
        _data.push(Action::SelectAttributeChangeExtent, changeExtent);
        return true;
    }

    [[msvc::noinline]] bool RequestChecksumRectangularArea(const VTInt id, const VTInt page, const VTInt top, const VTInt left, const VTInt bottom, const VTInt right) override // DECRQCRA
    {
        _data.push(Action::RequestChecksumRectangularArea, id, page, top, left, bottom, right);
        return true;
    }

    [[msvc::noinline]] bool SetGraphicsRendition(const VTParameters options) override // SGR
    {
        _data.push(Action::SetGraphicsRendition, options);
        return true;
    }

    [[msvc::noinline]] bool SetLineRendition(const LineRendition rendition) override // DECSWL, DECDWL, DECDHL
    {
        _data.push(Action::SetLineRendition, rendition);
        return true;
    }

    [[msvc::noinline]] bool SetCharacterProtectionAttribute(const VTParameters options) override // DECSCA
    {
        _data.push(Action::SetCharacterProtectionAttribute, options);
        return true;
    }

    [[msvc::noinline]] bool PushGraphicsRendition(const VTParameters options) override // XTPUSHSGR
    {
        _data.push(Action::PushGraphicsRendition, options);
        return true;
    }

    [[msvc::noinline]] bool PopGraphicsRendition() override // XTPOPSGR
    {
        _data.push(Action::PopGraphicsRendition);
        return true;
    }

    [[msvc::noinline]] bool SetMode(const DispatchTypes::ModeParams param) override // SM, DECSET
    {
        _data.push(Action::SetMode, param);
        return true;
    }

    [[msvc::noinline]] bool ResetMode(const DispatchTypes::ModeParams param) override // RM, DECRST
    {
        _data.push(Action::ResetMode, param);
        return true;
    }

    [[msvc::noinline]] bool RequestMode(const DispatchTypes::ModeParams param) override // DECRQM
    {
        _data.push(Action::RequestMode, param);
        return true;
    }

    [[msvc::noinline]] bool DeviceStatusReport(const DispatchTypes::StatusType statusType, const VTParameter id) override // DSR
    {
        _data.push(Action::DeviceStatusReport, statusType, id);
        return true;
    }

    [[msvc::noinline]] bool DeviceAttributes() override // DA1
    {
        _data.push(Action::DeviceAttributes);
        return true;
    }

    [[msvc::noinline]] bool SecondaryDeviceAttributes() override // DA2
    {
        _data.push(Action::SecondaryDeviceAttributes);
        return true;
    }

    [[msvc::noinline]] bool TertiaryDeviceAttributes() override // DA3
    {
        _data.push(Action::TertiaryDeviceAttributes);
        return true;
    }

    [[msvc::noinline]] bool Vt52DeviceAttributes() override // VT52 Identify
    {
        _data.push(Action::Vt52DeviceAttributes);
        return true;
    }

    [[msvc::noinline]] bool RequestTerminalParameters(const DispatchTypes::ReportingPermission permission) override // DECREQTPARM
    {
        _data.push(Action::RequestTerminalParameters, permission);
        return true;
    }

    [[msvc::noinline]] bool DesignateCodingSystem(const VTID codingSystem) override // DOCS
    {
        _data.push(Action::DesignateCodingSystem, codingSystem);
        return true;
    }

    [[msvc::noinline]] bool Designate94Charset(const VTInt gsetNumber, const VTID charset) override // SCS
    {
        _data.push(Action::Designate94Charset, gsetNumber, charset);
        return true;
    }

    [[msvc::noinline]] bool Designate96Charset(const VTInt gsetNumber, const VTID charset) override // SCS
    {
        _data.push(Action::Designate96Charset, gsetNumber, charset);
        return true;
    }

    [[msvc::noinline]] bool LockingShift(const VTInt gsetNumber) override // LS0, LS1, LS2, LS3
    {
        _data.push(Action::LockingShift, gsetNumber);
        return true;
    }

    [[msvc::noinline]] bool LockingShiftRight(const VTInt gsetNumber) override // LS1R, LS2R, LS3R
    {
        _data.push(Action::LockingShiftRight, gsetNumber);
        return true;
    }

    [[msvc::noinline]] bool SingleShift(const VTInt gsetNumber) override // SS2, SS3
    {
        _data.push(Action::SingleShift, gsetNumber);
        return true;
    }

    [[msvc::noinline]] bool AcceptC1Controls(const bool enabled) override // DECAC1
    {
        _data.push(Action::AcceptC1Controls, enabled);
        return true;
    }

    [[msvc::noinline]] bool AnnounceCodeStructure(const VTInt ansiLevel) override // ACS
    {
        _data.push(Action::AnnounceCodeStructure, ansiLevel);
        return true;
    }

    [[msvc::noinline]] bool SoftReset() override // DECSTR
    {
        _data.push(Action::SoftReset);
        return true;
    }

    [[msvc::noinline]] bool HardReset() override // RIS
    {
        _data.push(Action::HardReset);
        return true;
    }

    [[msvc::noinline]] bool ScreenAlignmentPattern() override // DECALN
    {
        _data.push(Action::ScreenAlignmentPattern);
        return true;
    }

    [[msvc::noinline]] bool SetCursorStyle(const DispatchTypes::CursorStyle cursorStyle) override // DECSCUSR
    {
        _data.push(Action::SetCursorStyle, cursorStyle);
        return true;
    }

    [[msvc::noinline]] bool SetCursorColor(const COLORREF color) override // OSCSetCursorColor, OSCResetCursorColor
    {
        _data.push(Action::SetCursorColor, color);
        return true;
    }

    [[msvc::noinline]] bool SetClipboard(wil::zwstring_view content) override // OSCSetClipboard
    {
        _data.push(Action::SetClipboard, content);
        return true;
    }

    [[msvc::noinline]] bool WindowManipulation(const DispatchTypes::WindowManipulationType function, const VTParameter parameter1, const VTParameter parameter2) override
    {
        _data.push(Action::WindowManipulation, function, parameter1, parameter2);
        return true;
    }

    [[msvc::noinline]] bool AddHyperlink(const std::wstring_view uri, const std::wstring_view params) override
    {
        _data.push(Action::AddHyperlink, uri, params);
        return true;
    }

    [[msvc::noinline]] bool EndHyperlink() override
    {
        _data.push(Action::EndHyperlink);
        return true;
    }

    [[msvc::noinline]] bool DoConEmuAction(const std::wstring_view string) override
    {
        _data.push(Action::DoConEmuAction, string);
        return true;
    }

    [[msvc::noinline]] bool DoITerm2Action(const std::wstring_view string) override
    {
        _data.push(Action::DoITerm2Action, string);
        return true;
    }

    [[msvc::noinline]] bool DoFinalTermAction(const std::wstring_view string) override
    {
        _data.push(Action::DoFinalTermAction, string);
        return true;
    }

    [[msvc::noinline]] bool DoVsCodeAction(const std::wstring_view string) override
    {
        _data.push(Action::DoVsCodeAction, string);
        return true;
    }

    [[msvc::noinline]] StringHandler DownloadDRCS(const VTInt fontNumber, const VTParameter startChar, const DispatchTypes::DrcsEraseControl eraseControl, const DispatchTypes::DrcsCellMatrix cellMatrix, const DispatchTypes::DrcsFontSet fontSet, const DispatchTypes::DrcsFontUsage fontUsage, const VTParameter cellHeight, const DispatchTypes::CharsetSize charsetSize) override // DECDLD
    {
        _data.push(Action::DownloadDRCS, fontNumber, startChar, eraseControl, cellMatrix, fontSet, fontUsage, cellHeight, charsetSize);
        return nullptr;
    }

    [[msvc::noinline]] bool RequestUserPreferenceCharset() override // DECRQUPSS
    {
        _data.push(Action::RequestUserPreferenceCharset);
        return true;
    }

    [[msvc::noinline]] StringHandler AssignUserPreferenceCharset(const DispatchTypes::CharsetSize charsetSize) override // DECAUPSS
    {
        _data.push(Action::AssignUserPreferenceCharset, charsetSize);
        return nullptr;
    }

    [[msvc::noinline]] StringHandler DefineMacro(const VTInt macroId, const DispatchTypes::MacroDeleteControl deleteControl, const DispatchTypes::MacroEncoding encoding) override // DECDMAC
    {
        _data.push(Action::DefineMacro, macroId, deleteControl, encoding);
        return nullptr;
    }

    [[msvc::noinline]] bool InvokeMacro(const VTInt macroId) override // DECINVM
    {
        _data.push(Action::InvokeMacro, macroId);
        return true;
    }

    [[msvc::noinline]] StringHandler RestoreTerminalState(const DispatchTypes::ReportFormat format) override // DECRSTS
    {
        _data.push(Action::RestoreTerminalState, format);
        return nullptr;
    }

    [[msvc::noinline]] StringHandler RequestSetting() override // DECRQSS
    {
        _data.push(Action::RequestSetting);
        return nullptr;
    }

    [[msvc::noinline]] bool RequestPresentationStateReport(const DispatchTypes::PresentationReportFormat format) override // DECRQPSR
    {
        _data.push(Action::RequestPresentationStateReport, format);
        return true;
    }

    [[msvc::noinline]] StringHandler RestorePresentationState(const DispatchTypes::PresentationReportFormat format) override // DECRSPS
    {
        _data.push(Action::RestorePresentationState, format);
        return nullptr;
    }

    [[msvc::noinline]] bool PlaySounds(const VTParameters parameters) override // DECPS
    {
        _data.push(Action::PlaySounds, parameters);
        return true;
    }

    enum class Action : uint8_t
    {
        Print,
        PrintString,
        CursorUp,
        CursorDown,
        CursorForward,
        CursorBackward,
        CursorNextLine,
        CursorPrevLine,
        CursorHorizontalPositionAbsolute,
        VerticalLinePositionAbsolute,
        HorizontalPositionRelative,
        VerticalPositionRelative,
        CursorPosition,
        CursorSaveState,
        CursorRestoreState,
        InsertCharacter,
        DeleteCharacter,
        ScrollUp,
        ScrollDown,
        NextPage,
        PrecedingPage,
        PagePositionAbsolute,
        PagePositionRelative,
        PagePositionBack,
        RequestDisplayedExtent,
        InsertLine,
        DeleteLine,
        InsertColumn,
        DeleteColumn,
        SetKeypadMode,
        SetAnsiMode,
        SetTopBottomScrollingMargins,
        SetLeftRightScrollingMargins,
        WarningBell,
        CarriageReturn,
        LineFeed,
        ReverseLineFeed,
        BackIndex,
        ForwardIndex,
        SetWindowTitle,
        HorizontalTabSet,
        ForwardTab,
        BackwardsTab,
        TabClear,
        TabSet,
        SetColorTableEntry,
        SetDefaultForeground,
        SetDefaultBackground,
        AssignColor,
        EraseInDisplay,
        EraseInLine,
        EraseCharacters,
        SelectiveEraseInDisplay,
        SelectiveEraseInLine,
        ChangeAttributesRectangularArea,
        ReverseAttributesRectangularArea,
        CopyRectangularArea,
        FillRectangularArea,
        EraseRectangularArea,
        SelectiveEraseRectangularArea,
        SelectAttributeChangeExtent,
        RequestChecksumRectangularArea,
        SetGraphicsRendition,
        SetLineRendition,
        SetCharacterProtectionAttribute,
        PushGraphicsRendition,
        PopGraphicsRendition,
        SetMode,
        ResetMode,
        RequestMode,
        DeviceStatusReport,
        DeviceAttributes,
        SecondaryDeviceAttributes,
        TertiaryDeviceAttributes,
        Vt52DeviceAttributes,
        RequestTerminalParameters,
        DesignateCodingSystem,
        Designate94Charset,
        Designate96Charset,
        LockingShift,
        LockingShiftRight,
        SingleShift,
        AcceptC1Controls,
        AnnounceCodeStructure,
        SoftReset,
        HardReset,
        ScreenAlignmentPattern,
        SetCursorStyle,
        SetCursorColor,
        SetClipboard,
        WindowManipulation,
        AddHyperlink,
        EndHyperlink,
        DoConEmuAction,
        DoITerm2Action,
        DoFinalTermAction,
        DoVsCodeAction,
        DownloadDRCS,
        RequestUserPreferenceCharset,
        AssignUserPreferenceCharset,
        DefineMacro,
        InvokeMacro,
        RestoreTerminalState,
        RequestSetting,
        RequestPresentationStateReport,
        RestorePresentationState,
        PlaySounds,
    };

private:
    LogData& _data;
};
