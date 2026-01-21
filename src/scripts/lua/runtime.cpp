#include "lua/runtime.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>

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

void LuaRuntime::reset()
{
	if (allocatorState.isActivated()) {
		const auto currentLimit = allocatorState.limit;
		allocatorState.disableLimit();

		state = sol::state(sol::default_at_panic, lua::memory::limitedAlloc, &allocatorState);

		allocatorState = {.used = allocatorState.used, .limit = currentLimit};
	} else {
		state = sol::state();
	}
}

bool LuaRuntime::setMemoryLimit(size_t limit)
{
	if (allocatorState.isActivated()) {
		allocatorState.limit = limit;
	}
	return allocatorState.isActivated();
}

void LuaRuntime::require(sol::lib lib)
{
	if (!loadedLibs.contains(lib)) {
		state.open_libraries(lib);
		loadedLibs.insert(lib);
	}
}

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
