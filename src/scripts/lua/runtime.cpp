#include <fmt/core.h>
#include <fstream>
#include <ranges>
#include <spdlog/spdlog.h>

#include "lua/runtime.hpp"

namespace lua
{
	auto libName(sol::lib lib) noexcept
		-> std::optional<std::string_view>
	{
		const auto &names = lua::details::libsLookupTable;
		const auto it = names.find(lib);
		if (it == names.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	auto libByName(std::string_view libName) noexcept
		-> std::optional<sol::lib>
	{
		for (const auto [lib, name] : lua::details::libsLookupTable) {
			if (name == libName) {
				return lib;
			}
		}
		return std::nullopt;
	}

	auto toString(sol::object obj)
		-> std::string
	{
		sol::state_view lua(obj.lua_state());
		if (!lua["tostring"].valid()) {
			// log error
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
		std::array<char, signature.size()> header{};
		ifs.read(header.data(), header.size());
		if (ifs.gcount() < static_cast<std::streamsize>(header.size())) {
			return false; 
		}
		return std::ranges::equal(header, signature);
	}
}

bool LuaState::require(sol::lib lib)
{ 
	if (not isLibraryLoaded(lib)) {
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
	sandbox = sol::environment(lua.state, sol::create);
	sandbox["_G"] = sandbox;

	if (loadedLibs.empty()) {
		loadLibs(sandboxPresets.at(preset));
	} else {
		loadLibs(loadedLibs);
	}
	loadSafePrint();
	loadSafeExternalScriptFilesRoutine();

	if(doCollectGrbg) {
		lua.state.collect_garbage();
	}
}

auto LuaRuntime::run(std::string_view script)
	-> sol::protected_function_result
{
	return lua.state.script(script, sandbox);
}

auto LuaRuntime::runFile(const fs::path &scriptFile)
	-> sol::protected_function_result
{
	if (!fs::exists(scriptFile)) {
		spdlog::error("Attempting to run a non-existent script: {}", scriptFile.string());
		return run("return nil");
	}
	if (!isPathAllowed(scriptFile)) {
		spdlog::error("Attempting to run a script outside the allowed path: {}", 
					  scriptFile.string());
		return run("return nil");
	}
	if (lua::isBytecode(scriptFile)) {
		spdlog::error("Attempting to run precompiled Lua bytecode: {}", 
					  scriptFile.string());
		return run("return nil");
	}
	return lua.state.script_file(scriptFile.string(), sandbox);
}

bool LuaRuntime::require (sol::lib lib)
{ 
	if (preset == Presets::Custom) {
		return loadLib(lib);
	}
	return false;
};

void LuaRuntime::loadSafeExternalScriptFilesRoutine()
{
	sandbox.set_function("dofile", &LuaRuntime::dofile, this);
	sandbox["require"] = sandbox["dofile"];
}

bool LuaRuntime::loadSafePrint()
{
	if (!lua.require(sol::lib::base)) {
		return false;
	}
	sandbox.set_function("print", &LuaRuntime::print, this);
	return true;
}

auto LuaRuntime::checkRulesFor(sol::lib lib) const noexcept
	-> opt_cref<LibSymbolsRules> 
{
	const auto it = libsSandboxingRules.find(lib);
	if (it == libsSandboxingRules.end()) {
		return  std::nullopt;
	}
	return it->second;
}

bool LuaRuntime::loadLib(sol::lib lib)
{
	const auto rules = checkRulesFor(lib);
	if (!rules) {
		return false;
	}
	if (!lua.require(lib)) {
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
	
	const sol::table src = lua.state[libLookupName];
	if (!src.valid()) {
		return;
	}
	if (libLookupName != "_G") {
		sandbox[libLookupName] = sol::table(lua.state, sol::create);
	}
	
	sol::table dst = sandbox[libLookupName];

	if (rules.allowedAllExceptRestricted) { 
		for (const auto &[name, object]: src) {
			dst[name] = object;
		}
	} else {
		for (const auto &name : rules.allowed) {
			dst[name] = src[name];
		}
	}
	for (const auto &name : rules.restricted) {
		dst[name] = sol::nil;
	}
}

auto LuaRuntime::toScriptPath(const std::string &fileName) const
	-> fs::path
{
	auto scriptPath {fs::path(fileName)};
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
