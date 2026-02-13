#pragma once

#include "lua/sol2.hpp"
#include "utils/filesystem.hpp"

#include <chrono>
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
} // namespace lua
/*-----------------------------------------------------------------------------------------------*/
namespace lua::memory
{
	using Allocator = lua_Alloc;

	constexpr size_t c1MB = 1L * 1024 * 1024;
	constexpr size_t cDefaultMemLimit = c1MB;

	struct LimitedAllocatorState
	{
		size_t used {};
		size_t limit {cDefaultMemLimit};

		bool limitReached {false};
		bool overflow {false};

		[[nodiscard]]
		bool isLimitEnabled() const { return limit > 0; }

		void resetErrorFlags() noexcept { limitReached = overflow = false; }

		void disableLimit() { limit = 0; }
	};

	void *limitedAlloc(void *ud, void *ptr, size_t currSize, size_t newSize) noexcept;
} // namespace lua::memory
/*-----------------------------------------------------------------------------------------------*/
namespace lua::registry
{
	using Key = sol::lightuserdata_value;

	template <typename Tag>
	struct KeyTag
	{
		inline static char kTag{};
		static auto key() noexcept -> Key { return Key{&kTag}; }
	};

	template <typename Tag, typename DataT = Tag>
	struct RegistrySlot
	{
		using SlotKey = KeyTag<Tag>;
		using Stored = sol::lightuserdata_value;

		static void set(sol::state_view lua, DataT *data)
		{
			lua.registry()[SlotKey::key()] = Stored{static_cast<void *>(data)};
		}

		static DataT *get(sol::state_view lua)
		{
			auto data = sol::object{lua.registry()[SlotKey::key()]};

			if (!data.valid() || !data.is<Stored>()) {
				return nullptr;
			}
			return static_cast<DataT*>(data.as<Stored>().value);
		}

		[[nodiscard]]
		static bool empty(sol::state_view lua) { return get(lua) == nullptr; }

		static void remove(sol::state_view lua)
		{
			lua.registry()[SlotKey::key()] = sol::nil;
		}
	};

} // namespace lua::registry
/*-----------------------------------------------------------------------------------------------*/
namespace lua::timeoutGuard
{
	using namespace std::chrono_literals;
	namespace time = std::chrono;
	using InstructionsCount = int;

	constexpr auto kDefaultCheckPeriod {10'000};
	constexpr auto kDefaultLimit {5ms};
/*-----------------------------------------------------------------------------------------------*/
	void defaultHook(lua_State *L, lua_Debug* /*ar*/);

	void setHook(sol::state_view lua,
				 InstructionsCount checkPeriod,
				 lua_Hook func = lua::timeoutGuard::defaultHook) noexcept;

	void removeHook(sol::state_view lua) noexcept;
/*-----------------------------------------------------------------------------------------------*/
	struct HookContext
	{
		using clock = time::steady_clock;

		clock::time_point deadline{};
		bool enabled{false};

		void start(time::milliseconds limit) noexcept
		{
			enabled = true;
			deadline = clock::now() + limit;
		}
		void reset() noexcept { *this = HookContext{}; }

		[[nodiscard]]
		bool isTimedOut() const noexcept { return enabled && clock::now() > deadline; }
	};
/*-----------------------------------------------------------------------------------------------*/
	class Watchdog
	{
	private:
		lua_State *lua{nullptr};
		InstructionsCount checkPeriod{0};
		lua_Hook hook{nullptr};
		HookContext context{};

		bool running{false};

	public:
		using CtxRegistry = registry::RegistrySlot<HookContext>;

		Watchdog(sol::state_view lua,
				 InstructionsCount checkPeriod = kDefaultCheckPeriod,
				 lua_Hook hookFn = defaultHook)
			: lua(lua),
			  checkPeriod(checkPeriod > 0 ? checkPeriod : kDefaultCheckPeriod),
			  hook(hookFn)
		{}

		Watchdog(const Watchdog &) = delete;
		Watchdog &operator=(const Watchdog &) = delete;
		Watchdog(Watchdog &&) = delete;
		Watchdog &operator=(Watchdog &&) = delete;
		~Watchdog() { detach(); }

		bool attach(sol::state_view newLua, bool force = false) noexcept;
		void detach() noexcept;

		bool configureHook(InstructionsCount newCheckPeriod, lua_Hook newHook) noexcept;

		[[nodiscard]]
		bool armed() const noexcept { return running; }
		[[nodiscard]]
		bool timeOut() const noexcept { return context.isTimedOut(); }

		bool arm(time::milliseconds limit) noexcept;
		bool rearm(time::milliseconds limit) noexcept;
		void disarm() noexcept;

	private:
		[[nodiscard]]
		bool attached() const noexcept { return lua != nullptr; }
	};
/*-----------------------------------------------------------------------------------------------*/
	class GuardedScope
	{
	private:
		Watchdog *watchdog{nullptr};

	public:
		GuardedScope(Watchdog &watchdog, time::milliseconds limit = kDefaultLimit)
			: watchdog(&watchdog)
		{
			if (!watchdog.arm(limit)) {
				disable();
			}
		}

		GuardedScope(const GuardedScope &) = delete;
		GuardedScope &operator=(const GuardedScope &) = delete;

		GuardedScope(GuardedScope &&other) noexcept : watchdog(other.watchdog) { other.disable(); }
		GuardedScope &operator=(GuardedScope &&other) = delete;

		~GuardedScope()
		{
			if (disabled()) {
				return;
			}
			watchdog->disarm();
		}

		bool rearm(time::milliseconds limit = kDefaultLimit)
		{
			if (disabled()) {
				return false;
			}
			watchdog->disarm();
			return watchdog->arm(limit);
		}

		[[nodiscard]]
		bool timedOut() const noexcept { return !disabled() && watchdog->timeOut(); }

	private:
		void disable() noexcept { watchdog = nullptr; }

		[[nodiscard]]
		bool disabled() const noexcept { return watchdog == nullptr; }
	};
} // namespace lua::timeoutGuard
