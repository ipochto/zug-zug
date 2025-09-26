#pragma once

#include <algorithm>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

/*----------------------------------------------------------------------------
--  Utils
----------------------------------------------------------------------------*/
namespace fs_utils {

	inline bool startsWith(const fs::path &base, const fs::path &path)
	{
		if (base.empty()) {
			return false;
		}
		const auto baseNorm = fs::absolute(base).lexically_normal();
		const auto pathNorm = fs::absolute(path).lexically_normal();

		const auto [baseEnd, notused] = std::ranges::mismatch(baseNorm, pathNorm);
		return baseEnd == baseNorm.end();
	}

	inline bool startsWithOneOf(const std::vector<fs::path> &bases, const fs::path &path)
	{
		if (bases.empty()) {
			return false;
		}
		for (const auto &base: bases) {
			if (startsWith(base, path)) {
				return true;
			}
		}
		return false;
	}
}
