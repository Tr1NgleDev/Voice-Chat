#pragma once
#include <cstdint>
#include <limits>

#pragma pack(push, 1)
struct int24_t
{
	uint8_t bytes[3];

	int24_t() = default;

	constexpr int24_t(int32_t v)
		: bytes{ (uint8_t)(clamp(v) & 0xFF),
				 (uint8_t)((clamp(v) >> 8) & 0xFF),
				 (uint8_t)((clamp(v) >> 16) & 0xFF) } {
	}

	constexpr operator int32_t() const
	{
		int32_t v = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
		return (v & 0x800000) ? (v | ~0xFFFFFF) : v;
	}

private:
	static constexpr int32_t clamp(int32_t v)
	{
		return v < -8388608 ? -8388608 : (v > 8388607 ? 8388607 : v);
	}
};
#pragma pack(pop)

static_assert(sizeof(int24_t) == 3, "int24_t must be exactly 3 bytes");

namespace std
{
	template<>
	struct numeric_limits<int24_t> : numeric_limits<int32_t>
	{
		static constexpr int digits = 23;
		static constexpr int digits10 = 6;
		static constexpr int24_t min() noexcept { return int24_t(-8388608); }
		static constexpr int24_t max() noexcept { return int24_t(8388607); }
		static constexpr int24_t lowest() noexcept { return min(); }
	};
}