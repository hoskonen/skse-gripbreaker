#pragma once

#include "WeaponClassification.h"

namespace CombatContextUtils
{
	[[nodiscard]] WeaponClassification::Type ClassifyWeapon(const RE::TESObjectWEAP* a_weapon);
	[[nodiscard]] std::string_view WeaponTypeName(const RE::TESObjectWEAP* a_weapon);
	[[nodiscard]] std::string_view ActorValueName(RE::ActorValue a_actorValue);
	[[nodiscard]] RE::ActorValue RelevantWeaponSkill(const RE::TESObjectWEAP* a_weapon);
}
