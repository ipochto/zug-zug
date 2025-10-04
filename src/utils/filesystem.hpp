#pragma once

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <ranges>

namespace fs = std::filesystem;

/*----------------------------------------------------------------------------
--  Utils
----------------------------------------------------------------------------*/
namespace fs_utils {

	[[nodiscard]]
	inline bool startsWith(const fs::path &path, const fs::path &root)
	{
		if (root.empty()) {
			return false;
		}
		const auto rootNorm = fs::absolute(root).lexically_normal();
		const auto pathNorm = fs::absolute(path).lexically_normal();

		const auto [rootEnd, _] = std::ranges::mismatch(rootNorm, pathNorm);
		return rootEnd == rootNorm.end();
	}

	template <std::ranges::input_range Range>
	requires std::same_as<std::ranges::range_value_t<Range>, fs::path>
	[[nodiscard]]
	inline bool startsWith(const fs::path &path, const Range &roots)
	{
		if (roots.empty()) {
			return false;
		}
		for (const auto &root: roots) {
			if (startsWith(path, root)) {
				return true;
			}
		}
		return false;
	}
}
