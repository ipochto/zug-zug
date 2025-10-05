#pragma once

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <ranges>

namespace fs = std::filesystem;

template <typename T>
concept fsPaths =
	std::ranges::range<T>
	&& std::same_as<std::remove_cvref_t<std::ranges::range_value_t<T>>, fs::path>;

/*----------------------------------------------------------------------------
--  Utils
----------------------------------------------------------------------------*/
namespace fs_utils {

	[[nodiscard]]
	inline auto normalize(const fs::path &path)
		-> fs::path
	{
		auto result = path.lexically_normal();
		if (result.native().ends_with(fs::path::preferred_separator)) {
			return result.parent_path();
		}
		return result;
	}	

	[[nodiscard]]
	inline bool startsWith(const fs::path &path, const fs::path &root)
	{
		if (root.empty()) {
			return false;
		}
		const auto rootNorm = normalize(fs::absolute(root));
		const auto pathNorm = normalize(fs::absolute(path));
		const auto [rootEnd, _] = std::ranges::mismatch(rootNorm, pathNorm);
		return rootEnd == rootNorm.end();
	}

	[[nodiscard]]
	inline bool startsWith(const fs::path &path, const fsPaths auto &roots)
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
