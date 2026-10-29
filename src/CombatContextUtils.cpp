#include "CombatContextUtils.h"

WeaponClassification::Type CombatContextUtils::ClassifyWeapon(const RE::TESObjectWEAP* a_weapon)
{
	using Type = WeaponClassification::Type;

	if (!a_weapon) {
		return Type::kUnavailable;
	}

	switch (a_weapon->GetWeaponType()) {
	case RE::WEAPON_TYPE::kHandToHandMelee:
		return Type::kHandToHand;
	case RE::WEAPON_TYPE::kOneHandSword:
		return Type::kOneHandSword;
	case RE::WEAPON_TYPE::kOneHandDagger:
		return Type::kOneHandDagger;
	case RE::WEAPON_TYPE::kOneHandAxe:
		return Type::kOneHandAxe;
	case RE::WEAPON_TYPE::kOneHandMace:
		return Type::kOneHandMace;
	case RE::WEAPON_TYPE::kTwoHandSword:
		return Type::kTwoHandSword;
	case RE::WEAPON_TYPE::kTwoHandAxe:
		return Type::kTwoHandAxe;
	case RE::WEAPON_TYPE::kBow:
		return Type::kBow;
	case RE::WEAPON_TYPE::kStaff:
		return Type::kStaff;
	case RE::WEAPON_TYPE::kCrossbow:
		return Type::kCrossbow;
	default:
		return Type::kUnknown;
	}
}

std::string_view CombatContextUtils::WeaponTypeName(const RE::TESObjectWEAP* a_weapon)
{
	return WeaponClassification::Name(ClassifyWeapon(a_weapon));
}

std::string_view CombatContextUtils::ActorValueName(const RE::ActorValue a_actorValue)
{
	switch (a_actorValue) {
	case RE::ActorValue::kOneHanded:
		return "OneHanded";
	case RE::ActorValue::kTwoHanded:
		return "TwoHanded";
	case RE::ActorValue::kBlock:
		return "Block";
	default:
		return "Unavailable";
	}
}

RE::ActorValue CombatContextUtils::RelevantWeaponSkill(const RE::TESObjectWEAP* a_weapon)
{
	if (!a_weapon) {
		return RE::ActorValue::kNone;
	}

	const auto skill = a_weapon->weaponData.skill.get();
	switch (skill) {
	case RE::ActorValue::kOneHanded:
	case RE::ActorValue::kTwoHanded:
	case RE::ActorValue::kBlock:
		return skill;
	default:
		return RE::ActorValue::kNone;
	}
}
