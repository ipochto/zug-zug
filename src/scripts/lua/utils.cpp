#include "lua/utils.hpp"

#include <array>
#include <fstream>
#include <ranges>
#include <spdlog/spdlog.h>

namespace lua
{
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
/*-----------------------------------------------------------------------------------------------*/
	namespace memory
	{
		void *limitedAlloc(void *ud, void *ptr, size_t currSize, size_t newSize) noexcept
		{
			auto *allocState = static_cast<LimitedAllocatorState*>(ud);

			if (allocState == nullptr) {
				assert((allocState != nullptr) && "Pointer to the allocator state must be provided.");
				return nullptr;
			}

			if (ptr == nullptr) {
				currSize = 0;
			}
			if (newSize == 0) {
				if (ptr != nullptr) {
					allocState->used -= (allocState->used >= currSize) ? currSize
																	   : allocState->used;
				}
				std::free(ptr);
				return nullptr;
			}
			const size_t usedBase = (allocState->used >= currSize) ? allocState->used - currSize
																   : 0;

			if (newSize > (std::numeric_limits<size_t>::max() - usedBase)) {
				spdlog::error("Lua allocator: arithmetic overflow while computing memory usage "
							  "[used: {}, requested more for: {}, size_t max: {}]",
							  usedBase,
							  newSize,
							  std::numeric_limits<size_t>::max());
				allocState->overflow = true;
				return nullptr;
			}
			const size_t newUsed = usedBase + newSize;
			if (allocState->isLimitEnabled() && newUsed > allocState->limit) {
				spdlog::error("Lua allocator: memory limit reached "
							  "[limit: {}, used: {}, requested total: {}]",
							  allocState->limit,
							  allocState->used,
							  newUsed);
				allocState->limitReached = true;
				return nullptr;
			}
			void *newPtr = std::realloc(ptr, newSize);
			if (newPtr != nullptr) {
				allocState->used = newUsed;
			}
			return newPtr;
		}
	} // namespace memory
/*-----------------------------------------------------------------------------------------------*/
	namespace timeoutGuard
	{
		void defaultHook(lua_State *L, lua_Debug* /*ar*/)
		{
			using Registry = registry::RegistrySlot<HookContext>;
			auto *ctx = Registry::get(L);
			if (ctx == nullptr) {
				luaL_error(L, "Timeout guard: Unable to get hook context.");
			}
			if (ctx->isTimedOut()) {
				luaL_error(L, "Timeout guard: Script timed out.");
			}
		}

		void setHook(sol::state_view lua,
					 InstructionsCount checkPeriod,
					 lua_Hook func /* = lua::timeoutGuard::defaultHook */)  noexcept
		{
			lua_sethook(lua.lua_state(), func, LUA_MASKCOUNT, checkPeriod);
		}

		void removeHook(sol::state_view lua) noexcept
		{
			lua_sethook(lua.lua_state(), nullptr, 0, 0);
		}
/*-----------------------------------------------------------------------------------------------*/
		void Watchdog::start(time::milliseconds limit) noexcept
		{
			if (!hookStatus.installed()) {
				arm();
			}
			context.start(limit);
		}

		void Watchdog::attachLuaState(sol::state_view newLua)
		{
			detachLuaState();
			lua = newLua.lua_state();
			context.registerIn(lua);
			hookStatus.registerIn(lua);
			if (hookStatus.installed()) {
				arm(hookStatus.checkPeriod, hookStatus.func);
			}
		}

		void Watchdog::detachLuaState()
		{
			if (lua == nullptr) {
				return;
			}
			removeHook(lua);
			context.unregister(lua);
			hookStatus.unregister(lua);
			lua = nullptr;
			context.stop();
		}

		void Watchdog::arm(InstructionsCount checkPeriod /* = kDefaultCheckPeriod */,
						   lua_Hook hook /* = defaultHook */) noexcept
		{
			setHook(lua, checkPeriod, hook);
			hookStatus = HookStatus{checkPeriod, hook};
		}
/*-----------------------------------------------------------------------------------------------*/
		void GuardedScope::onDestroy() noexcept
		{
			if (armed()) {
				watchdog->stop();
				if (prevPeriod != 0) {
					watchdog->arm(prevPeriod);
				}
			}
		}

		GuardedScope &GuardedScope::operator=(GuardedScope &&other) noexcept
		{
			if (this != &other) {
				onDestroy();
				watchdog = other.watchdog;
				prevPeriod = other.prevPeriod;
				other.disarm();
			}
			return *this;
		}
	} // namespace timeoutGuard
} // namespace lua
