#include "scripts/lua/runtime.hpp"

#include <doctest/doctest.h>

namespace mem = lua::memory;

TEST_CASE("limitedAlloc: malloc updates used")
{
	constexpr size_t objSize = 64;

	auto allocState = mem::LimitedAllocatorState({.limit = mem::c1MB});

	void *ptr = mem::limitedAlloc(&allocState, nullptr, objSize * 4 /* ignored */, objSize);
	REQUIRE(ptr != nullptr);
	CHECK(allocState.used == objSize);

	// cleanup
	ptr = mem::limitedAlloc(&allocState, ptr, objSize, 0);
	CHECK(ptr == nullptr);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: realloc grow increases used")
{
	constexpr size_t objSize = 64;
	constexpr size_t objSizeAfter = objSize * 2;

	auto allocState = mem::LimitedAllocatorState({.limit = mem::c1MB});

	void *ptr = mem::limitedAlloc(&allocState, nullptr, 0, objSize);
	REQUIRE(ptr != nullptr);
	REQUIRE(allocState.used == objSize);

	void *ptr2 = mem::limitedAlloc(&allocState, ptr, objSize, objSizeAfter);
	REQUIRE(ptr2 != nullptr);
	CHECK(allocState.used == objSizeAfter);

	// cleanup
	mem::limitedAlloc(&allocState, ptr2, objSizeAfter, 0);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: realloc shrink decreases used")
{
	constexpr size_t objSize = 256;
	constexpr size_t objSizeAfter = 64;

	auto allocState = mem::LimitedAllocatorState({.limit = mem::c1MB});

	void *ptr = mem::limitedAlloc(&allocState, nullptr, 0, objSize);
	REQUIRE(ptr != nullptr);
	REQUIRE(allocState.used == objSize);

	void *ptr2 = mem::limitedAlloc(&allocState, ptr, objSize, objSizeAfter);
	REQUIRE(ptr2 != nullptr);
	CHECK(allocState.used == objSizeAfter);

	// cleanup
	mem::limitedAlloc(&allocState, ptr2, objSizeAfter, 0);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: free clamps underflow when currSize > used")
{
	constexpr size_t objSize = 16;
	constexpr size_t objSizeAfter = objSize * 2;
	constexpr size_t initUsed = objSize / 2;

	auto allocState = mem::LimitedAllocatorState({.used = initUsed, .limit = mem::c1MB});

	void *ptr = std::malloc(objSize);
	REQUIRE(ptr != nullptr);

	// currSize > used -> should clamp to 0 without wrap-around
	void *ptr2 = mem::limitedAlloc(&allocState, ptr, objSize * 4, 0);
	CHECK(ptr2 == nullptr);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: usedBase clamps when currSize > used (realloc path)")
{
	constexpr size_t objSize = 16;
	constexpr size_t objSizeAfter = objSize * 2;
	constexpr size_t initUsed = objSize / 2;

	auto allocState = mem::LimitedAllocatorState({.used = initUsed, .limit = mem::c1MB});

	// ptr != nullptr so realloc path (newSize != 0)
	void *ptr = std::malloc(objSize);
	REQUIRE(ptr != nullptr);

	// currSize > used -> usedBase becomes 0, so newUsed = newSize
	void *ptr2 = mem::limitedAlloc(&allocState, ptr, objSize * 4, objSizeAfter);
	REQUIRE(ptr2 != nullptr);
	CHECK(allocState.used == objSizeAfter);

	// cleanup: currSize matches allocState.used
	mem::limitedAlloc(&allocState, ptr2, objSizeAfter, 0);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: ptr == nullptr forces currSize = 0")
{
	constexpr size_t objSize = 16;
	constexpr size_t initUsed = 500;

	auto allocState = mem::LimitedAllocatorState({.used = initUsed, .limit = mem::c1MB});

	// ptr == nullptr => currSize ignored (forced to 0)
	void *ptr = mem::limitedAlloc(&allocState, nullptr, initUsed / 2 , objSize);
	REQUIRE(ptr != nullptr);

	CHECK(allocState.used == initUsed + objSize);

	mem::limitedAlloc(&allocState, ptr, objSize, 0);

	CHECK(allocState.used == initUsed);
}

TEST_CASE("limitedAlloc: limitReached is set and returns nullptr on limit exceed")
{
	constexpr size_t limit = 64;

	auto allocState = mem::LimitedAllocatorState({.limit = limit});

	void *ptr = mem::limitedAlloc(&allocState, nullptr, 0, limit);
	REQUIRE(ptr != nullptr);
	REQUIRE(allocState.used == limit);

	void *ptr2 = mem::limitedAlloc(&allocState, ptr, limit, limit + 1);
	CHECK(ptr2 == nullptr);
	CHECK(allocState.used == limit); // unchanged (realloc not performed)
	CHECK(allocState.limitReached);
	CHECK_FALSE(allocState.overflow);

	// cleanup: free
	mem::limitedAlloc(&allocState, ptr, limit, 0);
	CHECK(allocState.used == 0);
}

TEST_CASE("limitedAlloc: overflow is set when usedBase + newSize overflows size_t")
{
	constexpr size_t objSize = 16;

	auto allocState = mem::LimitedAllocatorState({.used = std::numeric_limits<std::size_t>::max() - 1,
												  .limit = std::numeric_limits<std::size_t>::max()});

	// ptr == nullptr -> currSize becomes 0, usedBase = used
	// newSize = 16 => usedBase + newSize overflows
	void *ptr = limitedAlloc(&allocState, nullptr, 0, objSize);
	CHECK(ptr == nullptr);
	CHECK(allocState.overflow);
}

TEST_CASE("limitedAlloc: resetErrorFlags clears limitReached and overflow")
{
	constexpr size_t limit = 64;
	constexpr size_t objSize = 16;

	auto allocState = mem::LimitedAllocatorState({.limit = limit});

	void *ptr = mem::limitedAlloc(&allocState, nullptr, 0, limit);
	REQUIRE(ptr != nullptr);

	void *ptr2 = mem::limitedAlloc(&allocState, ptr, limit, limit + 1);
	CHECK(ptr2 == nullptr);
	REQUIRE(allocState.limitReached);
	CHECK_FALSE(allocState.overflow);

	allocState.resetErrorFlags();
	CHECK_FALSE(allocState.limitReached);
	CHECK_FALSE(allocState.overflow);

	// cleanup
	mem::limitedAlloc(&allocState, ptr, limit, 0);
	CHECK(allocState.used == 0);

	allocState.used = std::numeric_limits<std::size_t>::max() - 1;
	allocState.limit = std::numeric_limits<std::size_t>::max();

	void *ptr3 = mem::limitedAlloc(&allocState, nullptr, 0, objSize);
	CHECK(ptr3 == nullptr);
	REQUIRE(allocState.overflow);
	CHECK_FALSE(allocState.limitReached);

	allocState.resetErrorFlags();
	CHECK_FALSE(allocState.limitReached);
	CHECK_FALSE(allocState.overflow);
}

TEST_CASE("limitedAlloc: + LuaRuntime: used memory reduced to initial value after runtime reset")
{
	LuaRuntime lua(lua::memory::cDefaultMemLimit);

	auto &allocState = lua.getAllocatorState();
	size_t initialUsed = allocState.used;

	lua.state.script(R"(
		placeHolder = {}
		for i = 1, 32765 do
			placeHolder[i] = "A string #" .. " to use some memory"
		end
	)");
	CHECK(allocState.used > initialUsed);

	lua.reset();
	CHECK(allocState.used == initialUsed);
}

TEST_CASE("limitedAlloc: + LuaSandox on LuaRuntime: script returns error if memory limit exceeded")
{
	LuaRuntime lua(lua::memory::cDefaultMemLimit);

	LuaSandbox sandbox(lua, LuaSandbox::Presets::Minimal);

	auto result = sandbox.run(R"(
		placeHolder = {}
		while true do
			table.insert(placeHolder, 0xFFFF)
		end
	)");
	CHECK_FALSE(result.valid());
	CHECK(lua.getAllocatorState().limitReached == true);
}
