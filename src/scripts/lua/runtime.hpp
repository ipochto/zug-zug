#pragma once

#include <set>
#include <string_view>
#include <vector>

#include "lua/sol2.hpp"
#include "utils/filesystem.hpp"
#include "utils/optional_ref.hpp"


class LuaState
{
public:
	LuaState() noexcept = default;
	LuaState(const LuaState&) = delete;
	LuaState(const LuaState&&) = delete;
	LuaState& operator=(const LuaState&) = delete;
	LuaState& operator=(const LuaState&&) = delete;

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

	explicit LuaRuntime(LuaState &state, Presets preset = Presets::Base);
	LuaRuntime(const LuaRuntime&) = delete;
	LuaRuntime(const LuaRuntime&&) = delete;
	LuaRuntime& operator=(const LuaRuntime&) = delete;
	LuaRuntime& operator=(const LuaRuntime&&) = delete;

	~LuaRuntime() = default;

	template <typename Key>
	auto operator[](Key&& key) noexcept {
		return sandbox[std::forward<Key>(key)];
	}
	auto run(std::string_view script) { 
		return lua.state.script(script, sandbox);
	}
	auto runFile(const fs::path &scriptFile) { 
		return lua.state.script_file(scriptFile.string(), sandbox);
	}
	
	[[nodiscard]]
	bool require(sol::lib lib);

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
	void loadLibs(const LibsList &names);
	bool loadLib(sol::lib lib);
	void addLibToSandbox(sol::lib lib, const LibSymbolsRules &rules);
	
	//	sol::function_result loadfile(sol::stack_object file);
	//	sol::function_result dofile(sol::stack_object file);

	LuaState &lua;
	sol::environment sandbox;
	std::set<sol::lib> loadedLibs;
	Presets preset;

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
	inline auto libName(sol::lib lib) noexcept
		-> std::optional<std::string_view>
	{
		const auto &names = lua::details::libsLookupTable;
		const auto it = names.find(lib);
		if (it == names.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	[[nodiscard]]
	inline auto libByName(std::string_view libName) noexcept
		-> std::optional<sol::lib>
	{
		for (const auto [lib, name] : lua::details::libsLookupTable) {
			if (name == libName) {
				return lib;
			}
		}
		return std::nullopt;
	}
}
