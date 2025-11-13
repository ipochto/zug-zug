#include "scripts/lua/runtime.hpp"

#include <doctest/doctest.h>
#include <string>

TEST_CASE("LuaState require loads libraries")
{
	LuaRuntime lua;

	REQUIRE_FALSE(lua.state["assert"].valid());

	lua.require(sol::lib::base);

	CHECK(lua.state["assert"].valid());
}

TEST_CASE("LuaRuntime empty preset has no functions")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	CHECK_FALSE(sandbox["assert"].valid());
}

TEST_CASE("LuaState require does not loads libraries into LuaRuntime")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	REQUIRE_FALSE(lua.state["string"].valid());
	REQUIRE_FALSE(sandbox["string"].valid());

	lua.require(sol::lib::string);

	CHECK(lua.state["string"].valid());
	CHECK_FALSE(sandbox["string"].valid());
}

TEST_CASE("LuaRuntime a named fixed preset does not allows to load libraries manually")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	REQUIRE_FALSE(sandbox["string"].valid());

	REQUIRE_FALSE(sandbox.require(sol::lib::string));

	CHECK_FALSE(sandbox["string"].valid());
}

TEST_CASE("LuaRuntime custom preset allows to load libraries manually")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	REQUIRE_FALSE(sandbox["assert"].valid());
	REQUIRE_FALSE(sandbox["type"].valid());

	REQUIRE(sandbox.require(sol::lib::base));

	CHECK(sandbox["assert"].valid());
	CHECK(sandbox["type"].valid());
}

TEST_CASE("LuaRuntime base preset allows safe functions")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	REQUIRE(sandbox["type"].valid());

	sol::object result = sandbox["type"]("foo");
	CHECK(result.as<std::string>() == "string");
}

TEST_CASE("LuaRuntime restricted string functions not available")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	REQUIRE(sandbox.require(sol::lib::string));

	REQUIRE(sandbox["string"].valid());
	CHECK(sandbox["string"]["upper"].valid());
	CHECK_FALSE(sandbox["string"]["dump"].valid());
}

TEST_CASE("LuaRuntime restricted os functions not available")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	REQUIRE(sandbox.require(sol::lib::os));

	REQUIRE(sandbox["os"].valid());
	CHECK(sandbox["os"]["clock"].valid());
	CHECK_FALSE(sandbox["os"]["execute"].valid());
}

TEST_CASE("LuaRuntime restricted debug library not available")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	CHECK_FALSE(sandbox.require(sol::lib::debug));
	CHECK_FALSE(sandbox["debug"].valid());
}

TEST_CASE("LuaRuntime run executes code")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	auto result = sandbox.run("return tostring(42)");
	CHECK(result.get<std::string>() == "42");
}

TEST_CASE("LuaRuntime operator[] variable access")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	sandbox["x"] = 123;
	auto result = sandbox.run("return x * 2");
	CHECK(result.get<int>() == 246);
}

TEST_CASE("LuaRuntime sandbox keeps objects isolated from global lua")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	sandbox["x"] = 123;
	lua.state["x"] = 321;
	auto result = sandbox.run("return x * 2");
	CHECK(result.get<int>() == 246);

	result = lua.state.script("return x * 2");
	CHECK(result.get<int>() == 642);
}

TEST_CASE("LuaRuntime sandbox drops objects after reset()")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	sandbox["foo"] = "bar";
	REQUIRE(sandbox["foo"].valid());

	sandbox.reset();

	CHECK_FALSE(sandbox["foo"].valid());
}

TEST_CASE("LuaRuntime sandbox reloads libraries after reset()")
{
	LuaRuntime lua;
	LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

	CHECK(sandbox.require(sol::lib::base));
	CHECK(sandbox.require(sol::lib::string));

	sandbox.reset();

	CHECK(sandbox["assert"].valid());
	CHECK(sandbox["type"].valid());

	REQUIRE(sandbox["string"].valid());
	CHECK(sandbox["string"]["upper"].valid());
}

TEST_CASE("Multiple LuaRuntime sandboxes on the single LuaState")
{
	LuaRuntime lua;

	auto sandboxes = std::map<std::string, LuaSandbox>();

	sandboxes.emplace("core", LuaSandbox(lua, LuaSandbox::Presets::Core));
	sandboxes.emplace("complete", LuaSandbox(lua, LuaSandbox::Presets::Complete));
	
	auto &core = sandboxes.at("core");
	auto &complete = sandboxes.at("complete");

	core.run(R"(name = "core")");
	complete.run(R"(name = "complete")");

	REQUIRE(core["name"].valid());
	CHECK_EQ(core["name"].get<std::string>(), "core");

	REQUIRE(complete["name"].valid());
	CHECK_EQ(complete["name"].get<std::string>(), "complete");
}

//----------------------------------------------
// Not a tests, just some checks.
// Remove them then

TEST_CASE("Multiple LuaRuntime sandboxes on the single LuaState")
{
	LuaRuntime lua;
	auto sandboxes = std::map<std::string, LuaSandbox>();

	sandboxes.emplace("configs", LuaSandbox(lua, LuaSandbox::Presets::Custom));
	sandboxes.emplace("UI", LuaSandbox(lua, LuaSandbox::Presets::Custom));

	auto &luaConfigs = sandboxes.at("configs");
	auto &luaUI = sandboxes.at("UI");

	lua.state["name"] = "Raw Lua-state";
	luaConfigs["name"] = "'configs' sandbox";
	luaUI["name"] = "'UI' sandbox";

	const auto whoAmI= R"(
		print ("This is " .. name)
	)";

	lua.require(sol::lib::base);

	lua.state.script(whoAmI);
	for (auto &[name, sandbox] : sandboxes) {
		sandbox["print"] = lua.state["print"];
		sandbox.run(whoAmI);
	}
}