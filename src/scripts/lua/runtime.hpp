#pragma once

#include "lua/sol2.hpp"
#include "lua/utils.hpp"

#include "utils/enum_set.hpp"
#include "utils/filesystem.hpp"
#include "utils/optional_ref.hpp"

#include <map>
#include <string_view>
#include <vector>

template <typename T>
concept SolLibContainer =
	std::ranges::range<T>
	&& std::same_as<std::ranges::range_value_t<T>, sol::lib>;
/*-----------------------------------------------------------------------------------------------*/
class LuaRuntime
{
private:
	lua::memory::LimitedAllocatorState allocatorState{};
	lua::memory::Allocator allocatorFn{nullptr};

public:
	sol::state state;

private:
	enum_set<sol::lib> loadedLibs;
	lua::timeoutGuard::Watchdog timeoutGuard;


public:
	LuaRuntime()
		: state{},
		  timeoutGuard(state)
	{}

	~LuaRuntime() = default;

	LuaRuntime(size_t memoryLimit, lua::memory::Allocator fn = lua::memory::limitedAlloc)
		: allocatorState({.limit = memoryLimit}),
		  allocatorFn(fn),
		  state(sol::default_at_panic, fn, &allocatorState),
		  timeoutGuard(state)
	{}

	LuaRuntime(const LuaRuntime &) = delete;
	LuaRuntime(LuaRuntime &&) = delete;
	LuaRuntime &operator=(const LuaRuntime &) = delete;
	LuaRuntime &operator=(LuaRuntime &&) = delete;

	void reset();
	bool setMemoryLimit(size_t limit);
	void require(sol::lib lib);

	[[nodiscard]]
	auto getAllocatorState() const -> const lua::memory::LimitedAllocatorState &
	{
		return allocatorState;
	}

	[[nodiscard]]
	auto makeTimeoutGuardedScope(std::chrono::milliseconds limit)
		-> lua::timeoutGuard::GuardedScope
	{
		return lua::timeoutGuard::GuardedScope{timeoutGuard, limit};
	}
};
/*-----------------------------------------------------------------------------------------------*/
class LuaSandbox
{
public:
	enum class Presets { Core, Minimal, Complete, Custom };
	using Paths = std::vector<fs::path>;
	using ResultOrErrorMsg = std::tuple<sol::object, sol::object>;

	explicit LuaSandbox(LuaRuntime &runtime,
						Presets preset,
						const fs::path &root = {},
						const Paths &allowedPaths = {},
						std::ostream &printOutStrm = std::cout)
		: runtime(&runtime),
		  preset(preset),
		  printOutStrm(&printOutStrm)
	{
		setPathsForScripts(root, allowedPaths);
		reset();
	}
	~LuaSandbox() = default;

	LuaSandbox(const LuaSandbox &) = delete;
	LuaSandbox &operator=(const LuaSandbox &) = delete;

	LuaSandbox(LuaSandbox &&) = default;
	LuaSandbox &operator=(LuaSandbox &&) = default;

	auto operator[](auto &&key) noexcept { return sandbox[std::forward<decltype(key)>(key)]; }
	void reset(bool doCollectGrbg = false);

	auto run(std::string_view script) -> sol::protected_function_result;
	auto runFile(const fs::path &scriptFile) -> sol::protected_function_result;

	[[nodiscard]]
	bool require(sol::lib lib);
	bool allowScriptPath(const fs::path &path);

	[[nodiscard]]
	auto makeTimeoutGuardedScope(std::chrono::milliseconds limit)
		-> lua::timeoutGuard::GuardedScope
	{
		return runtime->makeTimeoutGuardedScope(limit);
	}

private:
	using LibNames = std::vector<std::string_view>;
	using Libs = std::vector<sol::lib>;
	using SandboxPresets = std::map<Presets, Libs>;

	struct LibSymbolsRules
	{
		bool allowedAllExceptRestricted{false};
		LibNames allowed {}; // This will be ignored if allowedAllExceptRestricted is set
		LibNames restricted {};
	};

	using LibsSandboxingRulesMap = std::map<sol::lib, LibSymbolsRules>;

	[[nodiscard]]
	static auto checkRulesFor(sol::lib lib) noexcept -> opt_cref<LibSymbolsRules>;

	bool loadLib(sol::lib lib);

	void loadLibs(const SolLibContainer auto &libs)
	{
		for (const auto lib : libs) {
			loadLib(lib);
		}
	}
	void copyLibFromState(sol::lib lib, const LibSymbolsRules &rules);

	void setPathsForScripts(const fs::path &root, const Paths &allowed);

	[[nodiscard]]
	auto toScriptPath(const std::string &fileName) const -> fs::path;

	[[nodiscard]]
	bool isPathAllowed(const fs::path &scriptFile) const;

	[[nodiscard]]
	auto checkIfAllowedToLoad(const fs::path &scriptFile) const
		-> std::tuple<bool, std::string_view>;

	auto loadfileReplace(sol::stack_object fileName) -> ResultOrErrorMsg;
	auto dofileReplace(sol::stack_object fileName) -> sol::protected_function_result;
	auto dofileSafe(sol::stack_object fileName) -> sol::variadic_results;
	auto requireReplace(sol::stack_object target) -> sol::object;
	auto requireFile(sol::stack_object fileName) -> ResultOrErrorMsg;

	void printReplace(sol::variadic_args args);

	void loadSafeExternalScriptFilesRoutine();
	void loadSafePrint();

private:
	LuaRuntime *runtime = {nullptr};
	sol::environment sandbox;

	Presets preset{Presets::Core};
	fs::path scriptsRoot;	// Absolute, lexically normalized path.
							// Relative paths to script files are resolved from this location.
							// If empty, loading external scripts is prohibited.
	Paths allowedScriptPaths;

	std::ostream *printOutStrm;

	enum_set<sol::lib> loadedLibs;

	static const SandboxPresets sandboxPresets;
	static const LibsSandboxingRulesMap libsSandboxingRules;
};
