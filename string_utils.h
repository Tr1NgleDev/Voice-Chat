#pragma once

#include <string>
#include <algorithm>
#include <cstdint>

inline constexpr static void trimStart(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](uint8_t c) {
		return !std::isspace(c);
		}));
}
inline constexpr static void trimEnd(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](uint8_t c) {
		return !std::isspace(c);
		}).base(), s.end());
}
inline constexpr static void trim(std::string& s)
{
	trimStart(s);
	trimEnd(s);
}
inline constexpr static void toLower(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](uint8_t c) { return std::tolower(c); });
}
inline constexpr static void toUpper(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](uint8_t c) { return std::toupper(c); });
}
