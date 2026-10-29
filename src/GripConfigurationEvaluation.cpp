#include "GripConfigurationEvaluation.h"

GripConfigurationEvaluation::Result GripConfigurationEvaluation::Evaluate(
	const WeaponClassification::Type a_weaponType) noexcept
{
	using Type = WeaponClassification::Type;

	switch (a_weaponType) {
	case Type::kOneHandDagger:
	case Type::kOneHandSword:
	case Type::kOneHandAxe:
	case Type::kOneHandMace:
		return { true, 1, kOneHandGripFactor, UnavailableReason::kNone };
	case Type::kTwoHandSword:
	case Type::kTwoHandAxe:
		return { true, 2, kTwoHandGripFactor, UnavailableReason::kNone };
	case Type::kUnavailable:
		return { false, 0, 0.0f, UnavailableReason::kWeaponUnavailable };
	case Type::kHandToHand:
		return { false, 0, 0.0f, UnavailableReason::kHandToHand };
	default:
		return { false, 0, 0.0f, UnavailableReason::kUnsupportedWeaponType };
	}
}

std::string_view GripConfigurationEvaluation::ReasonName(
	const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kWeaponUnavailable:
		return "weapon_unavailable";
	case UnavailableReason::kHandToHand:
		return "hand_to_hand";
	case UnavailableReason::kUnsupportedWeaponType:
		return "unsupported_weapon_type";
	default:
		return "none";
	}
}
