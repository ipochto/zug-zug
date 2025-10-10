#include <doctest/doctest.h>
#include <string>
#include "scripts/lua/runtime.hpp"

TEST_CASE("LuaState require loads libraries") {
	LuaState lua;

	REQUIRE_FALSE(lua.state["assert"].valid());

	REQUIRE(lua.require(sol::lib::base));

	CHECK(lua.state["assert"].valid());
}

TEST_CASE("LuaRuntime empty preset has no functions") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK_FALSE(sandbox["assert"].valid());
}

TEST_CASE("LuaState require does not loads libraries into LuaRuntime") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	REQUIRE_FALSE(lua.state["string"].valid());
	REQUIRE_FALSE(sandbox["string"].valid());

	REQUIRE(lua.require(sol::lib::string));

	CHECK(lua.state["string"].valid());
	CHECK_FALSE(sandbox["string"].valid());
}

TEST_CASE("LuaRuntime a named fixed preset does not allows to load libraries manually") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	REQUIRE_FALSE(sandbox["string"].valid());

	REQUIRE_FALSE(sandbox.require(sol::lib::string));

	CHECK_FALSE(sandbox["string"].valid());
}

TEST_CASE("LuaRuntime custom preset allows to load libraries manually") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	REQUIRE_FALSE(sandbox["assert"].valid());
	REQUIRE_FALSE(sandbox["type"].valid());

	REQUIRE(sandbox.require(sol::lib::base));

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

	REQUIRE(sandbox.require(sol::lib::string));

	REQUIRE(sandbox["string"].valid());
	CHECK(sandbox["string"]["upper"].valid());
	CHECK_FALSE(sandbox["string"]["dump"].valid());
}

TEST_CASE("LuaRuntime restricted os functions not available") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	REQUIRE(sandbox.require(sol::lib::os));

	REQUIRE(sandbox["os"].valid());
	CHECK(sandbox["os"]["clock"].valid());
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

TEST_CASE("LuaRuntime sandbox drops objects after reset()") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Base);

	sandbox["foo"] = "bar";
	REQUIRE(sandbox["foo"].valid());

	sandbox.reset();

	CHECK_FALSE(sandbox["foo"].valid());
}

TEST_CASE("LuaRuntime sandbox reloads libraries after reset()") {
	LuaState lua;
	LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom);

	CHECK(sandbox.require(sol::lib::base));
	CHECK(sandbox.require(sol::lib::string));

	sandbox.reset();

	CHECK(sandbox["assert"].valid());
	CHECK(sandbox["type"].valid());
	
	REQUIRE(sandbox["string"].valid());
	CHECK(sandbox["string"]["upper"].valid());
}
