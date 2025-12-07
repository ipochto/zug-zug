#pragma once

#include <functional>
#include <memory>
#include <optional>

template <typename T>
class [[nodiscard]] optional_ref
{
public:
	optional_ref() = default;
	optional_ref(std::nullopt_t) : ref(std::nullopt) {}
	optional_ref(T &value) : ref(value) {}

	explicit operator bool() const noexcept { return has_value(); }

	T &operator*() noexcept { return ref->get(); }
	const T &operator*() const noexcept { return ref->get(); }

	T *operator->() noexcept { return std::addressof(**this); }
	const T *operator->() const noexcept { return std::addressof(**this); }

	bool operator==(std::nullopt_t) const noexcept { return !has_value(); }
	bool operator!=(std::nullopt_t) const noexcept { return has_value(); }

	[[nodiscard]]
	bool has_value() const noexcept { return ref.has_value(); }

	void reset() noexcept { ref.reset(); }

private:
	std::optional<std::reference_wrapper<T>> ref;
};

template <typename T>
using opt_ref = optional_ref<T>;

template <typename T>
using opt_cref = optional_ref<const T>;
