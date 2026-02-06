#include "scripts/lua/runtime.hpp"

#include <doctest/doctest.h>
#include <string>

using namespace std::chrono_literals;

bool consist(std::string_view src, std::string_view fragment)
{
    return src.find(fragment) != std::string_view::npos;
}

namespace timeout = lua::timeoutGuard;

TEST_CASE("timeoutGuard: Watchdog arms on start and times out")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

    auto watchdog = timeout::Watchdog(lua);

	CHECK_FALSE(watchdog.armed());

	{
        auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

        CHECK(watchdog.armed());
        
		auto result = lua.safe_script(R"(
			while true do end
		)");
        REQUIRE_FALSE(result.valid());
        const sol::error err = result;
        CHECK(consist(err.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: GuardedScope restores hook period")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

    auto watchdog = timeout::Watchdog(lua);

	constexpr auto basePeriod = timeout::InstructionsCount{5'000};

	watchdog.arm(basePeriod);
	CHECK(watchdog.getPeriod() == basePeriod);

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 10ms, 20'000);

		CHECK(watchdog.getPeriod() == 20'000);
	}
	CHECK(watchdog.getPeriod() == basePeriod);
}


TEST_CASE("timeoutGuard: Watchdog reassigns to new lua_State")
{
	sol::state lua1;
	lua1.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua1);

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

		auto result = lua1.safe_script(R"(
			while true do end
		)");
		CHECK_FALSE(result.valid());
	}

	sol::state lua2;
	lua2.open_libraries(sol::lib::base);

	watchdog.assign(lua2);

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

		auto result = lua2.safe_script(R"(
			while true do end
		)");
        REQUIRE_FALSE(result.valid());
        const sol::error err = result;
        CHECK(consist(err.what(), "Script timed out"));
	}
}
