#include "lua/runtime.hpp"

#include <fmt/core.h>
#include <fstream>
#include <ranges>
#include <spdlog/spdlog.h>

// clang-format off
const LuaRuntime::SandboxPresets
LuaRuntime::sandboxPresets{
	{Presets::Core, {}},
	{Presets::Minimal,
		{sol::lib::base,
		 sol::lib::table}},
	{Presets::Complete,
		{sol::lib::base,
		 sol::lib::coroutine,
		 sol::lib::math,
		 sol::lib::os,
		 sol::lib::string,
		 sol::lib::table}},
	{Presets::Custom, {}}
};

const LuaRuntime::LibsSandboxingRulesMap
LuaRuntime::libsSandboxingRules{
	{sol::lib::base,
		{.allowed = {"assert", "error", "ipairs", "next", "pairs", "pcall", "select",
					 "tonumber", "tostring", "type", "unpack", "_VERSION", "xpcall"}}},
	{sol::lib::coroutine,
		{.allowedAllExceptRestricted = true}},
	{sol::lib::math,
		{.allowedAllExceptRestricted = true,
		 .restricted = {"random", "randomseed"}}},
	{sol::lib::os,
		{.allowed = {"clock", "difftime", "time"}}},
	{sol::lib::string,
		{.allowedAllExceptRestricted = true,
		 .restricted = {"dump"}}},
	{sol::lib::table,
		{.allowedAllExceptRestricted = true}}
};
// clang-format on

namespace lua
{
	namespace details
	{
		using LibsLookupTable = std::unordered_map<sol::lib, std::string_view>;

		// clang-format off
		const auto libsLookupTable = LibsLookupTable {
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
		// clang-format on

	} // namespace details

	auto libName(sol::lib lib) noexcept -> std::optional<std::string_view>
	{
		const auto &names = lua::details::libsLookupTable;
		if (auto it = names.find(lib); it != names.end()) {
			return it->second;
		}
		return std::nullopt;
	}

	auto libByName(std::string_view libName) noexcept -> std::optional<sol::lib>
	{
		for (const auto &[lib, name] : lua::details::libsLookupTable) {
			if (name == libName) {
				return lib;
			}
		}
		return std::nullopt;
	}

	auto toString(sol::object obj) -> std::string
	{
		sol::state_view lua(obj.lua_state());
		if (!lua["tostring"].valid()) {
			return {};
		}
		return lua["tostring"](obj).get<std::string>();
	}

	bool isBytecode(const fs::path &file)
	{
		constexpr auto signature = std::string_view(LUA_SIGNATURE);

		auto ifs = std::ifstream(file, std::ios::binary);
		if (!ifs) {
			return false;
		}
		auto header = std::array<char, signature.size()>{};
		ifs.read(header.data(), header.size());
		if (ifs.gcount() < static_cast<std::streamsize>(header.size())) {
			return false;
		}
		return std::ranges::equal(header, signature);
	}

	auto makeFnCallResult(sol::state &lua,
						  const auto &object,
						  sol::call_status callStatus /* = sol::call_status::ok */)
		-> sol::protected_function_result
	{
		bool isResultValid = callStatus == sol::call_status::ok;
		sol::stack::push(lua, object);
		return sol::protected_function_result(lua, -1, isResultValid ? 1 : 0, 1, callStatus);
	}
} // namespace lua

bool LuaState::require(sol::lib lib)
{
	if (!isLibraryLoaded(lib)) {
		loadLibrary(lib);
	}
	return isLibraryLoaded(lib);
}

void LuaState::loadLibrary(sol::lib lib)
{
	state.open_libraries(lib);
	loadedLibs.insert(lib);
}

void LuaRuntime::reset(bool doCollectGrbg /* = false */)
{
	sandbox = sol::environment(lua->state, sol::create);
	sandbox["_G"] = sandbox;

	if (loadedLibs.empty()) {
		loadLibs(sandboxPresets.at(preset));
	} else {
		loadLibs(loadedLibs);
	}
	loadSafePrint();
	loadSafeExternalScriptFilesRoutine();

	if (doCollectGrbg) {
		lua->state.collect_garbage();
	}
}

auto LuaRuntime::run(std::string_view script) -> sol::protected_function_result
{
	return lua->state.safe_script(script, sandbox);
}

auto LuaRuntime::runFile(const fs::path &scriptFile) -> sol::protected_function_result
{
	auto error = [this, &scriptFile](std::string_view msg) {
		const auto errMsg = fmt::format("{}: {}", msg, scriptFile.string());
		spdlog::error("{}", errMsg);
		return lua::makeFnCallResult(lua->state, errMsg, sol::call_status::file);
	};

	if (!fs::exists(scriptFile)) {
		return error("Attempting to run a non-existent script");
	}
	if (!isPathAllowed(scriptFile)) {
		return error("Attempting to run a script outside the allowed path");
	}
	if (lua::isBytecode(scriptFile)) {
		return error("Attempting to run precompiled Lua bytecode");
	}
	return lua->state.safe_script_file(scriptFile.string(), sandbox);
}

bool LuaRuntime::require(sol::lib lib)
{
	if (preset == Presets::Custom) {
		return loadLib(lib);
	}
	return false;
}

auto LuaRuntime::dofile(sol::stack_object fileName) -> sol::protected_function_result
{
	auto nil = [this]() { return lua::makeFnCallResult(lua->state, sol::nil); };

	if (!fileName.is<std::string>()) {
		return nil();
	}
	const auto filePath = toScriptPath(fileName.as<std::string>());
	if (auto result = runFile(filePath); result.valid()) {
		return result;
	}
	return nil();
}

void LuaRuntime::loadSafeExternalScriptFilesRoutine()
{
	sandbox.set_function("dofile", &LuaRuntime::dofile, this);
	sandbox["require"] = sandbox["dofile"];
}

bool LuaRuntime::loadSafePrint()
{
	if (!lua->require(sol::lib::base)) {
		return false;
	}
	sandbox.set_function("print", &LuaRuntime::print, this);
	return true;
}

auto LuaRuntime::checkRulesFor(sol::lib lib) const noexcept -> opt_cref<LibSymbolsRules>
{
	if (const auto it = libsSandboxingRules.find(lib); it != libsSandboxingRules.end()) {
		return it->second;
	}
	return std::nullopt;
}

bool LuaRuntime::loadLib(sol::lib lib)
{
	const auto rules = checkRulesFor(lib);
	if (!rules) {
		return false;
	}
	if (!lua->require(lib)) {
		return false;
	}
	addLibToSandbox(lib, *rules);
	loadedLibs.insert(lib);
	return true;
}

void LuaRuntime::addLibToSandbox(sol::lib lib, const LibSymbolsRules &rules)
{
	const auto libName = lua::libName(lib);
	if (!libName) {
		return;
	}
	const auto libLookupName = (lib == sol::lib::base) ? "_G" : *libName;

	const sol::table src = lua->state[libLookupName];
	if (!src.valid()) {
		return;
	}
	if (libLookupName != "_G") {
		sandbox[libLookupName] = sol::table(lua->state, sol::create);
	}

	sol::table dst = sandbox[libLookupName];

	if (rules.allowedAllExceptRestricted) {
		for (const auto &[name, object] : src) {
			dst[name] = object;
		}
		for (const auto &name : rules.restricted) {
			dst[name] = sol::nil;
		}
	} else {
		for (const auto &name : rules.allowed) {
			dst[name] = src[name];
		}
	}
}

void LuaRuntime::allowScriptPath(const fs::path &path)
{
	if (scriptsRoot.empty() || path.empty()) {
		return;
	}
	const auto allow = path.is_relative() ? scriptsRoot / path : path;
	allowedScriptPaths.push_back(fs_utils::normalize(allow));
}

void LuaRuntime::setPathsForScripts(const fs::path &root, const Paths &allowed)
{
	if (root.empty() || root.is_relative()) {
		scriptsRoot.clear();
		allowedScriptPaths.clear();
		return;
	}
	scriptsRoot = fs_utils::normalize(root);

	allowedScriptPaths.clear();
	for (const auto &path : allowed) {
		allowScriptPath(path);
	}
}

auto LuaRuntime::toScriptPath(const std::string &fileName) const -> fs::path
{
	auto scriptPath = fs::path(fileName);
	if (scriptPath.is_relative()) {
		scriptPath = scriptsRoot / scriptPath;
	}
	return scriptPath.lexically_normal();
}

void LuaRuntime::print(sol::variadic_args args)
{
	std::string result;
	for (auto &&arg : args) {
		result += lua::toString(arg) + " ";
	}
	if (!result.empty()) {
		result.pop_back(); // remove last space separator
	}
	fmt::println("[lua sandbox]:> {}", result);
}
