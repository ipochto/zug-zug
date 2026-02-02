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

auto LuaSandbox::run(std::string_view script)
	-> sol::protected_function_result
{
	return runtime->state.safe_script(script, sandbox);
}

auto LuaSandbox::checkIfAllowedToLoad(const fs::path &scriptFile) const
	-> std::tuple<bool, std::string_view>
{
	if (!fs::exists(scriptFile)) {
		return {false, "Attempting to run a non-existent script"};
	}
	if (!isPathAllowed(scriptFile)) {
		return {false, "Attempting to run a script outside the allowed path"};
	}
	if (lua::isBytecode(scriptFile)) {
		return {false, "Attempting to run precompiled Lua bytecode"};
	}
	return {true, {}};
}

auto LuaSandbox::runFile(const fs::path &scriptFile)
	-> sol::protected_function_result
{
	auto error = [&](std::string_view msg) {
		const auto errMsg = std::format("{}: {}", msg, scriptFile.string());
		spdlog::error("{}", errMsg);
		return lua::makeFnCallResult(runtime->state, errMsg, sol::call_status::file);
	};

	if (const auto [isFileOk, errMsg] = checkIfAllowedToLoad(scriptFile); !isFileOk) {
		return error(errMsg);
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

auto LuaSandbox::loadfileReplace(sol::stack_object fileName)
	-> ResultOrErrorMsg
{
	auto lua = sol::state_view(runtime->state.lua_state());

	auto makeError = [&](std::string_view errMsg) -> ResultOrErrorMsg {
		return {sol::nil, sol::make_object(lua, errMsg)};
	};

	if (!fileName.is<std::string>()) {
		return makeError("Bad argument #1 to 'loadfile' (string expected)");

	}
	const auto filePath = toScriptPath(fileName.as<std::string>());
	const auto &[isFileOk, fileErrMsg] = checkIfAllowedToLoad(filePath);

	if (!isFileOk) {
		return makeError(fileErrMsg);
	}
	auto loadResult = lua.load_file(filePath.string(), sol::load_mode::text);
	if (!loadResult.valid()) {
		sol::error err = loadResult;
		return makeError(err.what());
	}
	auto chunk = sol::protected_function(loadResult);
	sandbox.set_on(chunk);

	return { sol::make_object(lua, chunk), sol::nil };
}

auto LuaSandbox::dofileReplace(sol::stack_object fileName)
	-> sol::protected_function_result
{
	if (!fileName.is<std::string>()) {
		spdlog::error("Unable to execute dofile. Error: bad argument, string expected.");
		return {};
	}
	const auto filePath = toScriptPath(fileName.as<std::string>());

	auto scriptResult = runFile(filePath);
	if (!scriptResult.valid()) {
		sol::error err = scriptResult;
		spdlog::error(R"(Unable to execute dofile("{}"). Error: "{}")", 
					  fileName.as<std::string>(),
					  err.what());
		return {};
	}
	return scriptResult;
}

auto LuaSandbox::dofileSafe(sol::stack_object fileName)
	-> sol::variadic_results
{
	auto lua = sol::state_view(runtime->state.lua_state());

	auto result = sol::variadic_results {};

    auto makeError = [&](const std::string &msgError) {
        result.push_back (sol::make_object(lua, false));
        result.push_back(sol::make_object(lua, msgError));
        return result;
    };

	auto [chunk, error] = loadfileReplace(fileName);
	if (!chunk.valid()) {
		const auto msgError = std::format(R"(Unable to load script "{}". Error: "{}")",
										  fileName.as<std::string>(),
										  error.as<std::string>());
		return makeError(msgError);
	}
	auto fn = chunk.as<sol::protected_function>();
	auto scriptResult = fn();
	if (!scriptResult.valid()) {
		sol::error err = scriptResult;
		const auto msgError = std::format(R"(Unable to execute script "{}". Error: "{}")",
										  fileName.as<std::string>(),
										  err.what());
		return makeError(msgError);
	}
	result.push_back (sol::make_object(lua, true));
	for (auto &&value : scriptResult) {
		result.push_back(value);
	}
	return result;
}

auto LuaSandbox::requireFile(sol::stack_object fileName)
	-> ResultOrErrorMsg
{
	auto [chunk, errMsg] = loadfileReplace(fileName);
	if (!chunk.valid()) {
		return { sol::nil, errMsg };
	}
	auto function = chunk.as<sol::protected_function>();
	auto result = function();
	if (!result.valid()) {
		sol::error err = result;
		return { sol::nil, sol::make_object(runtime->state, err.what()) };
	}
	if (result.return_count() == 0) {
		return {};
	}
	return { result[0], sol::nil };
}

auto LuaSandbox::requireReplace(sol::stack_object target)
	-> sol::object
{
	if (!target.is<std::string>()) {
		spdlog::error("Unable to execute 'require'. Error: bad argument, string expected.");
		return sol::nil;
	}
	const auto possibleLibName = target.as<std::string>();
	const auto lib = lua::libByName(possibleLibName);
	if (!lib) {
		spdlog::error(R"(require("{}"): library not found.)", possibleLibName);		
		return sol::nil;
	}
	if (!require(*lib)) {
		spdlog::error(R"(require("{}"): library is forbidden.)", possibleLibName);
		return sol::nil;
	}
	const auto libLookupName = lua::libLookupName(*lib);
	return sandbox[libLookupName];
}

void LuaSandbox::loadSafeExternalScriptFilesRoutine()
{
	sandbox.set_function("dofile", &LuaSandbox::dofileReplace, this);
	sandbox.set_function("safe_dofile", &LuaSandbox::dofileSafe, this);

	sandbox.set_function("loadfile", &LuaSandbox::loadfileReplace, this);
	sandbox.set_function("require_file", &LuaSandbox::requireFile, this);
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

bool LuaSandbox::allowScriptPath(const fs::path &path)
{
	if (scriptsRoot.empty() || path.empty()) {
		return false;
	}
	if (path.empty()
		|| (scriptsRoot.empty() && path.is_relative())) {
		return false;
	}
	const auto allow = path.is_relative() ? scriptsRoot / path : path;
	allowedScriptPaths.push_back(fs_utils::normalize(allow));
	return true;
}

void LuaSandbox::setPathsForScripts(const fs::path &root, const Paths &allowed)
{
	scriptsRoot.clear();
	if (!root.empty() && root.is_absolute()) {
		scriptsRoot = fs_utils::normalize(root);
	}
	allowedScriptPaths.clear();
	for (const auto &path : allowed) {
		allowScriptPath(path);
	}
}

auto LuaSandbox::toScriptPath(const std::string &fileName)
	const -> fs::path
{
	auto scriptPath = fs::path(fileName);
	if (scriptPath.is_relative() && !scriptsRoot.empty()) {
		scriptPath = scriptsRoot / scriptPath;
	}
	return scriptPath.lexically_normal();
}

bool LuaSandbox::isPathAllowed(const fs::path &scriptFile) const
{
	if (scriptFile.empty()) {
		return false;
	}
	if (scriptFile.is_relative()) {
		if (scriptsRoot.empty()) {
			return false;
		}
		return fs_utils::startsWith(scriptsRoot / scriptFile, allowedScriptPaths);
	}
	return fs_utils::startsWith(scriptFile, allowedScriptPaths);
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
