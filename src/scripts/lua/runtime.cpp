#include "lua/runtime.hpp"

#include <array>
#include <fmt/core.h>
#include <fstream>
#include <ranges>
#include <spdlog/spdlog.h>

namespace ranges = std::ranges;

// clang-format off
const LuaSandbox::SandboxPresets
LuaSandbox::sandboxPresets{
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

const LuaSandbox::LibsSandboxingRulesMap
LuaSandbox::libsSandboxingRules{
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
		struct LibName
		{
			sol::lib lib;
			std::string_view name;
		};
		// clang-format off
		constexpr auto libsNames = std::to_array<LibName> ({
			{sol::lib::base,      "base"},
			{sol::lib::bit32,     "bit32"}, // Lua 5.2+
			{sol::lib::coroutine, "coroutine"},
			{sol::lib::debug,     "debug"},
			{sol::lib::ffi,       "ffi"}, // LuaJIT only
			{sol::lib::io,        "io"},
			{sol::lib::jit,       "jit"}, // LuaJIT only
			{sol::lib::math,      "math"},
			{sol::lib::os,        "os"},
			{sol::lib::package,   "package"},
			{sol::lib::string,    "string"},
			{sol::lib::table,     "table"},
			{sol::lib::utf8,      "utf8"} // Lua 5.3+
		});
		// clang-format on
	} // namespace details

	constexpr auto libName(sol::lib lib) noexcept -> std::optional<std::string_view>
	{
		auto findLib = [lib](auto &lookup) -> bool { return lookup.lib == lib; };
		
		const auto &libs = lua::details::libsNames;
		if (const auto it = ranges::find_if(libs, findLib); it != libs.end()) {
			return it->name;
		}
		return std::nullopt;
	}

	constexpr auto libByName(std::string_view libName) noexcept -> std::optional<sol::lib>
	{
		auto findLibName = [libName](auto &lookup) -> bool { return lookup.name == libName; };

		const auto &libs = lua::details::libsNames;
		if (const auto it = ranges::find_if(libs, findLibName); it != libs.end()) {
			return it->lib;
		}
		return std::nullopt;
	}

	constexpr auto libLookupName(sol::lib lib) noexcept -> std::string_view
	{
		return (lib == sol::lib::base) ? "_G" : lua::libName(lib).value_or("");
	}

	auto toString(const sol::object &obj) -> std::string
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
		return ranges::equal(header, signature);
	}

	auto makeFnCallResult(sol::state &lua,
						  const auto &object,
						  sol::call_status callStatus /* = sol::call_status::ok */)
		-> sol::protected_function_result
	{
		bool isResultValid = callStatus == sol::call_status::ok;
		sol::stack::push(lua, object);
		return sol::protected_function_result {lua, -1, isResultValid ? 1 : 0, 1, callStatus};
	}
} // namespace lua

void LuaSandbox::reset(bool doCollectGrbg /* = false */)
{
	sandbox = sol::environment(runtime->state, sol::create);
	sandbox["_G"] = sandbox;

	if (loadedLibs.empty()) {
		loadLibs(sandboxPresets.at(preset));
	} else {
		loadLibs(loadedLibs);
	}
	loadSafePrint();
	loadSafeExternalScriptFilesRoutine();

	if (doCollectGrbg) {
		runtime->state.collect_garbage();
	}
}

auto LuaSandbox::run(std::string_view script) -> sol::protected_function_result
{
	return runtime->state.safe_script(script, sandbox);
}

auto LuaSandbox::runFile(const fs::path &scriptFile) -> sol::protected_function_result
{
	auto error = [this, &scriptFile](std::string_view msg) {
		const auto errMsg = fmt::format("{}: {}", msg, scriptFile.string());
		spdlog::error("{}", errMsg);
		return lua::makeFnCallResult(runtime->state, errMsg, sol::call_status::file);
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
	return runtime->state.safe_script_file(scriptFile.string(), sandbox);
}

bool LuaSandbox::require(sol::lib lib)
{
	if (preset == Presets::Custom) {
		return loadLib(lib);
	}
	return false;
}

auto LuaSandbox::dofileReplace(sol::stack_object fileName) -> sol::protected_function_result
{
	auto nil = [this]() { return lua::makeFnCallResult(runtime->state, sol::nil); };

	if (!fileName.is<std::string>()) {
		return nil();
	}
	const auto filePath = toScriptPath(fileName.as<std::string>());
	if (auto result = runFile(filePath); result.valid()) {
		return result;
	}
	return nil();
}

auto LuaSandbox::requireReplace(sol::stack_object target) -> sol::protected_function_result
{
	auto nil = [this]() { return lua::makeFnCallResult(runtime->state, sol::nil); };

	if (!target.is<std::string>()) {
		return nil();
	}
	const auto possibleLibName = target.as<std::string>();
	if (const auto lib = lua::libByName(possibleLibName); lib.has_value()) {
		if (require(*lib)) {
			const auto libLookupName = lua::libLookupName(*lib);
			return lua::makeFnCallResult(runtime->state, sandbox[libLookupName]);
		}
		return nil();
	}
	return dofileReplace(target);
}

void LuaSandbox::loadSafeExternalScriptFilesRoutine()
{
	sandbox.set_function("dofile", &LuaSandbox::dofileReplace, this);
	sandbox.set_function("require", &LuaSandbox::requireReplace, this);
}

void LuaSandbox::loadSafePrint()
{
	runtime->require(sol::lib::base);
	sandbox.set_function("print", &LuaSandbox::printReplace, this);
}

auto LuaSandbox::checkRulesFor(sol::lib lib) noexcept -> opt_cref<LibSymbolsRules>
{
	if (const auto it = libsSandboxingRules.find(lib); it != libsSandboxingRules.end()) {
		return it->second;
	}
	return std::nullopt;
}

bool LuaSandbox::loadLib(sol::lib lib)
{
	const auto rules = checkRulesFor(lib);
	if (!rules) {
		return false;
	}
	runtime->require(lib);

	copyLibFromState(lib, *rules);
	loadedLibs.insert(lib);
	return true;
}

void LuaSandbox::copyLibFromState(sol::lib lib, const LibSymbolsRules &rules)
{
	const auto libLookupName = lua::libLookupName(lib);
	if (libLookupName.empty()) {
		return;
	}
	const sol::table src = runtime->state[libLookupName];
	if (!src.valid()) {
		return;
	}
	if (lib != sol::lib::base) { // The 'base' is loaded directly into '_G', which already exists
		sandbox[libLookupName] = sol::table(runtime->state, sol::create);
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

void LuaSandbox::allowScriptPath(const fs::path &path)
{
	if (scriptsRoot.empty() || path.empty()) {
		return;
	}
	const auto allow = path.is_relative() ? scriptsRoot / path : path;
	allowedScriptPaths.push_back(fs_utils::normalize(allow));
}

void LuaSandbox::setPathsForScripts(const fs::path &root, const Paths &allowed)
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

auto LuaSandbox::toScriptPath(const std::string &fileName) const -> fs::path
{
	auto scriptPath = fs::path(fileName);
	if (scriptPath.is_relative()) {
		scriptPath = scriptsRoot / scriptPath;
	}
	return scriptPath.lexically_normal();
}

void LuaSandbox::printReplace(sol::variadic_args args)
{
	std::string result;
	for (auto &&arg : args) {
		result += lua::toString(arg);
		result += " ";
	}
	if (!result.empty()) {
		result.pop_back(); // remove last space separator
	}
	*printOutStrm << "[lua sandbox]:> " << result << "\n";
}
