/***************************************************************************************************
 *
 *   RawInputViewer - A utility to test, visualize, and map WM_INPUT messages.
 *
 *   Copyright (c) 2025 by Bitdancer (@RealBitdancer)
 *
 *   Licensed under the MIT License. See LICENSE file in the repository for details.
 *
 *   Source: https://github.com/RealBitdancer/RawInputViewer
 *
 **************************************************************************************************/

#include "RawInputViewer.hpp"
#include "resource.h"
#include <map>

BEGIN_ANONYMOUS_NAMESPACE

class MainWindow final : public Window
{
private:
    [[nodiscard]] bool adjustKeyboardInput(RawKeyboard& rawKbd)
    {
        // Filter out overruns
        if (rawKbd.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE)
        {
            return false;
        }

        // Handle Ctrl+{key} sequence
        if ((rawKbd.Flags & RI_KEY_E1) != 0)
        {
            pendingSequence_ = ScanCodeSequence::E1;
            return false;
        }

        // 0xE02A (fake L-shift) indicates the start of an E0 key sequence
        if ((rawKbd.Flags & RI_KEY_E0) != 0 && rawKbd.MakeCode == 0x2A)
        {
            pendingSequence_ = ScanCodeSequence::E0;
            return false;
        }

        const ScanCodeSequence pendingSequence = std::exchange(pendingSequence_, ScanCodeSequence::None);

        if (rawKbd.MakeCode == 0)
        {
            // If we don't have a make code, try to get it from the VK.
            // Flags should still be set correctly, even though MakeCode == 0.
            rawKbd.MakeCode = LOWORD(MapVirtualKey(rawKbd.VKey, MAPVK_VK_TO_VSC_EX));
            rawKbd.adjustments |= AdjustmentFlags::MakeCodeMapped;
        }

        if (rawKbd.MakeCode == 0)
        {
            return false;
        }

        if (rawKbd.MakeCode == 0x45)
        {
            if (pendingSequence == ScanCodeSequence::E1)
            {
                // Must be Pause/Break
                rawKbd.VKey = VK_PAUSE;
                rawKbd.adjustments |= AdjustmentFlags::VirtualKeyAdjusted;
            }
            else
            {
                // Must be Num Lock
                rawKbd.adjustments |= AdjustmentFlags::ExtendedLookup;
            }
        }

        // Adjust virtual keys to match reality
        switch (const bool isE0 = (rawKbd.Flags & RI_KEY_E0) != 0; rawKbd.VKey)
        {
            case VK_SHIFT:
            {
                if (rawKbd.MakeCode == 0x2a)
                {
                    rawKbd.VKey = VK_LSHIFT;
                    rawKbd.adjustments |= AdjustmentFlags::VirtualKeyAdjusted;
                }
                else if (rawKbd.MakeCode == 0x36)
                {
                    rawKbd.VKey = VK_RSHIFT;
                    rawKbd.adjustments |= AdjustmentFlags::VirtualKeyAdjusted;
                }
                break;
            }
            case VK_CONTROL:
            {
                if (isE0)
                {
                    rawKbd.VKey = VK_RCONTROL;
                    rawKbd.adjustments |= AdjustmentFlags::VirtualKeyAdjusted;
                }
                break;
            }
            case VK_MENU:
            {
                if (isE0)
                {
                    rawKbd.VKey = VK_RMENU;
                    rawKbd.adjustments |= AdjustmentFlags::VirtualKeyAdjusted;
                }
                break;
            }
        }

        return true;
    }

    void addKeyEventToListView(const RawKeyboard& rawKbd)
    {
        if (const int item = listView_.insertItem(listView_.getItemCount(), rawKbd); item >= 0)
        {
            listView_.ensureVisible(item, false);
        }
    }

    void clearListView() noexcept
    {
        listView_.deleteAllItems();
        pendingSequence_ = ScanCodeSequence::None;
    }

    void adjustLayout() noexcept
    {
        const SIZE size = getClientSize();
        const int statusBarHeight = statusBar_.getHeight();

        // Prevent the TB buttons to jump up and down when resizing the main window
        toolBar_.move(0, 0, size.cx, toolBar_.getHeight(), TRUE);
        const int toolBarHeight = toolBar_.getHeight();

        statusBar_.move(0, size.cy - statusBarHeight, size.cx, statusBarHeight, TRUE);
        listView_.move(0, toolBarHeight, size.cx, size.cy - toolBarHeight - statusBarHeight, TRUE);
    }

    [[nodiscard]] WINDOWPLACEMENT getWindowPlacement() const noexcept
    {
        // clang-format off
        WINDOWPLACEMENT windowPlacement
        {
            .length = sizeof(windowPlacement),
            .showCmd = SW_SHOWNORMAL,
            .ptMinPosition = {-1, -1},
            .ptMaxPosition = {-1, -1},
            .rcNormalPosition = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT}
        };
        // clang-format on

        // In case of GetWindowPlacement() failing, the default
        // values are stored in the registry, which is fine.
        GetWindowPlacement(hwnd_, &windowPlacement);

        return windowPlacement;
    }

    auto lookupVirtualKey(const RawKeyboard& rawKbd) const noexcept
    {
        const auto it = vkeyMapping_.find(rawKbd.VKey);
        if (it == std::end(vkeyMapping_))
        {
            return vkeyMapping_.find(0xff);
        }
        return it;
    }

    auto lookupKeyCode(const RawKeyboard& rawKbd) const noexcept
    {
        const auto it = scanCodeMapping_.find(rawKbd.getLookupCode());
        if (it == std::end(scanCodeMapping_))
        {
            return scanCodeMapping_.find(0x000);
        }
        return it;
    }

    [[nodiscard]] std::optional<LRESULT> getListViewItemDisplayInfo(LVITEMW& item)
    {
        if ((item.mask & LVIF_TEXT) == 0)
        {
            return std::nullopt;
        }

        auto formatTo = [this]<typename T>(const T& from, LVITEMW& to, ListView::DisplayFormat format, int version = 0) -> LPARAM
        {
            if constexpr (std::is_integral_v<T>)
            {
                switch (MAKELONG(std::to_underlying(format), version))
                {
                    case MAKELONG(std::to_underlying(ListView::DisplayFormat::Hex), 0):
                    {
                        *std::format_to_n(to.pszText, to.cchTextMax - 1, L"{:#04x}", from).out = L'\0';
                        break;
                    }
                    case MAKELONG(std::to_underlying(ListView::DisplayFormat::Hex), 1):
                    {
                        *std::format_to_n(to.pszText, to.cchTextMax - 1, L"{:#05x}", from).out = L'\0';
                        break;
                    }
                    case MAKELONG(std::to_underlying(ListView::DisplayFormat::Hex), 2):
                    {
                        StringResource<32> na(hinstance_, IDS_NA);
                        na.copyTo(to.pszText, to.cchTextMax);
                        break;
                    }
                    case MAKELONG(std::to_underlying(ListView::DisplayFormat::Bin), 0):
                    {
                        *std::format_to_n(to.pszText, to.cchTextMax - 1, L"{:#010b}", from).out = L'\0';
                        break;
                    }
                    default:
                    {
                        *std::format_to_n(to.pszText, to.cchTextMax - 1, L"{}", from).out = L'\0';
                        break;
                    }
                }
            }
            else
            {
                *std::format_to_n(to.pszText, to.cchTextMax - 1, L"{}", from).out = L'\0';
            }
            return TRUE;
        };

        switch (const RawKeyboard rawKbd(PackedRawKeyboard{item.lParam}.getRawKeyboard()); item.iSubItem)
        {
            case 0:
            {
                const auto it = lookupVirtualKey(rawKbd);
                return formatTo(it->second.second.c_str(), item, listView_.getDisplayFormat(item.iSubItem));
            }
            case 1:
            {
                const auto it = lookupVirtualKey(rawKbd);
                return formatTo(it->second.first.c_str(), item, listView_.getDisplayFormat(item.iSubItem));
            }
            case 2:
            {
                return formatTo(rawKbd.VKey, item, listView_.getDisplayFormat(item.iSubItem));
            }
            case 3:
            {
                return formatTo(rawKbd.MakeCode, item, listView_.getDisplayFormat(item.iSubItem));
            }
            case 4:
            {
                return formatTo(rawKbd.Flags, item, listView_.getDisplayFormat(item.iSubItem));
            }
            case 5:
            {
                switch (const auto it = lookupKeyCode(rawKbd); listView_.getDisplayFormat(item.iSubItem))
                {
                    case ListView::DisplayFormat::Sml:
                    {
                        return formatTo(it->second.sml, item, ListView::DisplayFormat::Sml);
                    }
                    case ListView::DisplayFormat::Ray:
                    {
                        return formatTo(it->second.ray, item, ListView::DisplayFormat::Ray);
                    }
                    case ListView::DisplayFormat::Glfw:
                    {
                        return formatTo(it->second.glfw, item, ListView::DisplayFormat::Glfw);
                    }
                }
                break;
            }
            case 6:
            {
                const int keyCode = lookupKeyCode(rawKbd)->second.keyCode;
                return formatTo(keyCode, item, listView_.getDisplayFormat(item.iSubItem), keyCode > 0 ? 1 : 2);
            }
        }

        return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
    }

    [[nodiscard]] std::optional<LRESULT> customDrawListViewItem(NMLVCUSTOMDRAW* customDraw)
    {
        switch (customDraw->nmcd.dwDrawStage)
        {
            case CDDS_PREPAINT:
            {
                return CDRF_NOTIFYITEMDRAW;
            }
            case CDDS_ITEMPREPAINT:
            {
                return CDRF_NOTIFYSUBITEMDRAW;
            }
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
            {
                const AdjustmentFlags flags = AdjustmentFlags::MakeCodeMapped | AdjustmentFlags::VirtualKeyAdjusted;
                const RawKeyboard rawKbd = PackedRawKeyboard{customDraw->nmcd.lItemlParam}.getRawKeyboard();
                if ((rawKbd.adjustments & flags) != AdjustmentFlags{0})
                {
                    // Draw adjusted values (VK or scan code) in bold to hint to the user what was adjusted.
                    const int mask = (rawKbd.adjustments & AdjustmentFlags::VirtualKeyAdjusted) != AdjustmentFlags{0} ? 0b0110 : 0b1000;
                    SelectObject(customDraw->nmcd.hdc, ((1 << customDraw->iSubItem) & mask) != 0 ? listView_.getBoldFont() : listView_.getFont());
                    customDraw->clrText = GetSysColor(COLOR_INFOTEXT);
                    customDraw->clrTextBk = GetSysColor(COLOR_INFOBK);
                    return CDRF_NEWFONT;
                }
                return CDRF_DODEFAULT;
            }
        }

        return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
    }

    bool copyToolTip(HINSTANCE hinstance, const NMTBGETINFOTIPW* infoTip, std::initializer_list<ToolTipPair> pairs)
    {
        for (const auto& pair : pairs)
        {
            if (infoTip->iItem == pair.buttonId)
            {
                StringResource<128> tip(hinstance, pair.toolTipId);
                tip.copyTo(infoTip->pszText, infoTip->cchTextMax);
                return true;
            }
        }
        return false;
    }

#pragma region Window message handling

    [[nodiscard]] std::optional<LRESULT> onCreate(HWND, UINT, WPARAM, LPARAM)
    {
        toolBar_.create(hinstance_, *this);
        listView_.create<IDS_COLUMNS>(hinstance_, *this);
        statusBar_.create(hinstance_, *this);
        return 0;
    }

    [[nodiscard]] std::optional<LRESULT> onInput(HWND, UINT, WPARAM wParam, LPARAM lParam)
    {
        UINT size = 0;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        TempBuffer<void> buffer(size);
        RAWINPUT* raw = static_cast<RAWINPUT*>(buffer.data());

        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        switch (raw->header.dwType)
        {
            case RIM_TYPEKEYBOARD:
            {
                RawKeyboard rawKbd(raw->data.keyboard);

                if (!toolBar_.isAdjustmentChecked() || adjustKeyboardInput(rawKbd))
                {
                    addKeyEventToListView(rawKbd);
                }
                break;
            }
            case RIM_TYPEMOUSE:
            {
                if (raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
                {
                    clearListView();
                }
                break;
            }
        }

        // Per https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-input,
        // if inputCode is RIM_INPUT (0), DefWindowProc() must be called for system cleanup.
        // Returning std::nullopt causes windowSubclassProc() to call DefSubclassProc(),
        // satisfying this requirement. For RIM_INPUTSINK (1), return 0 as processed.
        const int inputCode = GET_RAWINPUT_CODE_WPARAM(wParam);
        return inputCode == RIM_INPUT ? std::nullopt : std::optional<LRESULT>(0);
    }

    [[nodiscard]] std::optional<LRESULT> onSize(HWND, UINT, WPARAM, LPARAM)
    {
        adjustLayout();
        return 0;
    }

    [[nodiscard]] std::optional<LRESULT> onCommand(HWND, UINT, WPARAM wParam, LPARAM)
    {
        switch (LOWORD(wParam))
        {
            case ID_CLEAR_LIST_VIEW:
            {
                clearListView();
                return 0;
            }
            case ID_NOHOTKEYS:
            case ID_NOLEGACY:
            {
                registerRawInputDevice();
                return 0;
            }
        }

        return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
    }

    [[nodiscard]] std::optional<LRESULT> onNotify(HWND, UINT, WPARAM, LPARAM lParam)
    {
        if (auto hdr = reinterpret_cast<const NMHDR*>(lParam); listView_.isSame(hdr->hwndFrom))
        {
            switch (hdr->code)
            {
                case LVN_GETDISPINFO:
                {
                    return getListViewItemDisplayInfo(reinterpret_cast<NMLVDISPINFOW*>(lParam)->item);
                }
                case NM_CUSTOMDRAW:
                {
                    return customDrawListViewItem(reinterpret_cast<NMLVCUSTOMDRAW*>(lParam));
                }
                case LVN_ITEMCHANGING:
                {
                    auto nmlv = reinterpret_cast<const NMLISTVIEW*>(lParam);
                    if (nmlv->uChanged & LVIF_STATE)
                    {
                        if ((nmlv->uNewState & LVIS_SELECTED) != (nmlv->uOldState & LVIS_SELECTED))
                        {
                            return TRUE; // Prevent selection change
                        }
                    }
                }
            }
        }
        else if (listView_.isHeader(hdr->hwndFrom) && hdr->code == HDN_DROPDOWN)
        {
            auto header = reinterpret_cast<const NMHEADERW*>(lParam);
            listView_.showSplitButtonMenu(hinstance_, header->iItem);
            return 0;
        }
        else if (toolBar_.isSame(hdr->hwndFrom) && hdr->code == TBN_GETINFOTIPW)
        {
            auto infoTip = reinterpret_cast<const NMTBGETINFOTIPW*>(lParam);
            copyToolTip(hinstance_, infoTip, {{ID_CLEAR_LIST_VIEW, IDS_TOOLTIP_CLEAR}, {ID_TOGGLE_ADJUSTMENT, IDS_TOOLTIP_ADJUST}});
            return 0;
        }
        else if (statusBar_.isToolBar(hdr->hwndFrom) && hdr->code == TBN_GETINFOTIPW)
        {
            auto infoTip = reinterpret_cast<const NMTBGETINFOTIPW*>(lParam);
            copyToolTip(hinstance_, infoTip, {{ID_NOHOTKEYS, IDS_TOOLTIP_NOHOTKEYS}, {ID_NOLEGACY, IDS_TOOLTIP_NOLEGACY}});
            return 0;
        }

        return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
    }

    [[nodiscard]] std::optional<LRESULT> onClose([[maybe_unused]] HWND hwnd, UINT, WPARAM, LPARAM)
    {
        _ASSERT(hwnd == hwnd_);
        // Save the last window position, size, list view column widths, and selected view type
        CurrentUserRegKey regKey = getAppRegKey(RegKeyDisposition::OpenOrCreateReadWrite);
        if (regKey.writeBinaryValue(windowPlacementValueName_, getWindowPlacement()))
        {
            const auto headerProperties = listView_.getHeaderProperties();
            regKey.writeBinaryValue(headerPropertiesValueName_, headerProperties);

            ToolBarButtonStates states = toolBar_.isAdjustmentChecked() ? ToolBarButtonStates::Adjustment : ToolBarButtonStates{0};
            states |= statusBar_.isNoHotkeysChecked() ? ToolBarButtonStates::NoHotkeys : ToolBarButtonStates{0};
            states |= statusBar_.isNoLegacyChecked() ? ToolBarButtonStates::NoLegacy : ToolBarButtonStates{0};
            regKey.writeBinaryValue(toolBarButtonStates_, states);
        }

        return std::nullopt; // DefWindowProcW() closes the window and issues a WM_DESTROY message
    }

    [[nodiscard]] std::optional<LRESULT> onDestroy(HWND, UINT, WPARAM, LPARAM)
    {
        registerRawInputDevice(RIDEV_REMOVE);
        PostQuitMessage(0);
        return 0;
    }

    [[nodiscard]] std::optional<LRESULT> dispatchMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
    {
        switch (msg)
        {
            case WM_CREATE:
            {
                return onCreate(hwnd, msg, wParam, lParam);
            }
            case WM_SIZE:
            {
                return onSize(hwnd, msg, wParam, lParam);
            }
            case WM_INPUT:
            {
                return onInput(hwnd, msg, wParam, lParam);
            }
            case WM_COMMAND:
            {
                return onCommand(hwnd, msg, wParam, lParam);
            }
            case WM_NOTIFY:
            {
                return onNotify(hwnd, msg, wParam, lParam);
            }
            case WM_CLOSE:
            {
                return onClose(hwnd, msg, wParam, lParam);
            }
            case WM_DESTROY:
            {
                return onDestroy(hwnd, msg, wParam, lParam);
            }
        }

        return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE)
        {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            auto self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
            self->hwnd_ = hwnd;
            setWindowSubclass(hwnd, self);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

#pragma endregion

#pragma region Child Windows

    class ToolBar final : public Window
    {
    public:
        ToolBar(HINSTANCE hinstance)
            : imageList_{hinstance, ID_TOOLBAR}
        {
        }

        void create(HINSTANCE hinstance, const Window& parent)
        {
            const HMENU hmenu = reinterpret_cast<HMENU>(ID_TOOLBAR);
            const DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS;
            createEx(0, TOOLBARCLASSNAMEW, L"", style, 0, 0, 0, 0, parent.hwnd(), hmenu, hinstance, nullptr);
            setWindowSubclass(hwnd_, this);

            StringResource<32> clearLabel(hinstance, IDS_TBBUTTON_CLEAR);
            StringResource<32> adjustLabel(hinstance, IDS_TBBUTTON_ADJUST);

            // clang-format off
            const TBBUTTON buttons[] =
            {
                {
                    .iBitmap = 2,
                    .fsStyle = TBSTYLE_SEP
                },
                {
                    .iBitmap = 0,
                    .idCommand = ID_CLEAR_LIST_VIEW,
                    .fsState = TBSTATE_ENABLED,
                    .iString = reinterpret_cast<INT_PTR>(clearLabel.str())
                },
                {
                    .iBitmap = 2,
                    .fsStyle = TBSTYLE_SEP
                },
                {
                    .iBitmap = 1,
                    .idCommand = ID_TOGGLE_ADJUSTMENT,
                    .fsState = TBSTATE_CHECKED | TBSTATE_ENABLED,
                    .fsStyle = TBSTYLE_CHECK,
                    .iString = reinterpret_cast<INT_PTR>(adjustLabel.str())
                }
            };
            // clang-format on

            const WPARAM numButtons = std::size(buttons);
            const SIZE iconSize = imageList_.getIconSize();

            sendMessage(TB_SETPADDING, 0, MAKELONG(32, 8));
            sendMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
            sendMessage(TB_SETBITMAPSIZE, 0, static_cast<LPARAM>(MAKELONG(iconSize.cx, iconSize.cy)));
            sendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(imageList_.handle()));
            sendMessage(TB_ADDBUTTONS, numButtons, reinterpret_cast<LPARAM>(buttons));
            sendMessage(TB_AUTOSIZE, 0, 0);
        }

        [[nodiscard]] bool isAdjustmentChecked() const noexcept
        {
            return sendMessage(TB_ISBUTTONCHECKED, ID_TOGGLE_ADJUSTMENT, 0) != 0;
        }

        void setAdjustmentChecked(bool checked) noexcept
        {
            sendMessage(TB_CHECKBUTTON, ID_TOGGLE_ADJUSTMENT, MAKELONG(checked, 0));
        }

    private:
        ImageList imageList_;

        [[nodiscard]] std::optional<LRESULT> dispatchMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
        {
            switch (msg)
            {
                case WM_ERASEBKGND:
                {
                    // The toolbar was created borderless to avoid a double border on the top, left, and right.
                    // We leave it at that but draw a nice edge on the bottom to have a clean
                    // separation between the toolbar and list view.
                    RECT rect{getClientRect()};
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    DrawEdge(hdc, &rect, EDGE_ETCHED, BF_BOTTOM | BF_ADJUST);
                    FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
                    return TRUE;
                }
            }
            return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
        }
    };

    class ListView final : public Window
    {
    public:
        enum class DisplayFormat : long
        {
            Default,
            Dec = IDC_POPUP_DEC,
            Hex = IDC_POPUP_HEX,
            Bin = IDC_POPUP_BIN,
            Sml = IDC_POPUP_SML,
            Ray = IDC_POPUP_RAY,
            Glfw = IDC_POPUP_GLFW
        };

        ListView(HINSTANCE hinstance)
            : smallImageList_{hinstance, ID_LISTVIEW}
        {
        }

        template<UINT COLUMN_DESC_ID, wchar_t TOKEN_SEP = L';', wchar_t GROUP_SEP = L'|'>
        void create(HINSTANCE hinstance, const Window& parent)
        {
            const SIZE clientSize = parent.getClientSize();
            const DWORD style = WS_CHILD | WS_VISIBLE | LVS_REPORT;
            createEx(0, WC_LISTVIEWW, L"", style, 0, 0, clientSize.cx, clientSize.cy, parent.hwnd(), nullptr, hinstance, nullptr);
            setWindowSubclass(hwnd_, this);
            hwndHeader_ = ListView_GetHeader(hwnd_);

            ListView_SetImageList(hwnd_, smallImageList_.handle(), LVSIL_SMALL);
            ListView_SetExtendedListViewStyle(hwnd_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            StringResource columnDesks(hinstance, COLUMN_DESC_ID);
            for (size_t position = 0; std::wstring_view columnDesk : splitAndTrimTrailing(columnDesks.view(), GROUP_SEP))
            {
                const auto [name, widthAndMoreView] = splitOnce(columnDesk, TOKEN_SEP);
                const auto [width, formatAndMoreView] = splitOnce(widthAndMoreView, TOKEN_SEP);
                const auto [format, resIdAndMoreView] = splitOnce(formatAndMoreView, TOKEN_SEP);
                const auto [resId, check] = splitOnce(resIdAndMoreView, TOKEN_SEP);
                insertColumn(position++, name, toUInt(width, 10), toInt(format, 10), toULong(resId, 10), toULong(check, 10));
            }

            hfont_ = reinterpret_cast<HFONT>(sendMessage(WM_GETFONT, 0, 0));
            if (hfont_ == nullptr)
            {
                THROW_LAST_SYSTEM_ERROR();
            }

            LOGFONTW lf{};
            if (GetObjectW(hfont_, sizeof(lf), &lf) == 0)
            {
                THROW_SYSTEM_ERROR(ERROR_INTERNAL_ERROR);
            }

            lf.lfWeight = FW_BOLD;
            hfontBold_ = CreateFontIndirectW(&lf);
            if (hfontBold_ == nullptr)
            {
                THROW_SYSTEM_ERROR(ERROR_INTERNAL_ERROR);
            }
        }

        int insertItem(int position, const RawKeyboard& rawKbd)
        {
            _ASSERT(IsWindow(hwnd_));

            // clang-format off
            LVITEMW item
            {
                .mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM,
                .iItem = static_cast<int>(position),
                .pszText = LPSTR_TEXTCALLBACKW,
                .iImage = rawKbd.isKeyDown ? 0 : 1,
                .lParam = PackedRawKeyboard(rawKbd).lParam
            };
            // clang-format on
            position = ListView_InsertItem(hwnd_, &item);

            const int subItemCount = Header_GetItemCount(hwndHeader_);
            for (int i = 0; i < subItemCount; ++i)
            {
                ListView_SetItemText(hwnd_, position, i + 1, LPSTR_TEXTCALLBACKW);
            }

            return position;
        }

        int insertColumn(size_t position, std::wstring_view name, size_t width, int format, unsigned long resId, unsigned long check)
        {
            _ASSERT(IsWindow(hwnd_));

            TempBuffer<wchar_t> zeroTerminated(name.size() + 1);
            *std::copy_n(name.begin(), name.size(), zeroTerminated.data()) = L'\0';

            // clang-format off
            LVCOLUMNW column
            {
                .mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT,
                .fmt = format,
                .cx = static_cast<int>(width),
                .pszText = zeroTerminated.data()
            };
            // clang-format on
            const int index = ListView_InsertColumn(hwnd_, position, &column);
            if (index >= 0 && resId != 0)
            {
                const LPARAM lParam = MAKELPARAM(resId, check);
                const HDITEM hdi{.mask = HDI_FORMAT | HDI_LPARAM, .fmt = format | HDF_STRING | HDF_SPLITBUTTON, .lParam = lParam};
                Header_SetItem(hwndHeader_, index, &hdi);
            }

            return index;
        }

        bool ensureVisible(int item, bool partialOk) noexcept
        {
            _ASSERT(IsWindow(hwnd_));
            return ListView_EnsureVisible(hwnd_, item, partialOk);
        }

        void deleteAllItems() noexcept
        {
            _ASSERT(IsWindow(hwnd_));
            ListView_DeleteAllItems(hwnd_);
        }

        [[nodiscard]] bool isHeader(HWND hwnd) const noexcept
        {
            if (IsWindow(hwnd) && IsWindow(hwndHeader_))
            {
                return hwndHeader_ == hwnd;
            }
            return false;
        }

        [[nodiscard]] int getItemCount() const noexcept
        {
            _ASSERT(IsWindow(hwnd_));
            return ListView_GetItemCount(hwnd_);
        }

        [[nodiscard]] std::vector<ListViewHeaderProperties> getHeaderProperties() const noexcept
        {
            _ASSERT(IsWindow(hwnd_) && IsWindow(hwndHeader_));
            const int columnCount = Header_GetItemCount(hwndHeader_);

            std::vector<ListViewHeaderProperties> headerProperties;
            headerProperties.reserve(columnCount);

            for (int i = 0; i < columnCount; ++i)
            {
                const auto [_, checkedMenuItem] = getHeaderUserData(i);
                const int width = ListView_GetColumnWidth(hwnd_, i);
                headerProperties.emplace_back(checkedMenuItem, width);
            }

            return headerProperties;
        }

        bool setHeaderProperties(const std::vector<ListViewHeaderProperties>& headerProperties) noexcept
        {
            _ASSERT(IsWindow(hwnd_) && IsWindow(hwndHeader_));
            const int columnCount = Header_GetItemCount(hwndHeader_);

            if (static_cast<int>(headerProperties.size()) < columnCount)
            {
                return false;
            }

            for (int i = 0; i < columnCount; ++i)
            {
                if (!ListView_SetColumnWidth(hwnd_, i, headerProperties[i].width))
                {
                    return false;
                }
                const auto [resourceId, _] = getHeaderUserData(i);
                setHeaderUserData(i, resourceId, headerProperties[i].checkedMenuItemId);
            }

            return true;
        }

        void showSplitButtonMenu(HINSTANCE hinstance, int column) noexcept
        {
            const auto [resourceId, checkedMenuItem] = getHeaderUserData(column);
            PopupMenu splitButtonMenu(hinstance, hwnd_, resourceId);
            splitButtonMenu.checkMenuItem(checkedMenuItem);

            RECT rcItem{};
            Header_GetItemRect(hwndHeader_, column, &rcItem);

            RECT rcDropDown{};
            Header_GetItemDropDownRect(hwndHeader_, column, &rcDropDown);

            POINT position{.x = rcDropDown.left, .y = rcItem.bottom};
            ClientToScreen(hwnd_, &position);

            const UINT flags = TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN;
            switch (const int selectedMenuItem = splitButtonMenu.track(flags, position); selectedMenuItem)
            {
                case IDC_POPUP_BIN:
                case IDC_POPUP_DEC:
                case IDC_POPUP_HEX:
                case IDC_POPUP_SML:
                case IDC_POPUP_RAY:
                case IDC_POPUP_GLFW:
                {
                    setHeaderUserData(column, resourceId, selectedMenuItem);
                    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                    break;
                }
            }
        }

        [[nodiscard]] DisplayFormat getDisplayFormat(int column)
        {
            const auto [resourceId, checkedMenuItem] = getHeaderUserData(column);

            if (resourceId == 0 || checkedMenuItem == 0)
            {
                return DisplayFormat::Default;
            }

            return static_cast<DisplayFormat>(checkedMenuItem);
        }

        [[nodiscard]] HFONT getFont() const noexcept
        {
            return hfont_;
        }

        [[nodiscard]] HFONT getBoldFont() const noexcept
        {
            return hfontBold_;
        }

    private:
        HFONT hfont_{};
        HFONT hfontBold_{};
        HWND hwndHeader_{};
        ImageList smallImageList_;

        [[nodiscard]] std::pair<int, int> getHeaderUserData(int column) const noexcept
        {
            HDITEMW hdi{.mask = HDI_LPARAM};
            Header_GetItem(hwndHeader_, column, &hdi);
            // LOWORD is resource ID, HIWORD is checked menu item
            return {LOWORD(hdi.lParam), HIWORD(hdi.lParam)};
        }

        void setHeaderUserData(int column, int resourceId, int checkedMenuItem) noexcept
        {
            HDITEMW hdi{.mask = HDI_LPARAM};
            hdi.lParam = MAKELPARAM(resourceId, checkedMenuItem);
            Header_SetItem(hwndHeader_, column, &hdi);
        }

        [[nodiscard]] std::optional<LRESULT> dispatchMessage(HWND, UINT msg, WPARAM, LPARAM) override
        {
            if (msg == WM_DESTROY && hfontBold_ != nullptr)
            {
                DeleteObject(std::exchange(hfontBold_, nullptr));
            }
            return std::nullopt;
        }
    };

    class StatusBar final : public Window
    {
    public:
        StatusBar(HINSTANCE hinstance)
            : toolBar_{hinstance}
        {
        }

        void create(HINSTANCE hinstance, const Window& parent)
        {
            StringResource help(hinstance, IDS_STATUS_BAR_HELP_TEXT);
            const DWORD style = WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | CCS_NOPARENTALIGN;
            createEx(0, STATUSCLASSNAMEW, help.str(), style, 0, 0, 0, 0, parent.hwnd(), nullptr, hinstance, nullptr);
            setWindowSubclass(hwnd_, this);

            toolBar_.create(hinstance, *this);
        }

        bool isNoHotkeysChecked() const noexcept
        {
            return toolBar_.sendMessage(TB_ISBUTTONCHECKED, ID_NOHOTKEYS, 0) != 0;
        }

        void setNoHotkeysChecked(bool checked) noexcept
        {
            toolBar_.sendMessage(TB_CHECKBUTTON, ID_NOHOTKEYS, MAKELONG(checked, 0));
        }

        bool isNoLegacyChecked() const noexcept
        {
            return toolBar_.sendMessage(TB_ISBUTTONCHECKED, ID_NOLEGACY, 0) != 0;
        }

        void setNoLegacyChecked(bool checked) noexcept
        {
            toolBar_.sendMessage(TB_CHECKBUTTON, ID_NOLEGACY, MAKELONG(checked, 0));
        }

        bool isToolBar(HWND hwnd) const noexcept
        {
            return toolBar_.isSame(hwnd);
        }

    private:
        class StatusToolBar final : public Window
        {
        public:
            StatusToolBar(HINSTANCE hinstance)
                : imageList_{hinstance, ID_STATUS_TOOLBAR}
            {
            }

            void create(HINSTANCE hinstance, const Window& parent)
            {
                const HMENU hmenu = reinterpret_cast<HMENU>(ID_STATUS_TOOLBAR);
                const DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | TBSTYLE_TRANSPARENT | CCS_NODIVIDER;
                createEx(0, TOOLBARCLASSNAMEW, nullptr, style, 0, 0, 0, 0, parent.hwnd(), hmenu, hinstance, nullptr);
                setWindowSubclass(hwnd_, this);

                // clang-format off
                const TBBUTTON buttons[] =
                {
                    {
                        .iBitmap = 0,
                        .idCommand = ID_NOHOTKEYS,
                        .fsState = TBSTATE_CHECKED | TBSTATE_ENABLED,
                        .fsStyle = TBSTYLE_CHECK,
                    },
                    {
                        .iBitmap = 2,
                        .fsStyle = TBSTYLE_SEP
                    },
                    {
                        .iBitmap = 1,
                        .idCommand = ID_NOLEGACY,
                        .fsState = TBSTATE_CHECKED | TBSTATE_ENABLED,
                        .fsStyle = TBSTYLE_CHECK,
                    }
                };
                // clang-format on

                const WPARAM numButtons = std::size(buttons);
                const SIZE iconSize = imageList_.getIconSize();

                sendMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
                sendMessage(TB_SETBITMAPSIZE, 0, static_cast<LPARAM>(MAKELONG(iconSize.cx, iconSize.cy)));
                sendMessage(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(imageList_.handle()));
                sendMessage(TB_ADDBUTTONS, numButtons, reinterpret_cast<LPARAM>(buttons));
            }

            // Due to CCS_NOPARENTALIGN, the size needs to be calculated "by hand"
            SIZE getButtonAreaSize() const
            {
                int count = static_cast<int>(sendMessage(TB_BUTTONCOUNT, 0, 0));
                if (count == 0)
                {
                    return {0, 0};
                }

                RECT totalRect{};
                for (int i = 0; i < count; ++i)
                {
                    RECT buttonRect{};
                    if (sendMessage(TB_GETITEMRECT, i, reinterpret_cast<LPARAM>(&buttonRect)))
                    {
                        if (i == 0)
                        {
                            totalRect = buttonRect;
                        }
                        else
                        {
                            totalRect.left = std::min(totalRect.left, buttonRect.left);
                            totalRect.top = std::min(totalRect.top, buttonRect.top);
                            totalRect.right = std::max(totalRect.right, buttonRect.right);
                            totalRect.bottom = std::max(totalRect.bottom, buttonRect.bottom);
                        }
                    }
                }
                return {totalRect.right - totalRect.left, totalRect.bottom - totalRect.top};
            }

        private:
            ImageList imageList_;
        } toolBar_;

        [[nodiscard]] std::optional<LRESULT> dispatchMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override
        {
            switch (msg)
            {
                case WM_NCCALCSIZE:
                {
                    return 0;
                }
                case WM_ERASEBKGND:
                {
                    // Themed status bars draw a border regardless of the window style flags set or not.
                    // So rather than having a discolored border, we draw the top edge using the same
                    // style we are using on the top toolbar, which gives a more symmetrical look.
                    RECT rect{getClientRect()};
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    DrawEdge(hdc, &rect, EDGE_ETCHED, BF_TOP | BF_ADJUST);
                    FillRect(hdc, &rect, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
                    return TRUE;
                }
                case WM_SIZE:
                {
                    // The toolbar should be right-aligned but should not override the gripper,
                    // hence the "- size.cy / 2" when calculating the left position of the toolbar.
                    const SIZE size{getClientSize()};
                    const SIZE tbArea{toolBar_.getButtonAreaSize()};
                    const int left = size.cx - tbArea.cx - size.cy / 2;
                    toolBar_.move(left, (size.cy - tbArea.cy) / 2 + 1, tbArea.cx, tbArea.cy, TRUE);
                    break;
                }
                case WM_COMMAND:
                {
                    return SendMessageW(GetParent(hwnd_), msg, wParam, lParam);
                }
            }
            return std::nullopt; // Let DefWindowProcW() deal with unhandled messages
        }
    };

#pragma endregion

#pragma region Registry

    [[nodiscard]] static std::wstring_view queryVersionResourceStringValue(void* buffer, std::wstring_view subBlock, const wchar_t* key) noexcept
    {
        UINT sizeInChars = 0;
        wchar_t* strValue = nullptr;
        const std::wstring fullSubBlock = std::format(L"{}\\{}", subBlock, key);
        if (!VerQueryValueW(buffer, fullSubBlock.c_str(), reinterpret_cast<LPVOID*>(&strValue), &sizeInChars))
        {
            return {};
        }
        if (sizeInChars == 0)
        {
            return {};
        }

        return {strValue, sizeInChars - 1};
    }

    // Constructs the registry key path for RawInputViewer. This function
    // can fail silently, in which case the returned string is empty
    // and all properties read or written will be ignored.
    [[nodiscard]] static std::wstring constructRegistryKeyPath(HINSTANCE hinstance) noexcept
    {
        TempBuffer<wchar_t, MAX_PATH> path{MAX_PATH};
        while (true)
        {
            const DWORD copied = ::GetModuleFileNameW(hinstance, path.data(), static_cast<DWORD>(path.elements()));
            if (copied == 0 || path.elements() > std::numeric_limits<unsigned short>::max())
            {
                return {};
            }

            if (copied >= path.elements() - 1)
            {
                path.resize(path.elements() * 2);
                continue;
            }

            break;
        }

        DWORD handle = 0;
        const DWORD versionSize = GetFileVersionInfoSizeW(path.data(), &handle);
        if (versionSize == 0)
        {
            return {};
        }

        // Since version info size is typically > 1k, the buffer is always dynamically allocated,
        // which is fine as the class is instantiated at startup.
        TempBuffer<void, 1> buffer(versionSize);
        if (!GetFileVersionInfoW(path.data(), 0, static_cast<DWORD>(buffer.size()), buffer.data()))
        {
            return {};
        }

        // First get the major and minor product version
        UINT fileInfoSize = 0;
        VS_FIXEDFILEINFO* fileInfo{};
        if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<LPVOID*>(&fileInfo), &fileInfoSize))
        {
            return {};
        }
        const DWORD majorVersion = (fileInfo->dwProductVersionMS >> 16) & 0xffff;
        const DWORD minorVersion = fileInfo->dwProductVersionMS & 0xffff;

        // Query translation in case the version resource is localized
        struct Translation
        {
            WORD language;
            WORD codePage;
        }* translation = nullptr;
        UINT translationSize = 0;
        // Try to get the first translation
        if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&translation), &translationSize))
        {
            return {};
        }
        if (translationSize < sizeof(Translation))
        {
            return {};
        }
        const std::wstring subBlock = std::format(L"\\StringFileInfo\\{:04x}{:04x}", translation->language, translation->codePage);

        const std::wstring_view companyName = queryVersionResourceStringValue(buffer.data(), subBlock, L"CompanyName");
        if (companyName.empty())
        {
            return {};
        }

        const std::wstring_view productName = queryVersionResourceStringValue(buffer.data(), subBlock, L"ProductName");
        if (productName.empty())
        {
            return {};
        }

        // The registry key path looks like "HKEY_CURRENT_USER\Software\<CompanyName>\<ProductName>\<major version>.<minor version>"
        return std::format(L"Software\\{}\\{}\\{}.{}", companyName, productName, majorVersion, minorVersion);
    }

    enum class RegKeyDisposition
    {
        OpenReadOnly,
        OpenOrCreateReadWrite
    };

    [[nodiscard]] CurrentUserRegKey getAppRegKey(RegKeyDisposition disposition)
    {
        if (registryKeyPath_.empty())
        {
            return {};
        }

        switch (disposition)
        {
            case RegKeyDisposition::OpenReadOnly:
            {
                HKEY hkey{};
                LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, registryKeyPath_.c_str(), 0, KEY_QUERY_VALUE, &hkey);
                return status == ERROR_SUCCESS ? CurrentUserRegKey(hkey) : CurrentUserRegKey();
            }
            case RegKeyDisposition::OpenOrCreateReadWrite:
            {
                HKEY hkey{};
                LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, registryKeyPath_.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hkey, nullptr);
                return status == ERROR_SUCCESS ? CurrentUserRegKey(hkey) : CurrentUserRegKey();
            }
            default:
            {
                std::unreachable();
            }
        }
    }

#pragma endregion

    bool registerRawInputDevice(DWORD flags) noexcept
    {
        // clang-format off
        const RAWINPUTDEVICE rid[]
        {
            {
                .usUsagePage = 0x01,
                .usUsage = 0x06,     // Keyboard
                .dwFlags = flags,
                .hwndTarget = hwnd_
            },
            {
                .usUsagePage = 0x01,
                .usUsage = 0x02,     // Mouse
                .dwFlags = 0,
                .hwndTarget = hwnd_
            }
        };
        // clang-format on
        return RegisterRawInputDevices(rid, std::size(rid), sizeof(RAWINPUTDEVICE));
    }

    bool registerRawInputDevice() noexcept
    {
        DWORD flags = statusBar_.isNoHotkeysChecked() ? 0 : RIDEV_NOHOTKEYS;
        flags |= statusBar_.isNoHotkeysChecked() ? 0 : RIDEV_NOLEGACY;
        return registerRawInputDevice(flags);
    }

    ToolBar toolBar_;
    ListView listView_;
    StatusBar statusBar_;
    const HINSTANCE hinstance_;
    const std::wstring registryKeyPath_;
    std::map<USHORT, KeyCodes> scanCodeMapping_;
    ScanCodeSequence pendingSequence_{ScanCodeSequence::None};
    std::map<USHORT, std::pair<std::wstring, std::wstring>> vkeyMapping_;
    static constexpr wchar_t toolBarButtonStates_[] = L"ToolbarButtonStates";
    static constexpr wchar_t windowPlacementValueName_[] = L"WindowPlacement";
    static constexpr wchar_t headerPropertiesValueName_[] = L"HeaderProperties";

public:
    MainWindow(HINSTANCE hinstance, int showCmd)
        : toolBar_{hinstance}
        , listView_{hinstance}
        , statusBar_{hinstance}
        , hinstance_{hinstance}
        , registryKeyPath_{constructRegistryKeyPath(hinstance)}
    {
        INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
        if (!InitCommonControlsEx(&icex))
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        StringResource className(hinstance_, IDS_APP);
        // clang-format off
        const WNDCLASSEXW wc
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = windowProc,
            .hInstance = hinstance_,
            .hIcon = LoadIconW(hinstance_, MAKEINTRESOURCE(IDI_APP)),
            .hCursor = LoadCursorW(hinstance_, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
            .lpszClassName = className.str()
        };
        const ATOM wndClass = RegisterClassExW(&wc);
        if (wndClass == 0)
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        StringResource<128> appTitle(hinstance_, IDS_APP_TITLE);
        createEx
        (
            WS_EX_APPWINDOW,
            className.str(),
            appTitle.str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            nullptr,
            nullptr,
            hinstance_,
            this
        );
        // clang-format on

        // Restore the last window position, size, list view column widths, and selected view type
        CurrentUserRegKey regKey = getAppRegKey(RegKeyDisposition::OpenReadOnly);
        const auto windowPlacement = regKey.readBinaryValue<WINDOWPLACEMENT>(windowPlacementValueName_, getWindowPlacement());
        if (SetWindowPlacement(hwnd_, &windowPlacement))
        {
            // Restore list view column width and view type
            const auto headerProperties = regKey.readBinaryValue(headerPropertiesValueName_, listView_.getHeaderProperties());
            listView_.setHeaderProperties(headerProperties);

            ToolBarButtonStates states = ToolBarButtonStates::Adjustment;
            states = regKey.readBinaryValue(toolBarButtonStates_, states);
            toolBar_.setAdjustmentChecked((states & ToolBarButtonStates::Adjustment) != ToolBarButtonStates{0});
            statusBar_.setNoHotkeysChecked((states & ToolBarButtonStates::NoHotkeys) != ToolBarButtonStates{0});
            statusBar_.setNoLegacyChecked((states & ToolBarButtonStates::NoLegacy) != ToolBarButtonStates{0});
        }

        const std::string scanCodeMapping = loadText(hinstance_, ID_SCANCODE_MAPPING);
        for (std::string_view mappingView : splitAndTrimTrailing(scanCodeMapping, '\n'))
        {
            const auto [scanCodeView, codeAndMoreView] = splitOnce(mappingView, '=');
            const auto [keyCodeView, smlAndMoreView] = splitOnce(codeAndMoreView, ',');
            const auto [smlView, raylibAndMoreView] = splitOnce(smlAndMoreView, ',');
            const auto [raylibView, glfwView] = splitOnce(raylibAndMoreView, ',');
            scanCodeMapping_.emplace(toUShort(scanCodeView, 16), KeyCodes{toInt(keyCodeView, 10), smlView, raylibView, glfwView});
        }

        const std::string vkeyMapping = loadText(hinstance_, ID_VIRTUAL_KEY_MAPPING);
        for (std::string_view mappingView : splitAndTrimTrailing(vkeyMapping, '\n'))
        {
            const auto [keyView, valView] = splitOnce(mappingView, '=');
            const auto [vkNameView, kNameView] = splitOnce(valView, ',');
            vkeyMapping_.emplace(toUShort(keyView, 16), std::pair{toWString(vkNameView), toWString(kNameView)});
        }

        if (!registerRawInputDevice())
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        ShowWindow(hwnd_, showCmd);
        UpdateWindow(hwnd_);
    }
};

END_ANONYMOUS_NAMESPACE

int WINAPI wWinMain(_In_ HINSTANCE hinstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int showCmd)
{
    try
    {
        MainWindow mainWindow(hinstance, showCmd);

        for (Msg msg; msg.getMessage();)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    catch (const std::exception& ex)
    {
        FatalAppExitA(0, ex.what());
    }

    return EXIT_SUCCESS;
}
