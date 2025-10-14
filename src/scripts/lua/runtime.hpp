#pragma once

#include "lua/sol2.hpp"
#include "utils/filesystem.hpp"
#include "utils/optional_ref.hpp"

#include <set>
#include <string_view>
#include <vector>

template <typename T>
concept SolLibContainer =
	std::ranges::range<T>
	&& std::same_as<std::remove_cvref_t<std::ranges::range_value_t<T>>, sol::lib>;

class LuaState
{
public:
	LuaState() noexcept = default;
	LuaState(const LuaState &) = delete;
	LuaState(LuaState &&) = delete;
	LuaState &operator=(const LuaState &) = delete;
	LuaState &operator=(LuaState &&) = delete;

	[[nodiscard]]
	bool require(sol::lib lib);

	sol::state state;

private:
	bool isLibraryLoaded(sol::lib lib) const noexcept { return loadedLibs.contains(lib); }
	void loadLibrary(sol::lib lib);

private:
	std::set<sol::lib> loadedLibs;
};

// Sandboxed lua runtime
class LuaRuntime
{
public:
	enum class Presets { Base, Configs, Custom };
	using Paths = std::vector<fs::path>;

	explicit LuaRuntime(LuaState &state,
						Presets preset,
						const fs::path &root = {},
						const Paths &allowedPaths = {})
		: lua(state),
		  preset(preset)
	{
		setPathsForScripts(root, allowedPaths);
		reset(false);
	}

	LuaRuntime(const LuaRuntime &) = delete;
	LuaRuntime(LuaRuntime &&) = delete;
	LuaRuntime &operator=(const LuaRuntime &) = delete;
	LuaRuntime &operator=(LuaRuntime &&) = delete;

	~LuaRuntime() = default;

	auto operator[](auto &&key) noexcept { return sandbox[std::forward<decltype(key)>(key)]; }
	void reset(bool doCollectGrbg = false);

	auto run(std::string_view script) -> sol::protected_function_result;
	auto runFile(const fs::path &scriptFile) -> sol::protected_function_result;

	[[nodiscard]]
	bool require(sol::lib lib);
	void allowScriptPath(const fs::path &path);

private:
	using LibNames = std::vector<std::string_view>;
	using Libs = std::vector<sol::lib>;
	using SandboxPresets = std::unordered_map<Presets, Libs>;

	struct LibSymbolsRules
	{
		bool allowedAllExceptRestricted{false};
		LibNames allowed{}; // This will be ignored if allowedAllExceptRestricted is set
		LibNames restricted{};
	};

	using LibsSandboxingRulesMap = std::unordered_map<sol::lib, LibSymbolsRules>;

	auto checkRulesFor(sol::lib lib) const noexcept -> opt_cref<LibSymbolsRules>;

	bool loadLib(sol::lib lib);

	void loadLibs(const SolLibContainer auto &libs)
	{
		for (const auto lib : libs) {
			loadLib(lib);
		}
	}
	void addLibToSandbox(sol::lib lib, const LibSymbolsRules &rules);

	void setPathsForScripts(const fs::path &root, const Paths &allowed);

	[[nodiscard]]
	auto toScriptPath(const std::string &fileName) const -> fs::path;

	[[nodiscard]]
	bool isPathAllowed(const fs::path &scriptFile) const
	{
		return fs_utils::startsWith(scriptFile, allowedScriptPaths);
	}
	auto dofile(sol::stack_object fileName) -> sol::protected_function_result;
	void loadSafeExternalScriptFilesRoutine();
	bool loadSafePrint();
	void print(sol::variadic_args args);

private:
	LuaState &lua;
	sol::environment sandbox;

	Presets preset{Presets::Base};
	fs::path scriptsRoot{}; // Absolute, lexically normalized path.
							// Relative paths to script files are resolved from this location.
							// If empty, loading external scripts is prohibited.
	Paths allowedScriptPaths{};

	std::set<sol::lib> loadedLibs;

	static const SandboxPresets sandboxPresets;
	static const LibsSandboxingRulesMap libsSandboxingRules;
};

namespace lua
{
	[[nodiscard]]
	auto libName(sol::lib lib) noexcept -> std::optional<std::string_view>;

	[[nodiscard]]
	auto libByName(std::string_view libName) noexcept -> std::optional<sol::lib>;

	[[nodiscard]]
	auto toString(sol::object obj) -> std::string;

	[[nodiscard]]
	bool isBytecode(const fs::path &file);

	[[nodiscard]]
	auto makeFnCallResult(sol::state &lua,
						  const auto &object,
						  sol::call_status callStatus = sol::call_status::ok)
		-> sol::protected_function_result;
} // namespace lua
