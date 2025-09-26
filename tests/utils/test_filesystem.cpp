#include <doctest/doctest.h>
#include "utils/filesystem.hpp"

TEST_CASE("fs_utils: startsWith absolute base") {
	const auto wrkDir = fs::path("/the/path/to/game/data");

	CHECK(fs_utils::startsWith(fs::path(wrkDir / "scripts"), wrkDir));
	CHECK(fs_utils::startsWith(fs::path(wrkDir / "./scripts"), wrkDir));

	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "scripts"), {}));
	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "../scripts"), wrkDir));
	CHECK_FALSE(fs_utils::startsWith(fs::path("scripts"), wrkDir));
	CHECK_FALSE(fs_utils::startsWith(fs::path("../scripts"), wrkDir));
}

TEST_CASE("fs_utils: startsWith relative base") {
	const auto wrkDir = fs::path("game/data");

	CHECK(fs_utils::startsWith(fs::path(wrkDir / "scripts"), wrkDir));
	CHECK(fs_utils::startsWith(fs::path(wrkDir / "./scripts"), wrkDir));

	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "scripts"), {}));
	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "../scripts"), wrkDir));
	CHECK_FALSE(fs_utils::startsWith(fs::path("scripts"), wrkDir));
	CHECK_FALSE(fs_utils::startsWith(fs::path("../scripts"), wrkDir));
}

TEST_CASE("fs_utils: startsWith range of bases") {
	const auto wrkDir = fs::path("/the/path/to/game/data");
	const auto allowedPaths = std::vector<fs::path> {
		{wrkDir / "scripts"},
		{wrkDir / "mods"}
	};

	CHECK(fs_utils::startsWith(fs::path(wrkDir / "scripts/config.lua"), allowedPaths));
	CHECK(fs_utils::startsWith(fs::path(wrkDir / "scripts/tileset"), allowedPaths));
	CHECK(fs_utils::startsWith(fs::path(wrkDir / "mods/config.lua"), allowedPaths));

	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "scripts/config.lua"), {}));
	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "config.lua"), allowedPaths));
	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "../scripts/tileset"), allowedPaths));
	CHECK_FALSE(fs_utils::startsWith(fs::path(wrkDir / "mods/../config.lua"), allowedPaths));
}
