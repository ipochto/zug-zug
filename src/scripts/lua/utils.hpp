#pragma once

#include "lua/sol2.hpp"
#include "utils/filesystem.hpp"

#include <optional>
#include <ranges>
#include <string_view>

namespace ranges = std::ranges;

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
			{sol::lib::bit32,     "bit32"}, // Lua 5.2 only
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

	[[nodiscard]]
	constexpr auto libName(sol::lib lib) noexcept
		-> std::optional<std::string_view>
	{
		auto findLib = [lib](auto &lookup) -> bool { return lookup.lib == lib; };

		const auto &libs = lua::details::libsNames;
		if (const auto it = ranges::find_if(libs, findLib); it != libs.end()) {
			return it->name;
		}
		return std::nullopt;
	}

	[[nodiscard]]
	constexpr auto libByName(std::string_view libName) noexcept
		-> std::optional<sol::lib>
	{
		auto findLibName = [libName](auto &lookup) -> bool { return lookup.name == libName; };

		const auto &libs = lua::details::libsNames;
		if (const auto it = ranges::find_if(libs, findLibName); it != libs.end()) {
			return it->lib;
		}
		return std::nullopt;
	}

	[[nodiscard]]
	constexpr auto libLookupName(sol::lib lib) noexcept
		-> std::string_view
	{
		return (lib == sol::lib::base) ? "_G" : lua::libName(lib).value_or("");
	}

	[[nodiscard]]
	inline auto makeFnCallResult(sol::state &lua,
								 const auto &object,
								 sol::call_status callStatus = sol::call_status::ok)
		-> sol::protected_function_result
	{
		bool isResultValid = callStatus == sol::call_status::ok;
		sol::stack::push(lua, object);
		return sol::protected_function_result {lua, -1, isResultValid ? 1 : 0, 1, callStatus};
	}

	[[nodiscard]]
	auto toString(const sol::object &obj) -> std::string;

	[[nodiscard]]
	bool isBytecode(const fs::path &file);

	namespace memory
	{
		constexpr size_t c1MB = 1L * 1024 * 1024;
		constexpr size_t cDefaultMemLimit = c1MB;

		struct LimitedAllocatorState
		{
			size_t used {};
			size_t limit {cDefaultMemLimit};

			bool limitReached {false};
			bool overflow {false};

			[[nodiscard]]
			bool isActivated() const { return used > 0; }
			[[nodiscard]]
			bool isLimitEnabled() const { return limit > 0; }

			void disableLimit() { limit = 0; }
		};

		void *limitedAlloc(void *ud, void *ptr, size_t currSize, size_t newSize) noexcept;
	} // namespace memory

	namespace timeoutGuard
	{
		namespace time = std::chrono;

		constexpr int kDefaultInstructionsCount = 10'000;

		struct WatchdogState
		{
			// Value is unused; its address is used as a unique key in the Lua registry
			inline static uint8_t kRegistryKey {};
			static auto registryKey() noexcept -> sol::lightuserdata_value
			{
				return sol::lightuserdata_value{ static_cast<void*>(&WatchdogState::kRegistryKey) };
			}
			
			using clock = time::steady_clock;

			clock::time_point deadline {};
			time::milliseconds limit {};

			bool enabled {false};

			WatchdogState() = default;
			explicit WatchdogState(time::milliseconds limit) noexcept 
				: limit(limit)
			{}

			void enable() noexcept { enabled = true; }
			void disable() noexcept { enabled = false; }
			void start() noexcept
			{ 
				enable();
				deadline = clock::now() + limit;
			}

			[[nodiscard]]
			bool isTimedOut() const noexcept { return enabled && clock::now() > deadline; }
		};
		void placeWatchdogState(lua_State *L, WatchdogState *ctx);
		auto getWatchdogState(lua_State *L) -> WatchdogState*;

		void watchdog(lua_State *L, lua_Debug* /*ar*/);

	} // namespace timeoutGuard
} // namespace lua