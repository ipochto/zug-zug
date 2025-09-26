#include <doctest/doctest.h>
#include "utils/filesystem.hpp"

TEST_CASE("fs_utils: startsWith") {
	const auto wrkDir = fs::path("/the/path/to/game/data");

	CHECK(fs_utils::startsWith(wrkDir, fs::path(wrkDir / "scripts")));
	CHECK(fs_utils::startsWith(wrkDir, fs::path(wrkDir / "./scripts")));

	CHECK_FALSE(fs_utils::startsWith({}, fs::path(wrkDir / "scripts")));
	CHECK_FALSE(fs_utils::startsWith(wrkDir, fs::path(wrkDir / "../scripts")));
	CHECK_FALSE(fs_utils::startsWith(wrkDir, fs::path("scripts")));
	CHECK_FALSE(fs_utils::startsWith(wrkDir, fs::path("../scripts")));
}

TEST_CASE("fs_utils: startsWithOneOf") {
	const auto wrkDir = fs::path("/the/path/to/game/data");
	const auto allowedPaths = std::vector<fs::path> {
		{wrkDir / "scripts"},
		{wrkDir / "mods"}
	};

	CHECK(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "scripts/config.lua")));
	CHECK(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "scripts/tileset")));
	CHECK(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "mods/config.lua")));

	CHECK_FALSE(fs_utils::startsWithOneOf({}, fs::path(wrkDir / "scripts/config.lua")));
	CHECK_FALSE(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "config.lua")));
	CHECK_FALSE(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "../scripts/tileset")));
	CHECK_FALSE(fs_utils::startsWithOneOf(allowedPaths, fs::path(wrkDir / "mods/../config.lua")));
}
