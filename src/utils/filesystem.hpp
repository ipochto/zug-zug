#pragma once

#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

/*----------------------------------------------------------------------------
--  Utils
----------------------------------------------------------------------------*/
namespace fs_utils {

	template<typename T>
	concept Character = std::same_as<T, char> || std::same_as<T, wchar_t>;

	template <Character CharT>
	inline bool starts_with(std::basic_string_view<CharT> checkStr,
                        	std::basic_string_view<CharT> prefix)
	{
		return checkStr.substr(0, prefix.size()) == prefix;
	}
}
