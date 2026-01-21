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

	namespace timeoutGuard
	{
		void placeWatchdogState(lua_State *L, WatchdogState *ctx)
		{
			auto lua = sol::state_view{L};
			auto registry = lua.registry();
			registry[WatchdogState::registryKey()] 
				= sol::lightuserdata_value{ static_cast<void*>(ctx) };
		}

		auto getWatchdogState(lua_State *L) -> WatchdogState*
		{
			auto lua = sol::state_view{L};
			auto registry = lua.registry();

			auto state = registry.get_or(WatchdogState::registryKey(),
										 sol::lightuserdata_value{nullptr});

    		return static_cast<WatchdogState*>(state.value);
		}

		void watchdog(lua_State *L, lua_Debug* /*ar*/)
		{
			auto *state = getWatchdogState(L);
			if (state && state->isTimedOut()) {
				luaL_error(L, "Script timed out");
			}
		}
	} // namespace timeoutGuard
} // namespace lua