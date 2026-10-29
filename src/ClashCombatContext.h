#pragma once

#include "LogicalClashNormalizer.h"
#include "WeaponClassification.h"

namespace ClashCombatContext
{
	struct Combatant
	{
		bool available{ false };
		bool isPlayer{ false };
		std::string_view role{ "unknown" };
		std::uint32_t actorHandle{ 0 };
		RE::FormID actorFormID{ 0 };
		std::string actorName{ "<unavailable>" };
		LogicalClashNormalizer::Hand attackHand{ LogicalClashNormalizer::Hand::kUnknown };
		std::string_view attackClassification{ "Unavailable" };
		RE::FormID weaponFormID{ 0 };
		std::string weaponName{ "<unavailable>" };
		WeaponClassification::Type weaponType{ WeaponClassification::Type::kUnavailable };
		bool supportedMeleeWeapon{ false };
		std::optional<float> weaponWeight;
		std::optional<float> staminaPercent;
		std::string_view skillName{ "Unavailable" };
		std::optional<float> skillValue;
		std::string_view weaponSkillName{ "Unavailable" };
		std::optional<float> weaponSkillValue;
	};

	struct Snapshot
	{
		std::chrono::steady_clock::time_point capturedAt;
		bool playerInvolved{ false };
		int staminaInfluence{ 50 };
		int skillInfluence{ 50 };
		Combatant attacker;
		Combatant target;
	};

	// Game-thread only. Resolves actor handles and copies all state needed after
	// the task returns; no Skyrim object pointer is stored in the result.
	[[nodiscard]] Snapshot Capture(
		const LogicalClashNormalizer::Participant& a_attacker,
		const LogicalClashNormalizer::Participant& a_target);
}
