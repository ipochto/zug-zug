#include "lua/runtime.hpp"

void LuaState::require(sol::lib lib)
{ 
	if (not isLibraryLoaded(lib)) {
		loadLibrary(lib);
	}
}

void LuaState::loadLibrary(sol::lib lib)
{ 
	state.open_libraries(lib);
	loadedLibs.insert(lib);
}


LuaRuntime::LuaRuntime(LuaState &state, Presets preset/* = Presets::Empty */)
	: lua(state)
	, sandbox(lua.state, sol::create)
	, preset(preset)
{
	sandbox["_G"] = sandbox;
	loadLibs(sandboxPresets.at(preset));
};

bool LuaRuntime::require (sol::lib lib)
{ 
	if (preset == Presets::Custom) {
		return loadLib(lib);
	}
	return false;
};

void LuaRuntime::loadLibs(const LibsList &libs)
{
	for (const auto lib : libs) {
		loadLib(lib);
	}
}

auto LuaRuntime::checkRulesFor(sol::lib lib)
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
	lua.require(lib);
	addLibToSandbox(lib, *rules);
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
