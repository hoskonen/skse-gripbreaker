#include "WeaponClassification.h"

std::string_view WeaponClassification::Name(const Type a_type) noexcept
{
	switch (a_type) {
	case Type::kHandToHand:
		return "HandToHand";
	case Type::kOneHandSword:
		return "OneHandSword";
	case Type::kOneHandDagger:
		return "OneHandDagger";
	case Type::kOneHandAxe:
		return "OneHandAxe";
	case Type::kOneHandMace:
		return "OneHandMace";
	case Type::kTwoHandSword:
		return "TwoHandSword";
	case Type::kTwoHandAxe:
		return "TwoHandAxe";
	case Type::kBow:
		return "Bow";
	case Type::kStaff:
		return "Staff";
	case Type::kCrossbow:
		return "Crossbow";
	case Type::kUnknown:
		return "Unknown";
	default:
		return "Unavailable";
	}
}

bool WeaponClassification::IsSupportedMelee(const Type a_type) noexcept
{
	switch (a_type) {
	case Type::kOneHandSword:
	case Type::kOneHandDagger:
	case Type::kOneHandAxe:
	case Type::kOneHandMace:
	case Type::kTwoHandSword:
	case Type::kTwoHandAxe:
		return true;
	default:
		return false;
	}
}
