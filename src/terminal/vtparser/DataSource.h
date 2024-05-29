#pragma once

#include <string_view>
#include <array>
#include <memory>

namespace vt {

struct StringStream
{
    StringStream(std::string_view target) : _target(target) {}

    size_t Read(char* buf, size_t capacity)
    {
        if (pos >= _target.size()) {
            return 0;
        }

        auto count = std::min(_target.size() - pos, capacity);
        std::memcpy(buf, _target.data() + pos, count);
        pos += count;

        return count;
    }

    std::string_view _target;
    size_t pos = 0;
};

struct DataSource
{
    static constexpr size_t kReadSize = 4096;
    static constexpr size_t kBufSize = kReadSize + 32;
    static constexpr size_t npos = (size_t)-1;

    size_t ReadFrom(auto& stream)
    {
        size_t bufPos = 0;

        if (_u8partial) {
            std::memcpy(_u8buf.get(), _u8partial.buf.data(), _u8partial.len);
            bufPos += _u8partial.len;
        }

        size_t u8len = 0;

        while (true) {
            auto bytesLen = stream.Read(_u8buf.get() + bufPos, kReadSize);
            if (bytesLen == 0) {
                if (_u8partial) {
                    u8len = _u8partial.len;
                    _u8partial.len = 0;
                    break;
                }
                else {
                    return 0;
                }
            }
            bytesLen += bufPos;
            u8len = trim_partial_utf8(_u8buf.get(), bytesLen);
            if (u8len != 0) {
                if (u8len == bytesLen) {
                    _u8partial.len = 0;
                }
                else {
                    auto trimed = bytesLen - u8len;
                    std::memcpy(_u8partial.buf.data(), _u8buf.get() + u8len, trimed);
                    _u8partial.len = (uint8_t)trimed;
                }
                break;
            }
            bufPos = bytesLen;
        }

        auto len = MultiByteToWideChar(CP_UTF8, 0UL, _u8buf.get(), (int)u8len, _u16buf.get(), kBufSize);
        if (len == 0) {
            throw;
        }
        _u16len = (size_t)len;

        return _u16len;
    }

    std::wstring_view Data() const
    {
        return { _u16buf.get(), _u16len };
    }

private:
    // From simdbuf, MIT License.
    inline size_t trim_partial_utf8(const char* input, size_t length)
    {
        if (length < 3) {
            switch (length) {
            case 2:
                if (uint8_t(input[length - 1]) >= 0xc0) { return length - 1; } // 2-, 3- and 4-byte characters with only 1 byte left
                if (uint8_t(input[length - 2]) >= 0xe0) { return length - 2; } // 3- and 4-byte characters with only 2 bytes left
                return length;
            case 1:
                if (uint8_t(input[length - 1]) >= 0xc0) { return length - 1; } // 2-, 3- and 4-byte characters with only 1 byte left
                return length;
            case 0:
                return length;
            }
        }
        if (uint8_t(input[length - 1]) >= 0xc0) { return length - 1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (uint8_t(input[length - 2]) >= 0xe0) { return length - 2; } // 3- and 4-byte characters with only 1 byte left
        if (uint8_t(input[length - 3]) >= 0xf0) { return length - 3; } // 4-byte characters with only 3 bytes left
        return length;
    }

    struct {
        uint8_t len = 0;
        std::array<char, 4> buf;
        operator bool() const { return len != 0; }
    } _u8partial;

    std::unique_ptr<char[]> _u8buf{ new char[kBufSize] };
    size_t _u8len = 0;

    std::unique_ptr<wchar_t[]> _u16buf{ new wchar_t[kBufSize] };
    size_t _u16len = 0;
};

}
