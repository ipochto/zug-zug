#include <doctest/doctest.h>
#include "scripts/lua/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

struct TempDir {
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

	void clear()
	{
		fs::remove_all(path);
	}

	std::string randomString(size_t length) {
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
}

TEST_CASE("LuaRuntime sandbox runs a script file: Cpp side.") {

	LuaState lua;

	const auto tmpDir = TempDir();
	const auto wrkDir = fs::absolute(tmpDir.path / "scripts");
	fs::create_directories(wrkDir);

	REQUIRE(createScriptFile(wrkDir / "allowed.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "../forbidden.lua", files::script));

	SUBCASE("File exists, path is allowed.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		auto result = sandbox.runFile(fs::path(wrkDir / "allowed.lua"));
		REQUIRE(result.valid());
		CHECK(result.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, path is allowed but messy.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		auto result = sandbox.runFile(fs::path(wrkDir / "../scripts/./allowed.lua"));

		REQUIRE(result.valid());
		CHECK(result.get<std::string>() == "foo");
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File does not exist, path is allowed.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		auto result = sandbox.runFile(fs::path(wrkDir / "non-existent.lua"));
		CHECK_FALSE(result.valid());
	}
	
	SUBCASE("File exists, path is forbidden.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		auto result = sandbox.runFile(fs::path(wrkDir / "../forbidden.lua"));
		CHECK_FALSE(result.valid());
	}

	SUBCASE("Trying to load precompiled bytecode.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);
		
		REQUIRE(createBytecodeFile(wrkDir / "bytecode.lua"));

		auto result = sandbox.runFile(fs::path(wrkDir / "bytecode.lua"));
		CHECK_FALSE(result.valid());
	}
}

TEST_CASE("LuaRuntime sandbox runs a script file: Lua side.") {

	LuaState lua;

	const auto tmpDir = TempDir();
	const auto wrkDir = fs::absolute(tmpDir.path / "scripts");
	fs::create_directories(wrkDir / "modules");

	REQUIRE(createScriptFile(wrkDir / "script.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "../forbidden.lua", files::script));
	REQUIRE(createScriptFile(wrkDir / "modules/module.lua", files::module));

	SUBCASE("File exists, path is allowed.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		sandbox.run(R"(result = dofile("script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File exists, path is allowed but messy") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		sandbox.run(R"(result = dofile("../scripts/./script.lua"))");
		REQUIRE(sandbox["result"].valid());
		CHECK(sandbox["result"] == std::string("foo"));
		CHECK(sandbox["bar"] == 42);
	}

	SUBCASE("File does not exist, path is allowed.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		sandbox.run(R"(result = dofile("non-existent.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}
	
	SUBCASE("File exists, path is forbidden.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

		sandbox.run(R"(result = dofile("../forbidden.lua"))");
		CHECK(sandbox["result"] == sol::nil);
	}	

	SUBCASE("Load existed script file as a module.") {
		LuaRuntime sandbox(lua, LuaRuntime::Presets::Custom, wrkDir);

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
