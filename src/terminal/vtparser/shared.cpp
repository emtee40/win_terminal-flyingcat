#include "precomp.h"
#include "shared.h"

// This is a copy of how DirectXMath.h determines _XM_SSE_INTRINSICS_ and _XM_ARM_NEON_INTRINSICS_.
#if (defined(_M_IX86) || defined(_M_X64) || __i386__ || __x86_64__) && !defined(_M_HYBRID_X86_ARM64) && !defined(_M_ARM64EC)
#define TIL_SSE_INTRINSICS
#elif defined(_M_ARM) || defined(_M_ARM64) || defined(_M_HYBRID_X86_ARM64) || defined(_M_ARM64EC) || __arm__ || __aarch64__
#define TIL_ARM_NEON_INTRINSICS
#else
#define TIL_NO_INTRINSICS
#endif

// Returns true for C0 characters and C1 [single-character] CSI.
static bool isActionableFromGround(const wchar_t wch) noexcept
{
    // This is equivalent to:
    //   return (wch <= 0x1f) || (wch >= 0x7f && wch <= 0x9f);
    // It's written like this to get MSVC to emit optimal assembly for findActionableFromGround.
    // It lacks the ability to turn boolean operators into binary operations and also happens
    // to fail to optimize the printable-ASCII range check into a subtraction & comparison.
    return (wch <= 0x1f) | (static_cast<wchar_t>(wch - 0x7f) <= 0x20);
}

[[msvc::forceinline]] static size_t findActionableFromGroundPlain(const wchar_t* beg, const wchar_t* end, const wchar_t* it) noexcept
{
#pragma loop(no_vector)
    for (; it < end && !isActionableFromGround(*it); ++it)
    {
    }
    return it - beg;
}

size_t findActionableFromGround(const wchar_t* data, size_t count) noexcept
{
    // The following vectorized code replicates isActionableFromGround which is equivalent to:
    //   (wch <= 0x1f) || (wch >= 0x7f && wch <= 0x9f)
    // or rather its more machine friendly equivalent:
    //   (wch <= 0x1f) | ((wch - 0x7f) <= 0x20)
#if defined(TIL_SSE_INTRINSICS)

    auto it = data;

    for (const auto end = data + (count & ~size_t{ 7 }); it < end; it += 8)
    {
        const auto wch = _mm_loadu_si128(reinterpret_cast<const __m128i*>(it));
        const auto z = _mm_setzero_si128();

        // Dealing with unsigned numbers in SSE2 is annoying because it has poor support for that.
        // We'll use subtractions with saturation ("SubS") to work around that. A check like
        // a < b can be implemented as "max(0, a - b) == 0" and "max(0, a - b)" is what "SubS" is.

        // Check for (wch < 0x20)
        auto a = _mm_subs_epu16(wch, _mm_set1_epi16(0x1f));
        // Check for "((wch - 0x7f) <= 0x20)" by adding 0x10000-0x7f, which overflows to a
        // negative number if "wch >= 0x7f" and then subtracting 0x9f-0x7f with saturation to an
        // unsigned number (= can't go lower than 0), which results in all numbers up to 0x9f to be 0.
        auto b = _mm_subs_epu16(_mm_add_epi16(wch, _mm_set1_epi16(static_cast<short>(0xff81))), _mm_set1_epi16(0x20));
        a = _mm_cmpeq_epi16(a, z);
        b = _mm_cmpeq_epi16(b, z);

        const auto c = _mm_or_si128(a, b);
        const auto mask = _mm_movemask_epi8(c);

        if (mask)
        {
            unsigned long offset;
            _BitScanForward(&offset, mask);
            it += offset / 2;
            return it - data;
        }
    }

    return findActionableFromGroundPlain(data, data + count, it);

#elif defined(TIL_ARM_NEON_INTRINSICS)

    auto it = data;
    uint64_t mask;

    for (const auto end = data + (count & ~size_t{ 7 }); it < end;)
    {
        const auto wch = vld1q_u16(it);
        const auto a = vcleq_u16(wch, vdupq_n_u16(0x1f));
        const auto b = vcleq_u16(vsubq_u16(wch, vdupq_n_u16(0x7f)), vdupq_n_u16(0x20));
        const auto c = vorrq_u16(a, b);

        mask = vgetq_lane_u64(c, 0);
        if (mask)
        {
            goto exitWithMask;
        }
        it += 4;

        mask = vgetq_lane_u64(c, 1);
        if (mask)
        {
            goto exitWithMask;
        }
        it += 4;
    }

    return findActionableFromGroundPlain(data, data + count, it);

exitWithMask:
    unsigned long offset;
    _BitScanForward64(&offset, mask);
    it += offset / 16;
    return it - data;

#else

    return findActionableFromGroundPlain(data, data + count, p);

#endif
}
