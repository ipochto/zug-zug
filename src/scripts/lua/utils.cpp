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
} // namespace lua
/*-----------------------------------------------------------------------------------------------*/
namespace lua::memory
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
} // namespace lua::memory
/*-----------------------------------------------------------------------------------------------*/
namespace lua::timeoutGuard
{
	void defaultHook(lua_State *L, lua_Debug* /*ar*/)
	{
		using Registry = registry::TaggedRegistrySlot<HookContext>;
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
				 lua_Hook func /* = lua::timeoutGuard::defaultHook */) noexcept
	{
		assert(checkPeriod > 0 && "Check period must be a positive integer.");
		lua_sethook(lua.lua_state(), func, LUA_MASKCOUNT, checkPeriod);
	}

	void removeHook(sol::state_view lua) noexcept
	{
		lua_sethook(lua.lua_state(), nullptr, 0, 0);
	}
/*-----------------------------------------------------------------------------------------------*/
	bool Watchdog::attach(sol::state_view newLua, bool force /* = false */) noexcept
	{
		if (force) {
			detach();
		} else if (armed()) {
			spdlog::error("Cannot attach timeout watchdog to a new Lua state while it's armed");
			return false;
		}
		lua = newLua;
		return true;
	}

	void Watchdog::detach() noexcept
	{
		disarm();
		lua = nullptr;
	}

	bool Watchdog::configureHook(InstructionsCount newCheckPeriod, lua_Hook newHook) noexcept
	{
		if (armed()) {
			spdlog::error("Cannot change timeout watchdog hook settings while it's armed");
			return false;
		}
		if (newCheckPeriod <= 0) {
			spdlog::error("Unable to change timeout watchdog hook settings: "
						  "Check period has to be a positive integer");
			return false;
		}
		if (newHook == nullptr) {
			spdlog::error("Unable to change timeout watchdog hook settings: "
						  "Hook function pointer cannot be null");
			return false;
		}
		checkPeriod = newCheckPeriod;
		hook = newHook;
		return true;
	}

	bool Watchdog::arm(time::milliseconds limit) noexcept
	{
		if (armed()) {
			spdlog::error("Unable to arm timeout watchdog: already armed");
			return false;
		}
		if (!attached()) {
			spdlog::error("Unable to arm timeout watchdog: "
						  "Lua state is not properly initialized");
			return false;
		}
		if (!CtxRegistry::empty(lua)) {
			spdlog::error("Unable to arm timeout watchdog: "
						  "Lua state already has a hook context registered");
			return false;
		}
		if (lua_gethook(lua) != nullptr) {
			spdlog::error("Unable to arm timeout watchdog: Lua state already has a hook set");
			return false;
		}
		running = true;
		CtxRegistry::set(lua, &context);
		setHook(lua, checkPeriod, hook);
		context.start(limit);
		return true;
	}

	bool Watchdog::rearm(time::milliseconds limit) noexcept
	{
		if (!armed()) {
			spdlog::error("Unable to rearm timeout watchdog: it is not currently armed");
			return false;
		}
		context.start(limit);
		return true;
	}

	void Watchdog::disarm() noexcept
	{
		context.reset();
		const bool wasArmed = running;
		running = false;

		if (!attached() || !wasArmed) {
			return;
		}
		removeHook(lua);
		CtxRegistry::remove(lua);
	}
} // namespace lua::timeoutGuard
