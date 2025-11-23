#pragma once

#include "lua/sol2.hpp"
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

class LuaRuntime
{
public:
	sol::state state;

	LuaRuntime() = default;
	~LuaRuntime() = default;

	LuaRuntime(const LuaRuntime &) = delete;
	LuaRuntime(LuaRuntime &&) = delete;
	LuaRuntime &operator=(const LuaRuntime &) = delete;
	LuaRuntime &operator=(LuaRuntime &&) = delete;

	void require(sol::lib lib)
	{
		if (!loadedLibs.contains(lib)) {
			state.open_libraries(lib);
			loadedLibs.insert(lib);		
		}
	}

private:
	enum_set<sol::lib> loadedLibs;
};

class LuaSandbox
{
public:
	enum class Presets { Core, Minimal, Complete, Custom };
	using Paths = std::vector<fs::path>;

	explicit LuaSandbox(LuaRuntime &runtime,
						Presets preset,
						const fs::path &root = {},
						const Paths &allowedPaths = {})
		: runtime(&runtime),
		  preset(preset)
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
	void allowScriptPath(const fs::path &path);

private:
	using LibNames = std::vector<std::string_view>;
	using Libs = std::vector<sol::lib>;
	using SandboxPresets = std::map<Presets, Libs>;

	struct LibSymbolsRules
	{
		bool allowedAllExceptRestricted{false};
		LibNames allowed{}; // This will be ignored if allowedAllExceptRestricted is set
		LibNames restricted{};
	};

	using LibsSandboxingRulesMap = std::map<sol::lib, LibSymbolsRules>;

	auto checkRulesFor(sol::lib lib) const noexcept -> opt_cref<LibSymbolsRules>;

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
	bool isPathAllowed(const fs::path &scriptFile) const
	{
		return fs_utils::startsWith(scriptFile, allowedScriptPaths);
	}

	auto dofileReplace(sol::stack_object fileName) -> sol::protected_function_result;
	auto requireReplace(sol::stack_object target) -> sol::protected_function_result;
	void printReplace(sol::variadic_args args);

	void loadSafeExternalScriptFilesRoutine();
	void loadSafePrint();

private:
	LuaRuntime *runtime = {nullptr};
	sol::environment sandbox;

	Presets preset{Presets::Core};
	fs::path scriptsRoot{}; // Absolute, lexically normalized path.
							// Relative paths to script files are resolved from this location.
							// If empty, loading external scripts is prohibited.
	Paths allowedScriptPaths{};

	enum_set<sol::lib> loadedLibs;

	static const SandboxPresets sandboxPresets;
	static const LibsSandboxingRulesMap libsSandboxingRules;
};

namespace lua
{
	[[nodiscard]]
	constexpr auto libName(sol::lib lib) noexcept -> std::optional<std::string_view>;

	[[nodiscard]]
	constexpr auto libByName(std::string_view libName) noexcept -> std::optional<sol::lib>;

	[[nodiscard]]
	constexpr auto libLookupName(sol::lib lib) -> std::string_view;

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
