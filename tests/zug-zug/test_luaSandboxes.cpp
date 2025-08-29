#include <doctest/doctest.h>
#include <sol/forward.hpp>
#include "scripts/lua/runtime.hpp"

TEST_CASE("LuaState require loads libraries") {
	LuaState lua;

	REQUIRE_FALSE(lua.state["assert"].valid());

	CHECK(lua.require(sol::lib::base));
	REQUIRE(lua.state["assert"].valid());
}

TEST_CASE("LuaRuntime empty preset has no functions") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK_FALSE(sandbox["assert"].valid());
}

TEST_CASE("LuaState require does not loads libraries into LuaRuntime") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK_FALSE(lua.state["assert"].valid());
	CHECK_FALSE(sandbox["assert"].valid());

	CHECK(lua.require(sol::lib::base));

	CHECK(lua.state["assert"].valid());
	CHECK_FALSE(sandbox["assert"].valid());
}

TEST_CASE("LuaRuntime a named fixed preset does not allows to load libraries manually") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	REQUIRE_FALSE(sandbox["string"].valid());

	CHECK_FALSE(sandbox.require(sol::lib::string));

	CHECK_FALSE(sandbox["string"].valid());
}

TEST_CASE("LuaRuntime custom preset allows to load libraries manually") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK_FALSE(sandbox["assert"].valid());
	CHECK_FALSE(sandbox["type"].valid());

	CHECK(sandbox.require(sol::lib::base));

	CHECK(sandbox["assert"].valid());
	CHECK(sandbox["type"].valid());
}

TEST_CASE("LuaRuntime base preset allows safe functions") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	REQUIRE(sandbox["type"].valid());

	sol::object result = sandbox["type"]("foo");
	CHECK(result.as<std::string>() == "string");
}

TEST_CASE("LuaRuntime restricted string functions not available") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK(sandbox.require(sol::lib::string));

	REQUIRE(sandbox["string"]["upper"].valid());
	CHECK_FALSE(sandbox["string"]["dump"].valid());
}

TEST_CASE("LuaRuntime restricted os functions not available") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK(sandbox.require(sol::lib::os));

	REQUIRE(sandbox["os"]["clock"].valid());
	CHECK_FALSE(sandbox["os"]["execute"].valid());
}

TEST_CASE("LuaRuntime restricted debug library not available") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK_FALSE(sandbox.require(sol::lib::debug));
	CHECK_FALSE(sandbox["debug"].valid());
}

TEST_CASE("LuaRuntime run executes code") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	auto result = sandbox.run("return tostring(42)");
	CHECK(result.get<std::string>() == "42");
}

TEST_CASE("LuaRuntime operator[] variable access") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	sandbox["x"] = 123;
	auto result = sandbox.run("return x * 2");
	CHECK(result.get<int>() == 246);
}

TEST_CASE("LuaRuntime sandbox keeps objects isolated from global lua") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	sandbox["x"] = 123;
	lua.state["x"] = 321;
	auto result = sandbox.run("return x * 2");
	CHECK(result.get<int>() == 246);

	result = lua.state.script("return x * 2");
	CHECK(result.get<int>() == 642);
}
