#include "ClashCombatContext.h"

#include "ActorValueUtils.h"
#include "CombatContextUtils.h"
#include "Settings.h"

namespace
{
	using Hand = LogicalClashNormalizer::Hand;
	using Participant = LogicalClashNormalizer::Participant;

	constexpr RE::FormID kPlayerFormID = 0x14;

	struct ActiveWeapon
	{
		RE::TESObjectWEAP* weapon{ nullptr };
		Hand hand{ Hand::kUnknown };
		RE::NiPointer<RE::BGSAttackData> attackData;
	};

	ActiveWeapon ResolveActiveWeapon(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}

		const auto* process = a_actor->GetActorRuntimeData().currentProcess;
		const auto* high = process ? process->high : nullptr;
		RE::NiPointer<RE::BGSAttackData> attackData = high ? high->attackData : nullptr;
		if (!attackData) {
			return {};
		}

		const auto leftHand = attackData->IsLeftAttack();
		auto* equippedForm = a_actor->GetEquippedObject(leftHand);
		return {
			equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr,
			leftHand ? Hand::kLeft : Hand::kRight,
			std::move(attackData)
		};
	}

	std::string_view AttackClassification(
		const RE::NiPointer<RE::BGSAttackData>& a_attackData,
		const bool a_blocking)
	{
		if (!a_attackData) {
			return a_blocking ? "Blocking" : "Unavailable";
		}

		const auto& flags = a_attackData->data.flags;
		const auto powerAttack = flags.any(RE::AttackData::AttackFlag::kPowerAttack);
		const auto bash = flags.any(RE::AttackData::AttackFlag::kBashAttack);
		if (bash && powerAttack) {
			return "PowerBash";
		}
		if (bash) {
			return "Bash";
		}
		if (powerAttack) {
			return "PowerAttack";
		}
		if (a_blocking) {
			return "Blocking";
		}
		return "Attack";
	}

	ClashCombatContext::Combatant CaptureCombatant(
		const Participant& a_participant,
		const std::string_view a_role)
	{
		ClashCombatContext::Combatant context;
		context.role = a_role;
		context.actorHandle = a_participant.actorHandle.native_handle();

		const auto actorPointer = a_participant.actorHandle.get();
		auto* actor = actorPointer.get();
		if (!actor) {
			return context;
		}

		context.available = true;
		context.actorFormID = actor->GetFormID();
		context.isPlayer = context.actorFormID == kPlayerFormID;
		if (const auto* actorName = actor->GetName(); actorName && actorName[0] != '\0') {
			context.actorName = actorName;
		}

		const auto activeWeapon = ResolveActiveWeapon(actor);
		const auto blocking = actor->IsBlocking();
		context.attackHand = activeWeapon.hand;
		context.attackClassification = AttackClassification(activeWeapon.attackData, blocking);
		context.staminaPercent = ActorValueUtils::ReadStaminaPercent(actor);
		context.weaponType = CombatContextUtils::ClassifyWeapon(activeWeapon.weapon);
		context.supportedMeleeWeapon =
			WeaponClassification::IsSupportedMelee(context.weaponType);
		if (activeWeapon.weapon) {
			context.weaponFormID = activeWeapon.weapon->GetFormID();
			if (const auto* weaponName = activeWeapon.weapon->GetName();
			    weaponName && weaponName[0] != '\0') {
				context.weaponName = weaponName;
			}
			const auto weight = activeWeapon.weapon->GetWeight();
			if (std::isfinite(weight)) {
				context.weaponWeight = weight;
			}
		}

		const auto weaponSkill = CombatContextUtils::RelevantWeaponSkill(activeWeapon.weapon);
		context.weaponSkillValue = ActorValueUtils::Read(actor, weaponSkill);
		context.weaponSkillName = CombatContextUtils::ActorValueName(
			context.weaponSkillValue ? weaponSkill : RE::ActorValue::kNone);

		const auto bash = activeWeapon.attackData &&
		                  activeWeapon.attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack);
		const auto skill = bash || blocking ? RE::ActorValue::kBlock : weaponSkill;
		context.skillValue = skill == weaponSkill ?
			context.weaponSkillValue : ActorValueUtils::Read(actor, skill);
		context.skillName = CombatContextUtils::ActorValueName(
			context.skillValue ? skill : RE::ActorValue::kNone);
		return context;
	}
}

ClashCombatContext::Snapshot ClashCombatContext::Capture(
	const LogicalClashNormalizer::Participant& a_attacker,
	const LogicalClashNormalizer::Participant& a_target)
{
	Snapshot snapshot;
	snapshot.capturedAt = std::chrono::steady_clock::now();

	// Resolve identity first so NPC-only clashes do not trigger process,
	// equipment, or actor-value reads.
	const auto attackerActor = a_attacker.actorHandle.get();
	const auto targetActor = a_target.actorHandle.get();
	const auto attackerIsPlayer = attackerActor && attackerActor->GetFormID() == kPlayerFormID;
	const auto targetIsPlayer = targetActor && targetActor->GetFormID() == kPlayerFormID;
	snapshot.playerInvolved = attackerIsPlayer || targetIsPlayer;
	if (!snapshot.playerInvolved) {
		return snapshot;
	}

	snapshot.staminaInfluence = Settings::Get().staminaInfluence;
	snapshot.skillInfluence = Settings::Get().skillInfluence;
	snapshot.attacker = CaptureCombatant(a_attacker, "attacker");
	snapshot.target = CaptureCombatant(a_target, "target");
	return snapshot;
}
