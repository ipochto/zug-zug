#pragma once

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>

template <typename T>
concept CountedEnum =
	std::is_enum_v<T>
	&& (requires { std::remove_cv_t<T>::Count; } || requires { std::remove_cv_t<T>::count; });

template <CountedEnum Enum, typename Enum_ut = std::underlying_type_t<Enum>>
consteval Enum_ut enumSize()
{
	using RawEnum = std::remove_cv_t<Enum>;

	if constexpr (requires { RawEnum::Count; }) {
		return static_cast<Enum_ut>(RawEnum::Count);
	} else {
		return static_cast<Enum_ut>(RawEnum::count);
	}
}

template <CountedEnum Enum, typename Enum_ut = std::underlying_type_t<Enum>>
class enum_set
{
public:
	using mask_t = uint64_t;
	static constexpr Enum_ut cMaxCapacity = 64;

	constexpr enum_set() noexcept = default;

	template <typename... Enums>
	requires(std::same_as<Enum, Enums> && ...)
	constexpr enum_set(Enums... vals) noexcept
	{
		(insert(vals), ...);
	}
	constexpr enum_set(std::initializer_list<Enum> init) noexcept
	{
		for (Enum e : init) {
			insert(e);
		}
	}
	constexpr void insert(Enum e) noexcept { bits |= bit(e); }
	constexpr void erase(Enum e) noexcept { bits &= ~bit(e); }
	constexpr void clear() noexcept { bits = 0; }

	[[nodiscard]]
	constexpr bool contains(Enum e) const noexcept
	{
		return bits & bit(e);
	}

	[[nodiscard]]
	constexpr bool empty() const noexcept
	{
		return bits == 0;
	}

	[[nodiscard]]
	constexpr size_t size() const noexcept
	{
		return std::popcount(bits);
	}

	struct iterator
	{
		// For STL-compatibility
		using value_type = Enum;
		using difference_type = std::ptrdiff_t;
		using iterator_category = std::input_iterator_tag;

		mask_t rest = 0;
		Enum_ut idx = N;

		constexpr iterator() noexcept = default;

		constexpr iterator(mask_t mask, bool end) noexcept
		{
			if (end || mask == 0) {
				rest = 0;
				idx = N;
			} else {
				rest = mask;
				idx = std::countr_zero(rest);
				rest &= rest - 1;
			}
		}
		constexpr value_type operator*() const noexcept { return to_enum(idx); }
		constexpr iterator &operator++() noexcept // pre-increment
		{
			if (rest == 0) {
				idx = N;
			} else {
				idx = std::countr_zero(rest);
				rest &= rest - 1;
			}
			return *this;
		}
		constexpr iterator operator++(int) noexcept // post-increment
		{
			iterator ret = *this;
			++(*this);
			return ret;
		}
		constexpr bool operator==(const iterator &other) const noexcept { return idx == other.idx; }
		constexpr bool operator!=(const iterator &other) const noexcept { return idx != other.idx; }
	};

	constexpr iterator begin() const noexcept { return iterator(bits, /*end=*/false); }
	constexpr iterator end() const noexcept { return iterator(bits, /*end=*/true); }

private:
	[[nodiscard]]
	static constexpr Enum_ut to_index(Enum e) noexcept
	{
		return static_cast<Enum_ut>(e);
	}

	[[nodiscard]]
	static constexpr Enum to_enum(Enum_ut idx) noexcept
	{
		return static_cast<Enum>(idx);
	}

	[[nodiscard]]
	static constexpr mask_t bit(Enum e) noexcept
	{
		return mask_t(1) << to_index(e);
	}

private:
	static constexpr Enum_ut N = enumSize<Enum>();
	static_assert(N <= cMaxCapacity, "enum_set supports up to 64 values");

	mask_t bits = 0;
};