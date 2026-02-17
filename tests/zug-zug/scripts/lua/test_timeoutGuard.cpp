#include "scripts/lua/runtime.hpp"

#include <doctest/doctest.h>
#include <string>
#include <utility>

using namespace std::chrono_literals;

bool contains(std::string_view src, std::string_view fragment)
{
	return src.find(fragment) != std::string_view::npos;
}

namespace timeout = lua::timeoutGuard;

TEST_CASE("timeoutGuard: Manual watchdog arms and times out")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	CHECK_FALSE(watchdog.armed());
	REQUIRE(watchdog.arm(5ms));

	REQUIRE(watchdog.armed());

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	CHECK(watchdog.timeOut());
	watchdog.disarm();
	CHECK_FALSE(watchdog.timeOut());
	CHECK_FALSE(watchdog.armed());
}

TEST_CASE("timeoutGuard: Manual watchdog re-armed to protect multiple executions")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	const auto boilerPlate = R"(
		local sum = 1;
		for i = 1, 10000 do
			sum = sum + i
		end
		return sum
	)";

	auto watchdog = timeout::Watchdog(lua, 1'000);

	REQUIRE(watchdog.arm(5ms));
	CHECK_FALSE(watchdog.arm(5ms));

	auto result1 = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result1.valid());
	CHECK(contains(sol::error{result1}.what(), "Script timed out"));
	sol::error err1 = result1;
	CHECK(watchdog.timeOut());

	auto result2 = lua.safe_script(boilerPlate);
	REQUIRE_FALSE(result2.valid());
	CHECK(contains(sol::error{result2}.what(), "Script timed out"));

	REQUIRE(watchdog.rearm(5ms));
	CHECK_FALSE(watchdog.timeOut());

	auto result3 = lua.safe_script(boilerPlate);
	CHECK(result3.valid());

	watchdog.disarm();
}

TEST_CASE("timeoutGuard: Watchdog manual arm/disarm updates hook and registry")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) == nullptr);

	REQUIRE(watchdog.arm(5ms));
	CHECK(watchdog.armed());
	CHECK_FALSE(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) != nullptr);

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	watchdog.disarm();
	CHECK_FALSE(watchdog.armed());
	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) == nullptr);
}

TEST_CASE("timeoutGuard: Manual watchdog arm fails while registry slot is occupied")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto foreignCtx = timeout::HookContext{};
	timeout::Watchdog::CtxRegistry::set(lua, &foreignCtx);

	auto watchdog = timeout::Watchdog(lua);
	CHECK_FALSE(watchdog.arm(5ms));
	CHECK_FALSE(watchdog.armed());

	timeout::Watchdog::CtxRegistry::remove(lua);

	REQUIRE(watchdog.arm(5ms));

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	watchdog.disarm();
}

TEST_CASE("timeoutGuard: Manual watchdog arm fails while Lua state already has a hook")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	timeout::setHook(lua, 1, timeout::defaultHook);
	REQUIRE(lua_gethook(lua.lua_state()) != nullptr);
	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));

	auto watchdog = timeout::Watchdog(lua);
	CHECK_FALSE(watchdog.arm(5ms));
	CHECK_FALSE(watchdog.armed());

	timeout::removeHook(lua);
	CHECK(lua_gethook(lua.lua_state()) == nullptr);

	REQUIRE(watchdog.arm(5ms));
	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	watchdog.disarm();
}

TEST_CASE("timeoutGuard: Manual watchdog rearm fails when it is not armed")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	CHECK_FALSE(watchdog.rearm(5ms));
	CHECK_FALSE(watchdog.armed());
	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) == nullptr);

	REQUIRE(watchdog.arm(5ms));
	watchdog.disarm();

	CHECK_FALSE(watchdog.rearm(5ms));
}

TEST_CASE("timeoutGuard: Manual watchdog detach disarms and requires reattach")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	REQUIRE(watchdog.arm(5ms));
	CHECK_FALSE(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) != nullptr);

	watchdog.detach();
	CHECK_FALSE(watchdog.armed());
	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) == nullptr);
	CHECK_FALSE(watchdog.arm(5ms));

	REQUIRE(watchdog.attach(lua));
	REQUIRE(watchdog.arm(5ms));

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	watchdog.disarm();
}

TEST_CASE("timeoutGuard: Two watchdogs on same Lua state cannot arm simultaneously")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog1 = timeout::Watchdog(lua);
	auto watchdog2 = timeout::Watchdog(lua);

	REQUIRE(watchdog1.arm(5ms));
	CHECK_FALSE(watchdog2.arm(5ms));

	auto result1 = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result1.valid());
	CHECK(contains(sol::error{result1}.what(), "Script timed out"));

	watchdog1.disarm();

	REQUIRE(watchdog2.arm(5ms));
	auto result2 = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result2.valid());
	CHECK(contains(sol::error{result2}.what(), "Script timed out"));

	watchdog2.disarm();
}

TEST_CASE("timeoutGuard: Watchdog manual attach rejects reassign while armed")
{
	sol::state lua1;
	lua1.open_libraries(sol::lib::base);

	sol::state lua2;
	lua2.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua1);

	REQUIRE(watchdog.arm(5ms));
	CHECK_FALSE(watchdog.attach(lua2));

	auto lua1Result = lua1.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(lua1Result.valid());
	CHECK(contains(sol::error{lua1Result}.what(), "Script timed out"));

	watchdog.disarm();

	REQUIRE(watchdog.attach(lua2));
	REQUIRE(watchdog.arm(5ms));

	auto lua2Result = lua2.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(lua2Result.valid());
	CHECK(contains(sol::error{lua2Result}.what(), "Script timed out"));

	watchdog.disarm();
}

TEST_CASE("timeoutGuard: default hook reports missing context")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	timeout::setHook(lua, 1, timeout::defaultHook);

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Unable to get hook context"));

	timeout::removeHook(lua);
	CHECK(lua_gethook(lua.lua_state()) == nullptr);
}

TEST_CASE("timeoutGuard: ScopeGuard arms on start and times out")
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
		CHECK(contains(sol::error{result}.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: ScopeGuard can be re-armed multiple times")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	for (auto i = 0; i < 3; ++i) {
		CHECK_FALSE(watchdog.armed());

		{
			auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

			CHECK(watchdog.armed());

			auto result = lua.safe_script(R"(
				while true do end
			)");
			REQUIRE_FALSE(result.valid());
			CHECK(contains(sol::error{result}.what(), "Script timed out"));
		}
		CHECK_FALSE(watchdog.armed());
	}
}

TEST_CASE("timeoutGuard: Secondary scope guard is disabled when watchdog already armed")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);
	auto primaryGuard = timeout::GuardedScope(watchdog, 5ms);

	REQUIRE(watchdog.armed());

	auto secondaryGuard = timeout::GuardedScope(watchdog, 5ms);
	CHECK_FALSE(secondaryGuard.rearm(5ms));
	CHECK_FALSE(secondaryGuard.timedOut());

	auto result = lua.safe_script(R"(
		while true do end
	)");
	REQUIRE_FALSE(result.valid());
	CHECK(contains(sol::error{result}.what(), "Script timed out"));

	CHECK(primaryGuard.timedOut());
	CHECK_FALSE(secondaryGuard.timedOut());
}

TEST_CASE("timeoutGuard: Scope guard move transfers watchdog ownership")
{
	sol::state lua;
	lua.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua);

	{
		auto guard1 = timeout::GuardedScope(watchdog, 5ms);
		REQUIRE(watchdog.armed());

		auto guard2 = std::move(guard1);
		CHECK_FALSE(guard1.rearm(5ms));
		CHECK_FALSE(guard1.timedOut());

		auto result = lua.safe_script(R"(
			while true do end
		)");
		REQUIRE_FALSE(result.valid());
		CHECK(contains(sol::error{result}.what(), "Script timed out"));
		CHECK(guard2.timedOut());
	}

	CHECK_FALSE(watchdog.armed());
	CHECK(timeout::Watchdog::CtxRegistry::empty(lua));
	CHECK(lua_gethook(lua.lua_state()) == nullptr);
}

TEST_CASE("timeoutGuard:GuardedScope reassigns Watchdog to Lua state")
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

	REQUIRE(watchdog.attach(lua2));

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

		auto result = lua2.safe_script(R"(
			while true do end
		)");
		REQUIRE_FALSE(result.valid());
		CHECK(contains(sol::error{result}.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: Attach while armed keeps old Lua state protected")
{
	sol::state lua1;
	lua1.open_libraries(sol::lib::base);
	sol::state lua2;
	lua2.open_libraries(sol::lib::base);

	auto watchdog = timeout::Watchdog(lua1);

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

		REQUIRE(watchdog.armed());
		CHECK_FALSE(watchdog.attach(lua2));

		auto lua1Result = lua1.safe_script(R"(
			while true do end
		)");
		REQUIRE_FALSE(lua1Result.valid());
		CHECK(contains(sol::error{lua1Result}.what(), "Script timed out"));

		auto lua2Result = lua2.safe_script("return 42");
		REQUIRE(lua2Result.valid());
		CHECK(lua2Result.get<int>() == 42);
	}

	REQUIRE(watchdog.attach(lua2));

	{
		auto scopeGuard = timeout::GuardedScope(watchdog, 5ms);

		auto lua2Result = lua2.safe_script(R"(
			while true do end
		)");
		REQUIRE_FALSE(lua2Result.valid());
		CHECK(contains(sol::error{lua2Result}.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: GuardedScope stops infinite loop executed from sandbox")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	{
		auto scopeGuard = sandbox.makeTimeoutGuardedScope(5ms);

		auto result = sandbox.run(R"(
			while true do end
		)");
		REQUIRE_FALSE(result.valid());
		CHECK(contains(sol::error{result}.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: GuardedScope can be re-armed to protect multiple executions")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	const auto boilerPlate = R"(
		local sum = 1;
		for i = 1, 10000 do
			sum = sum + i
		end
		return sum
	)";

	{
		auto scopeGuard = sandbox.makeTimeoutGuardedScope(5ms);

		auto result1 = sandbox.run(R"(
			while true do end
		)");
		REQUIRE_FALSE(result1.valid());
		CHECK(contains(sol::error{result1}.what(), "Script timed out"));

		CHECK(scopeGuard.timedOut());

		auto result2 = sandbox.run(boilerPlate);
		REQUIRE_FALSE(result2.valid());
		CHECK(contains(sol::error{result2}.what(), "Script timed out"));

		REQUIRE(scopeGuard.rearm(5ms));
		CHECK_FALSE(scopeGuard.timedOut());

		auto result3 = sandbox.run(boilerPlate);
		REQUIRE(result3.valid());

		auto result4 = sandbox.run(R"(
			while true do end
		)");
		REQUIRE_FALSE(result4.valid());
		CHECK(contains(sol::error{result4}.what(), "Script timed out"));
	}
}

TEST_CASE("timeoutGuard: One runtime guard applies to multiple sandboxes on the same Lua state")
{
	LuaRuntime lua;
	LuaSandbox sandboxA(lua, LuaSandbox::Presets::Custom);
	LuaSandbox sandboxB(lua, LuaSandbox::Presets::Custom);

	{
		auto scopeGuard = sandboxA.makeTimeoutGuardedScope(5ms);

		auto aTimeout = sandboxA.run(R"(
			while true do end
		)");
		REQUIRE_FALSE(aTimeout.valid());
		CHECK(contains(sol::error{aTimeout}.what(), "Script timed out"));

		CHECK(scopeGuard.timedOut());

		scopeGuard.rearm(5ms); // rearm for the next script
		REQUIRE_FALSE(scopeGuard.timedOut());

		auto bTimeout = sandboxB.run(R"(
			while true do end
		)");
		REQUIRE_FALSE(bTimeout.valid());

		CHECK(contains(sol::error{bTimeout}.what(), "Script timed out"));
	}
}
