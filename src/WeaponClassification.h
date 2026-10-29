#pragma once

#include <string_view>

namespace WeaponClassification
{
	enum class Type
	{
		kUnavailable,
		kHandToHand,
		kOneHandSword,
		kOneHandDagger,
		kOneHandAxe,
		kOneHandMace,
		kTwoHandSword,
		kTwoHandAxe,
		kBow,
		kStaff,
		kCrossbow,
		kUnknown
	};

	[[nodiscard]] std::string_view Name(Type a_type) noexcept;
	[[nodiscard]] bool IsSupportedMelee(Type a_type) noexcept;
}
