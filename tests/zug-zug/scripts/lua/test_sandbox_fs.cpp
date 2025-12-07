#include "scripts/lua/runtime.hpp"

#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

struct TempDir
{
	fs::path path;
	TempDir()
	{
		path = fs::temp_directory_path() / ("zzTests_" + randomString(8));
		if (fs::exists(path)) {
			fs::remove_all(path);
		}
		fs::create_directories(path);
	}
	~TempDir() { fs::remove_all(path); }

	void clear() { fs::remove_all(path); }

	std::string randomString(size_t length)
	{
		const auto chars = "0123456789abcdef";
		auto rndDevice = std::random_device();
		auto generator = std::mt19937(rndDevice());
		auto distribution = std::uniform_int_distribution(0, 15);
		auto result = std::string();
		for (size_t i = 0; i < length; ++i) {
			result += chars[distribution(generator)];
		}
		return result;
	}
};

bool createScriptFile(const fs::path &fileName, const auto &script)
{
	if (std::ofstream ofs(fileName); ofs) {
		ofs << script;
		return true;
	}
	return false;
}

bool createBytecodeFile(const fs::path &fileName)
{
	if (std::ofstream ofs(fileName, std::ios::binary); ofs) {
		ofs.write(LUA_SIGNATURE, 4);
		ofs << "some garbage data...";
		return true;
	}
	return false;
}

namespace files
{
	const auto module = R"(
		function setBar(value)
			bar = value
		end
		return setBar
	)";
	const auto script = R"(
		local foo = "foo"
		bar = 42
		return foo
	)";
} // namespace files

TEST_CASE("LuaRuntime sandbox runs a script file: Cpp side.")
{
	LuaRuntime lua;

	const auto tmpDir = TempDir();
	const auto wrkDir = fs::absolute(tmpDir.path / "scripts");
	fs::create_directories(wrkDir);

	REQUIRE(createScriptFile(wrkDir / "allowed.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "../forbidden.lua", files::script));

	SUBCASE("File exists, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		auto result = sandbox.runFile(fs::path(wrkDir / "allowed.lua"));
		REQUIRE(result.valid());
		CHECK(result.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, relative path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {"."});

		auto result = sandbox.runFile(fs::path(wrkDir / "allowed.lua"));
		REQUIRE(result.valid());
		CHECK(result.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File[s] exist, path[s] are allowed.")
	{
		const auto scriptsRoot = wrkDir / "..";
		fs::create_directories(wrkDir / "../mods");
		REQUIRE(createScriptFile(scriptsRoot / "mods/allowed.lua", files::script));

		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, scriptsRoot, {"scripts", "mods"});

		auto scriptsResult = sandbox.runFile(fs::path(scriptsRoot / "scripts/allowed.lua"));
		REQUIRE(scriptsResult.valid());
		CHECK(scriptsResult.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);

		sandbox["bar"] = 0;

		auto modsResult = sandbox.runFile(fs::path(scriptsRoot / "mods/allowed.lua"));
		REQUIRE(modsResult.valid());
		CHECK(modsResult.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, path is allowed but messy.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		auto result = sandbox.runFile(fs::path(wrkDir / "../scripts/./allowed.lua"));

		REQUIRE(result.valid());
		CHECK(result.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File does not exist, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		auto result = sandbox.runFile(fs::path(wrkDir / "non-existent.lua"));
		CHECK_FALSE(result.valid());
	}

	SUBCASE("File exists, path is forbidden.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		auto result = sandbox.runFile(fs::path(wrkDir / "../forbidden.lua"));
		CHECK_FALSE(result.valid());
	}

	SUBCASE("File exists, but sandbox has no allowed path.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom);

		auto result = sandbox.runFile(fs::path(wrkDir / "allowed.lua"));
		CHECK_FALSE(result.valid());
	}

	SUBCASE("Trying to load precompiled bytecode.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		REQUIRE(createBytecodeFile(wrkDir / "bytecode.lua"));

		auto result = sandbox.runFile(fs::path(wrkDir / "bytecode.lua"));
		CHECK_FALSE(result.valid());
	}
}

TEST_CASE("LuaRuntime sandbox runs a script file: Lua side.")
{
	LuaRuntime lua;

	const auto tmpDir = TempDir();
	const auto wrkDir = fs::absolute(tmpDir.path / "scripts");
	fs::create_directories(wrkDir / "modules");

	REQUIRE(createScriptFile(wrkDir / "script.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "../forbidden.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "modules/module.lua", files::module));

	SUBCASE("File exists, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = dofile("script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, path is allowed but messy")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = dofile("../scripts/./script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File does not exist, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = dofile("non-existent.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}

	SUBCASE("File exists, path is forbidden.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = dofile("../forbidden.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}

	SUBCASE("Load existed script file as a module.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(
			dofile("script.lua")
			barSetter = require("modules/module.lua")
			before = bar;
			barSetter(13)
			after = bar
		)");
		CHECK(sandbox["before"] == 42);
		CHECK(sandbox["after"] == 13);
	}
}

TEST_CASE("LuaRuntime sandbox using 'require' to load files and standard libs: Lua side.")
{
	LuaRuntime lua;

	const auto tmpDir = TempDir();
	const auto wrkDir = fs::absolute(tmpDir.path / "scripts");
	fs::create_directories(wrkDir / "modules");

	REQUIRE(createScriptFile(wrkDir / "script.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "../forbidden.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "modules/module.lua", files::module));

	SUBCASE("File exists, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = require("script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, path is allowed but messy")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = require("../scripts/./script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File does not exist, path is allowed.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = require("non-existent.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}

	SUBCASE("File exists, path is forbidden.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(result = require("../forbidden.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}

	SUBCASE("Load existed script file as a module.")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(
			require("script.lua")
			barSetter = require("modules/module.lua")
			before = bar;
			barSetter(13)
			after = bar
		)");
		CHECK(sandbox["before"] == 42);
		CHECK(sandbox["after"] == 13);
	}

	SUBCASE("Load library as module")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Custom, wrkDir, {wrkDir});

		sandbox.run(R"(
			math = require("math")
			require ("string")
			maxValue = math.max(10, 15, 9)
			stringLen = string.len("foobar")
		)");
		CHECK(sandbox["maxValue"] == 15);
		CHECK(sandbox["stringLen"] == 6);
	}

	SUBCASE("Load forbidden library as module")
	{
		LuaSandbox sandbox(lua, LuaSandbox::Presets::Core, wrkDir, {wrkDir});

		sandbox.run(R"(
			math = require("math")
			require ("string")
		)");
		CHECK_FALSE(sandbox["math"].valid());
		CHECK_FALSE(sandbox["string"].valid());
	}
}
