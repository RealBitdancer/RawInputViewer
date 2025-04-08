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

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOAUDIO
#define NOCOMM
#define NOIME
#define NOMINMAX
#define NOOPENGL

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <strsafe.h>

#include <format>
#include <optional>
#include <ranges>
#include <vector>

// clang-format off
#define BEGIN_ANONYMOUS_NAMESPACE namespace {
#define END_ANONYMOUS_NAMESPACE }
// clang-format on

#define STRINGIZE_DETAIL(s) #s
#define STRINGIZE(s) STRINGIZE_DETAIL(s)

#ifdef _DEBUG

#define FORMATED_FILE_AND_LINE __FILE__ "(" STRINGIZE(__LINE__) ")"
#define SYSTEM_ERROR(e) std::system_error(std::error_code(e, std::system_category()), FORMATED_FILE_AND_LINE)

#else // !_DEBUG

#define SYSTEM_ERROR(e) std::system_error(std::error_code(e, std::system_category()))

#endif // _DEBUG

#define THROW_SYSTEM_ERROR(e) throw SYSTEM_ERROR(e)
#define THROW_LAST_SYSTEM_ERROR() THROW_SYSTEM_ERROR(::GetLastError())

namespace concepts
{
    // clang-format off
    template<typename T>
    concept Standard =
        std::is_standard_layout_v<T> &&
        std::is_trivially_copyable_v<T> &&
        !std::is_pointer_v<T>;

    template<typename T>
    concept Trivial =
        Standard<T> &&
        std::is_trivial_v<T> &&
        !std::is_polymorphic_v<T>;

    template<typename T>
    concept VoidOrTrivial =
        std::same_as<std::remove_cv_t<T>, void> ||
        Trivial<T>;

    template<typename T>
    concept Char =
        std::same_as<std::remove_cv_t<T>, char>;

    template<typename T>
    concept WChar =
        std::same_as<std::remove_cv_t<T>, wchar_t>;

    template<typename T>
    concept CharOrWChar =
        Char<T> ||
        WChar<T>;

    template<typename R>
    concept CharContiguousRange =
        std::ranges::contiguous_range<R> &&
        Char<std::ranges::range_value_t<R>>;

    template<typename R>
    concept WCharContiguousRange =
        std::ranges::contiguous_range<R> &&
        WChar<std::ranges::range_value_t<R>>;

    template<typename R>
    concept CharOrWCharContiguousRange =
        CharContiguousRange<R> ||
        WCharContiguousRange<R>;
    // clang-format on
} // namespace concepts

template<typename T>
    requires(std::is_enum_v<T> && requires(T e) { enableBitmaskOperatorOr(e); })
constexpr auto operator|(const T lhs, const T rhs)
{
    return static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<typename T>
    requires(std::is_enum_v<T> && requires(T e) { enableBitmaskOperatorOrAssign(e); })
constexpr T& operator|=(T& lhs, const T rhs)
{
    lhs = static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
    return lhs;
}

template<typename T>
    requires(std::is_enum_v<T> && requires(T e) { enableBitmaskOperatorAnd(e); })
constexpr auto operator&(const T lhs, const T rhs)
{
    return static_cast<T>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template<concepts::CharOrWChar CharType = char>
[[nodiscard]] constexpr bool isWhitespace(CharType ch) noexcept
{
    if constexpr (concepts::WChar<CharType>)
    {
        return iswspace(ch);
    }
    else
    {
        return isspace(static_cast<int>(static_cast<unsigned char>(ch)));
    }
}

template<concepts::VoidOrTrivial T, size_t NElements = 256>
class TempBuffer final
{
public:
    static_assert(NElements > 0, "NElements must be greater than zero");
    static_assert(!std::is_void_v<T>, "Use TempBuffer<void, N> for void specialization");

    // Non-copyable and non-movable
    TempBuffer(const TempBuffer&) = delete;
    TempBuffer& operator=(const TempBuffer&) = delete;

    TempBuffer() noexcept = default;

    inline TempBuffer(size_t elements)
    {
        resize(elements);
    }

    // Resizes the buffer; existing content will be lost
    void resize(size_t elements)
    {
        dynamic_.reset();
        elements_ = elements;

        if (elements_ <= NElements)
        {
            p_ = reinterpret_cast<T*>(static_);
        }
        else
        {
            dynamic_ = std::make_unique_for_overwrite<T[]>(elements_);
            p_ = dynamic_.get();
        }
    }

    // For default constructed TempBuffers, data() will return a pointer to static_[]
    [[nodiscard]] inline T* data() const noexcept
    {
        return p_;
    }

    // Number of T elements in the buffer
    [[nodiscard]] inline size_t elements() const noexcept
    {
        return elements_;
    }

    // Size of buffer in bytes
    [[nodiscard]] inline size_t size() const noexcept
    {
        return elements_ * sizeof(T);
    }

    // Capacity in Ts
    [[nodiscard]] inline size_t capacity() const noexcept
    {
        return p_ == reinterpret_cast<const T*>(static_) ? NElements : elements_;
    }

    [[nodiscard]] inline bool isDynamic() const noexcept
    {
        return dynamic_ && p_ == dynamic_.get();
    }

private:
    [[nodiscard]] static consteval size_t alignment()
    {
        return std::max(alignof(std::max_align_t), alignof(T));
    }

    T* p_{reinterpret_cast<T*>(static_)};
    size_t elements_{};
    std::unique_ptr<T[]> dynamic_;
    [[maybe_unused]] alignas(alignment()) std::byte static_[NElements * sizeof(T)]; // Buffer contents is intentionally uninitialized
};

// Specialization for T == void, just for convenience
template<size_t NBytes>
class TempBuffer<void, NBytes> final
{
public:
    inline TempBuffer(size_t bytes)
        : buffer_(bytes)
    {
    }

    inline void resize(size_t bytes)
    {
        buffer_.resize(bytes);
    }

    [[nodiscard]] inline void* data() const noexcept
    {
        return static_cast<void*>(buffer_.data());
    }

    [[nodiscard]] inline size_t size() const noexcept
    {
        return buffer_.size();
    }

    [[nodiscard]] inline size_t elements() const noexcept
    {
        return buffer_.elements();
    }

    [[nodiscard]] inline size_t capacity() const noexcept
    {
        return buffer_.capacity();
    }

    [[nodiscard]] inline bool isDynamic() const noexcept
    {
        return buffer_.isDynamic();
    }

private:
    TempBuffer<std::byte, NBytes> buffer_;
};

template<size_t NChars = 64>
class StringResource
{
public:
    static_assert(NChars > 0, "NChars must be greater than zero");

    StringResource(HINSTANCE hinstance, UINT resourceId)
    {
        // LoadStringW (unlike LoadStringA), when called with cchBufferMax == 0, returns a pointer (buffer)
        // directly to the resource in .rsrc. Assuming the static buffer of StringResource is large enough,
        // this means the only performance overhead of creating a new instance of StringResource is
        // the cost of copying the string from .rsrc to the static buffer of StringResource.
        wchar_t* buffer = nullptr;
        const int length = LoadStringW(hinstance, resourceId, reinterpret_cast<wchar_t*>(&buffer), 0);
        if (length <= 0)
        {
            THROW_LAST_SYSTEM_ERROR();
        }
        buffer_.resize(length + 1);
        *std::copy_n(buffer, length, buffer_.data()) = L'\0';
    }

    [[nodiscard]] inline wchar_t* str() const noexcept
    {
        return buffer_.data();
    }

    [[nodiscard]] inline size_t length() const noexcept
    {
        return buffer_.elements() - 1;
    }

    [[nodiscard]] inline size_t capacity() const noexcept
    {
        return buffer_.capacity();
    }

    [[nodiscard]] inline std::wstring_view view() const noexcept
    {
        return {buffer_.data(), length()};
    }

    inline void copyTo(std::span<wchar_t> dst) const noexcept
    {
        StringCchCopyNW(dst.data(), dst.size(), buffer_.data(), buffer_.elements());
    }

    inline void copyTo(wchar_t* dst, size_t dstSizeInChars) const noexcept
    {
        copyTo({dst, dstSizeInChars});
    }

private:
    TempBuffer<wchar_t, NChars> buffer_;
};

template<concepts::CharOrWChar CharType = char>
[[nodiscard]] std::basic_string<CharType> loadText(HINSTANCE hinstance, UINT resourceId)
{
    HRSRC hres = FindResourceW(hinstance, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (hres == nullptr)
    {
        THROW_LAST_SYSTEM_ERROR();
    }

    HGLOBAL hdata = LoadResource(hinstance, hres);
    if (hdata == nullptr)
    {
        THROW_LAST_SYSTEM_ERROR();
    }

    auto data = static_cast<const CharType*>(LockResource(hdata));
    const DWORD size = SizeofResource(hinstance, hres);
    if (size == 0)
    {
        THROW_LAST_SYSTEM_ERROR();
    }

    const size_t length = size / sizeof(CharType);
    return std::basic_string<CharType>(data, length);
}

template<concepts::CharContiguousRange R>
[[nodiscard]] std::wstring toWString(R&& range)
{
    auto mbcs = reinterpret_cast<const char*>(std::ranges::data(range));
    const int count = MultiByteToWideChar(CP_UTF8, 0, mbcs, static_cast<int>(range.size()), nullptr, 0);
    if (count == 0)
    {
        return {};
    }

    std::wstring wcs;
    wcs.resize_and_overwrite(
        count,
        [&](wchar_t* wcs, size_t wcsLength) noexcept
        {
            const int result = MultiByteToWideChar(CP_UTF8, 0, mbcs, static_cast<int>(range.size()), wcs, static_cast<int>(wcsLength));
            return result > 0 ? wcsLength : size_t{0};
        });

    return wcs;
}

// toIntegralNumber() might silently fail and return zero.
// This is acceptable and does not warrant a fatal exception.
template<std::integral T, concepts::CharOrWCharContiguousRange R>
[[nodiscard]] T toIntegralNumber(R&& range, int base)
{
    if constexpr (concepts::WChar<std::ranges::range_value_t<R>>)
    {
        TempBuffer<wchar_t, 128> zeroTerminated(range.size() + 1);
        *std::copy_n(range.begin(), range.size(), zeroTerminated.data()) = L'\0';
        return static_cast<T>(std::wcstoll(zeroTerminated.data(), nullptr, base));
    }
    else
    {
        TempBuffer<char, 128> zeroTerminated(range.size() + 1);
        *std::copy_n(range.begin(), range.size(), zeroTerminated.data()) = '\0';
        return static_cast<T>(std::strtoll(zeroTerminated.data(), nullptr, base));
    }
}

template<concepts::CharOrWCharContiguousRange R>
[[nodiscard]] unsigned long toULong(R&& range, int base)
{
    return toIntegralNumber<unsigned long>(range, base);
}

template<concepts::CharOrWCharContiguousRange R>
[[nodiscard]] int toInt(R&& range, int base)
{
    return toIntegralNumber<int>(range, base);
}

template<concepts::CharOrWCharContiguousRange R>
[[nodiscard]] unsigned int toUInt(R&& range, int base)
{
    return toIntegralNumber<unsigned int>(range, base);
}

template<concepts::CharOrWCharContiguousRange R>
[[nodiscard]] unsigned short toUShort(R&& range, int base)
{
    return toIntegralNumber<unsigned short>(range, base);
}

template<concepts::CharOrWChar CharType = char, concepts::CharOrWCharContiguousRange R>
constexpr auto splitAndTrimTrailing(R&& text, CharType separator) noexcept
{
    auto splitView = text | std::views::split(separator);
    return splitView |
           std::views::transform(
               [](auto subrange)
               {
                   auto begin = subrange.begin();
                   auto end = subrange.end();
                   while (end != begin && isWhitespace<CharType>(*(end - 1)))
                   {
                       --end;
                   }
                   return std::basic_string_view<CharType>(begin, end);
               }) |
           std::views::filter([](std::basic_string_view<CharType> sv) { return !sv.empty(); });
}

template<concepts::CharOrWChar CharType = char, concepts::CharOrWCharContiguousRange R>
[[nodiscard]] constexpr std::pair<std::basic_string_view<CharType>, std::basic_string_view<CharType>> splitOnce(R&& text, CharType separator) noexcept
{
    if (text.empty())
    {
        return {std::basic_string_view<CharType>{}, std::basic_string_view<CharType>{}};
    }

    auto pos = text.find(separator);
    if (pos == std::basic_string_view<CharType>::npos)
    {
        return {text, std::basic_string_view<CharType>{}};
    }

    return {text.substr(0, pos), text.substr(pos + 1)};
}

class CurrentUserRegKey
{
public:
    // No copying/move assignment
    CurrentUserRegKey(const CurrentUserRegKey&) = delete;
    CurrentUserRegKey& operator=(const CurrentUserRegKey&) = delete;
    CurrentUserRegKey& operator=(CurrentUserRegKey&&) = delete;

    // Move construction only
    CurrentUserRegKey(CurrentUserRegKey&& other) noexcept
        : hkey_{other.hkey_}
    {
        other.hkey_ = nullptr;
    }

    CurrentUserRegKey() noexcept = default;

    CurrentUserRegKey(HKEY hkey)
        : hkey_{hkey}
    {
    }

    template<concepts::Standard T>
    [[nodiscard]] T readBinaryValue(const wchar_t* valueName, const T& defaultVal) const noexcept
    {
        if (hkey_ == nullptr)
        {
            return defaultVal;
        }

        T val{};
        DWORD type = REG_BINARY;
        DWORD size = static_cast<DWORD>(sizeof(val));
        LSTATUS status = RegQueryValueExW(hkey_, valueName, nullptr, &type, reinterpret_cast<PBYTE>(&val), &size);
        if (status != ERROR_SUCCESS || size != sizeof(val))
        {
            return defaultVal;
        }

        return val;
    }

    template<concepts::Standard T>
    [[nodiscard]] std::vector<T> readBinaryValue(const wchar_t* valueName, const std::vector<T>& defaultVal) const noexcept
    {
        if (hkey_ == nullptr)
        {
            return defaultVal;
        }

        DWORD size = 0;
        DWORD type = REG_BINARY;
        LSTATUS status = RegQueryValueExW(hkey_, valueName, nullptr, &type, nullptr, &size);
        if (status != ERROR_SUCCESS || size == 0 || size % sizeof(T) != 0)
        {
            return defaultVal;
        }

        size_t elementCount = size / sizeof(T);
        std::vector<T> val;
        val.resize(elementCount);

        // Read the actual data
        status = RegQueryValueExW(hkey_, valueName, nullptr, &type, reinterpret_cast<PBYTE>(val.data()), &size);
        if (status != ERROR_SUCCESS)
        {
            return defaultVal;
        }

        return val;
    }

    template<concepts::Standard T>
    bool writeBinaryValue(const wchar_t* valueName, const T& val) noexcept
    {
        if (hkey_ == nullptr)
        {
            return false;
        }

        DWORD type = REG_BINARY;
        DWORD size = static_cast<DWORD>(sizeof(T));
        return RegSetValueExW(hkey_, valueName, 0, type, reinterpret_cast<const BYTE*>(&val), size) == ERROR_SUCCESS;
    }

    template<concepts::Standard T>
    bool writeBinaryValue(const wchar_t* valueName, const std::vector<T>& val) noexcept
    {
        if (hkey_ == nullptr)
        {
            return false;
        }

        DWORD type = REG_BINARY;
        DWORD size = static_cast<DWORD>(val.size() * sizeof(T));
        return RegSetValueExW(hkey_, valueName, 0, type, reinterpret_cast<const BYTE*>(val.data()), size) == ERROR_SUCCESS;
    }

    ~CurrentUserRegKey()
    {
        if (hkey_ != nullptr)
        {
            RegCloseKey(hkey_);
        }
    }

private:
    HKEY hkey_{};
};

class PopupMenu
{
public:
    PopupMenu(const PopupMenu&) = delete;
    PopupMenu& operator=(const PopupMenu&) = delete;

    PopupMenu(HINSTANCE hinstance, HWND parent, int resourceId)
        : parent_{parent}
        , hmenu_{LoadMenuW(hinstance, MAKEINTRESOURCEW(resourceId))}
    {
        if (hmenu_ == nullptr)
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        subMenu_ = GetSubMenu(hmenu_, 0);
        const DWORD le = GetLastError();
        if (subMenu_ == nullptr)
        {
            DestroyMenu(hmenu_);
            THROW_SYSTEM_ERROR(le);
        }
    }

    void checkMenuItem(int index) const noexcept
    {
        CheckMenuItem(subMenu_, index, MF_BYCOMMAND | MF_CHECKED);
    }

    int track(UINT flags, const POINT& position) const noexcept
    {
        return TrackPopupMenu(subMenu_, flags, position.x, position.y, 0, parent_, nullptr);
    }

    ~PopupMenu()
    {
        DestroyMenu(hmenu_);
    }

private:
    HWND parent_;
    HMENU hmenu_;
    HMENU subMenu_{};
};

class Bitmap
{
public:
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;

    Bitmap(HINSTANCE hinstance, UINT bitmapId)
        : bitmap_{LoadBitmapW(hinstance, MAKEINTRESOURCE(bitmapId))}
    {
        if (bitmap_ == nullptr)
        {
            THROW_LAST_SYSTEM_ERROR();
        }
    }

    ~Bitmap()
    {
        DeleteObject(bitmap_);
    }

    [[nodiscard]] HBITMAP handle() const noexcept
    {
        return bitmap_;
    }

    [[nodiscard]] SIZE getSize() const
    {
        BITMAP bitmapInfo;
        if (GetObjectW(bitmap_, sizeof(BITMAP), &bitmapInfo) == 0)
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        return {.cx = bitmapInfo.bmWidth, .cy = bitmapInfo.bmHeight};
    }

private:
    HBITMAP bitmap_;
};

class ImageList
{
public:
    ImageList(const ImageList&) = delete;
    ImageList& operator=(const ImageList&) = delete;

    ImageList(HINSTANCE hinstance, UINT bitmapId)
    {
        const Bitmap bitmap(hinstance, bitmapId);
        const SIZE size = bitmap.getSize();

        imageList_ = ImageList_Create(size.cy, size.cy, ILC_COLOR32 | ILC_MASK, size.cx / size.cy, 0);
        if (imageList_ == nullptr)
        {
            THROW_LAST_SYSTEM_ERROR();
        }

        ImageList_Add(imageList_, bitmap.handle(), nullptr);
    }

    [[nodiscard]] HIMAGELIST handle() const noexcept
    {
        return imageList_;
    }

    SIZE getIconSize() const noexcept
    {
        _ASSERT(imageList_ != nullptr);
        int cx = 0;
        int cy = 0;
        ImageList_GetIconSize(imageList_, &cx, &cy);
        return {cx, cy};
    }

    ~ImageList()
    {
        ImageList_Destroy(imageList_);
    }

private:
    HIMAGELIST imageList_{};
};

class Window
{
public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window() noexcept = default;

    virtual ~Window() noexcept
    {
        if (IsWindow(hwnd_))
        {
            DestroyWindow(hwnd_);
        }
    }

    [[nodiscard]] HWND hwnd() const noexcept
    {
        return hwnd_;
    }

    LRESULT sendMessage(UINT msg, WPARAM wParam, LPARAM lParam) const noexcept
    {
        _ASSERT(IsWindow(hwnd_));
        return SendMessageW(hwnd_, msg, wParam, lParam);
    }

    [[nodiscard]] RECT getRect() const noexcept
    {
        RECT rect{};
        _ASSERT(IsWindow(hwnd_));
        GetWindowRect(hwnd_, &rect);
        return rect;
    }

    [[nodiscard]] SIZE getSize() const noexcept
    {
        RECT rect{getRect()};
        return {rect.right - rect.left, rect.bottom - rect.top};
    }

    [[nodiscard]] int getHeight() const noexcept
    {
        return getSize().cy;
    }

    [[nodiscard]] RECT getClientRect() const noexcept
    {
        RECT rect{};
        _ASSERT(IsWindow(hwnd_));
        GetClientRect(hwnd_, &rect);
        return rect;
    }

    [[nodiscard]] SIZE getClientSize() const noexcept
    {
        RECT rect{getClientRect()};
        return {rect.right - rect.left, rect.bottom - rect.top};
    }

    bool move(int x, int y, int width, int height, bool repaint) noexcept
    {
        _ASSERT(IsWindow(hwnd_));
        return MoveWindow(hwnd_, x, y, width, height, repaint);
    }

    [[nodiscard]] bool isSame(HWND hwnd) const noexcept
    {
        if (IsWindow(hwnd) && IsWindow(hwnd_))
        {
            return hwnd_ == hwnd;
        }
        return false;
    }

protected:
    HWND hwnd_{};

    // clang-format off
    void createEx(
        DWORD exStyle,
        LPCWSTR className,
        LPCWSTR windowName,
        DWORD style,
        int x,
        int y,
        int width,
        int height,
        HWND parent,
        HMENU hmenu,
        HINSTANCE hinstance,
        LPVOID userData)
    {
        HWND hwnd = CreateWindowExW(exStyle, className, windowName, style, x, y, width, height, parent, hmenu, hinstance, userData);
        if (hwnd == nullptr)
        {
            THROW_LAST_SYSTEM_ERROR();
        }
        // hwnd_ might have been set by a subclass proc (such as for MainWindow)
        if (hwnd != hwnd_)
        {
            hwnd_ = hwnd;
        }
    }
    // clang-format on

    static void setWindowSubclass(HWND hwnd, Window* window)
    {
        auto idSubclass = reinterpret_cast<DWORD_PTR>(window);
        if (!SetWindowSubclass(hwnd, windowSubclassProc, idSubclass, 0))
        {
            THROW_SYSTEM_ERROR(ERROR_GEN_FAILURE);
        }
    }

    static LRESULT CALLBACK windowSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR idSubclass, DWORD_PTR)
    {
        auto self = reinterpret_cast<Window*>(idSubclass);
        const std::optional<LRESULT> result = self->dispatchMessage(hwnd, msg, wParam, lParam);
        if (msg == WM_NCDESTROY)
        {
            self->hwnd_ = nullptr;
        }
        if (result)
        {
            return result.value();
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

private:
    // For common controls, always let DefWindowProc() handle messages. MainWindow needs to override this.
    virtual std::optional<LRESULT> dispatchMessage(HWND, UINT, WPARAM, LPARAM)
    {
        return std::nullopt;
    }
};

enum class AdjustmentFlags : uint32_t
{
    MakeCodeMapped = 0b0000'0001,
    VirtualKeyAdjusted = 0b0000'0010,
    ExtendedLookup = 0b1000'0000
};

constexpr bool enableBitmaskOperatorOr(AdjustmentFlags);
constexpr AdjustmentFlags enableBitmaskOperatorOrAssign(AdjustmentFlags);
constexpr bool enableBitmaskOperatorAnd(AdjustmentFlags);

enum class ToolBarButtonStates : uint32_t
{
    Adjustment = 0b0001,
    NoHotkeys = 0b0010,
    NoLegacy = 0b0100
};

constexpr bool enableBitmaskOperatorOr(ToolBarButtonStates);
constexpr ToolBarButtonStates enableBitmaskOperatorOrAssign(ToolBarButtonStates);
constexpr bool enableBitmaskOperatorAnd(ToolBarButtonStates);

class RawKeyboard : public RAWKEYBOARD
{
public:
    explicit RawKeyboard(const RAWKEYBOARD& rawkbd) noexcept
        : RAWKEYBOARD{rawkbd}
        , adjustments{(rawkbd.Flags & RI_KEY_E0) != 0 ? AdjustmentFlags::ExtendedLookup : AdjustmentFlags{0}}
        , isKeyDown{(rawkbd.Flags & RI_KEY_BREAK) == 0}
    {
    }

    RawKeyboard(const RAWKEYBOARD& rawkbd, AdjustmentFlags adjustmentFlags) noexcept
        : RawKeyboard{rawkbd}
    {
        adjustments = adjustmentFlags;
    }

    [[nodiscard]] USHORT getLookupCode() const noexcept
    {
        return static_cast<USHORT>(MakeCode | ((adjustments & AdjustmentFlags::ExtendedLookup) != AdjustmentFlags{0} ? 0x100 : 0));
    }

    AdjustmentFlags adjustments;
    const bool isKeyDown;
};

union PackedRawKeyboard
{
    struct PackedRawKeyboardBits
    {
        uint32_t makeCode : 8;
        uint32_t flags : 8;
        uint32_t vKey : 8;
        AdjustmentFlags adjustments : 8;
    } bits;
    LPARAM lParam;

    explicit PackedRawKeyboard(const RawKeyboard& rawkbd) noexcept
        : bits{.makeCode = rawkbd.MakeCode, .flags = rawkbd.Flags, .vKey = rawkbd.VKey, .adjustments = rawkbd.adjustments}
    {
    }

    explicit PackedRawKeyboard(LPARAM lParam) noexcept
        : lParam{lParam}
    {
    }

    [[nodiscard]] RawKeyboard getRawKeyboard() const noexcept
    {
        // clang-format off
        const RAWKEYBOARD unpacked
        {
            .MakeCode = static_cast<USHORT>(bits.makeCode),
            .Flags = static_cast<USHORT>(bits.flags),
            .VKey = static_cast<USHORT>(bits.vKey)
        };
        // clang-format on
        return RawKeyboard{unpacked, bits.adjustments};
    }
};

static_assert(sizeof(PackedRawKeyboard) == sizeof(LPARAM), "PackedRawKeyboard must be the size of a 32-bit lParam");

struct ListViewHeaderProperties
{
    int checkedMenuItemId;
    int width;
};

class Msg : public MSG
{
public:
    Msg() noexcept
        : MSG{}
    {
    }

    [[nodiscard]] bool getMessage()
    {
        int result = GetMessageW(this, nullptr, 0, 0);
        if (result < 0)
        {
            THROW_LAST_SYSTEM_ERROR();
        }
        return result > 0;
    }
};

enum class ScanCodeSequence
{
    None,
    E0,
    E1
};

struct KeyCodes
{
    KeyCodes(int keyCode, std::string_view sml, std::string_view ray, std::string_view glfw)
        : keyCode{keyCode}
        , sml{toWString(sml)}
        , ray{toWString(ray)}
        , glfw{toWString(glfw)}
    {
    }

    const int keyCode;
    const std::wstring sml;
    const std::wstring ray;
    const std::wstring glfw;
};

struct ToolTipPair
{
    int buttonId;
    int toolTipId;
};
