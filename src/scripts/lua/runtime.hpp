#pragma once

#include <set>
#include <string_view>
#include <vector>

#include "lua/sol2.hpp"
#include "utils/filesystem.hpp"
#include "utils/optional_ref.hpp"

template <typename T>
concept SolLibContainer =
	std::ranges::range<T> &&
	std::same_as<std::remove_cvref_t<std::ranges::range_value_t<T>>, sol::lib>;

class LuaState
{
public:
	LuaState() noexcept = default;
	LuaState(const LuaState&) = delete;
	LuaState(LuaState&&) = delete;
	LuaState& operator=(const LuaState&) = delete;
	LuaState& operator=(LuaState&&) = delete;

	[[nodiscard]]
	bool require(sol::lib lib);

	sol::state state;

private:
	bool isLibraryLoaded(sol::lib lib) const noexcept { return loadedLibs.contains(lib); }
	void loadLibrary(sol::lib lib);

	std::set<sol::lib> loadedLibs;
};

// Sandboxed lua runtime
class LuaRuntime
{
public:
	enum class Presets {Base, Configs, Custom};

	explicit LuaRuntime(LuaState &state,
						Presets preset = Presets::Base,
						fs::path root = {})
		: lua(state),
		  preset(preset)
	{
		setRootForScripts(root.empty() ? fs::current_path() : root);
		reset(false);
	}
	LuaRuntime(const LuaRuntime&) = delete;
	LuaRuntime(LuaRuntime&&) = delete;
	LuaRuntime& operator=(const LuaRuntime&) = delete;
	LuaRuntime& operator=(LuaRuntime&&) = delete;

	~LuaRuntime() = default;

	auto operator[](auto &&key) noexcept {
		return sandbox[std::forward<decltype(key)>(key)];
	}
	void reset(bool doCollectGrbg = false);

	auto run(std::string_view script) { 
		return lua.state.script(script, sandbox);
	}
	auto runFile(const fs::path &scriptFile) { 
		return isPathAllowed(scriptFile) 
			   ? lua.state.script_file(scriptFile.string(), sandbox)
			   : run("return nil");
	}
	
	[[nodiscard]]
	bool require(sol::lib lib);

	void setRootForScripts(const fs::path &root) {
		scriptsRoot = fs::absolute(root).lexically_normal();
	}

private:
	using NamesList = std::vector<std::string_view>;
	using LibsList = std::vector<sol::lib>;
	using SandboxPresetsMap = std::unordered_map<Presets, LibsList>;

	struct LibSymbolsRules {
		bool allowedAllExceptRestricted {false};
		NamesList allowed {}; // This will be ignored if allowedAllExceptRestricted is set
		NamesList restricted {};
	};

	using LibsSandboxingRulesMap = std::unordered_map<sol::lib, LibSymbolsRules>;

	auto checkRulesFor(sol::lib lib) const noexcept -> opt_cref<LibSymbolsRules>;

	bool loadLib(sol::lib lib);

	void loadLibs(const SolLibContainer auto &libs) {
		for (const auto lib : libs) {
			loadLib(lib);
		}
	}
	void addLibToSandbox(sol::lib lib, const LibSymbolsRules &rules);

	[[nodiscard]]
	auto toScriptPath(const std::string &fileName) const -> fs::path;

	[[nodiscard]]
	bool isPathAllowed(const fs::path &scriptFile) const {
		return fs_utils::startsWith(scriptFile, scriptsRoot);
	}
	auto dofile(sol::stack_object fileName) {
		return runFile(toScriptPath(fileName.as<std::string>()));
	}
	void enableScriptFiles();
	bool enablePrint();
	void print(sol::variadic_args args);

	LuaState &lua;
	sol::environment sandbox;

	Presets preset {Presets::Base};
	fs::path scriptsRoot; // absolute and lexically normalized

	std::set<sol::lib> loadedLibs;

	static const SandboxPresetsMap sandboxPresets;
	static const LibsSandboxingRulesMap libsSandboxingRules;
};

inline const LuaRuntime::SandboxPresetsMap
LuaRuntime::sandboxPresets {
	{Presets::Base, {
		sol::lib::base,
		sol::lib::table}},
	{Presets::Configs, {
		sol::lib::base,
		sol::lib::table,
		sol::lib::string}},
	{Presets::Custom, {}}		
};

inline const LuaRuntime::LibsSandboxingRulesMap
LuaRuntime::libsSandboxingRules {
	{sol::lib::base, {
		.allowed = {"assert", "error", "ipairs", "next", "pairs", "pcall", "select",
					"tonumber", "tostring", "type", "unpack", "_VERSION", "xpcall", "print"}}},
	{sol::lib::coroutine, {
		.allowedAllExceptRestricted = true}},
	{sol::lib::math, {
		.allowedAllExceptRestricted = true,
		.restricted = {"random", "randomseed"}}},
	{sol::lib::os, {
		.allowed = {"clock", "date", "difftime", "time"}}},
	{sol::lib::string, {
		.allowedAllExceptRestricted = true,
		.restricted = {"dump"}}},
	{sol::lib::table, {
		.allowedAllExceptRestricted = true}}
};

namespace lua
{
	namespace details
	{
		using LibsLookupTable = std::unordered_map<sol::lib, std::string_view>;

		inline const LibsLookupTable libsLookupTable = {
			{sol::lib::base,		"base"},
			{sol::lib::bit32,		"bit32"}, // Lua 5.2+
			{sol::lib::coroutine,	"coroutine"},
			{sol::lib::debug,		"debug"},
			{sol::lib::ffi,			"ffi"}, // LuaJIT only
			{sol::lib::io,			"io"},
			{sol::lib::jit,			"jit"}, // LuaJIT only
			{sol::lib::math,		"math"},
			{sol::lib::os,			"os"},
			{sol::lib::package,		"package"},
			{sol::lib::string,		"string"},
			{sol::lib::table,		"table"},
			{sol::lib::utf8,		"utf8"} // Lua 5.3+
		};
	}
	[[nodiscard]]
	auto libName(sol::lib lib) noexcept -> std::optional<std::string_view>;

	[[nodiscard]]
	auto libByName(std::string_view libName) noexcept -> std::optional<sol::lib>;

	[[nodiscard]]
	auto toString(sol::object obj)-> std::string;
}
