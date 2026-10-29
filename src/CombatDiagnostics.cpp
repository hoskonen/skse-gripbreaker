#include "CombatDiagnostics.h"

#include "ActorValueUtils.h"
#include "CombatContextUtils.h"
#include "DisarmEvaluation.h"
#include "GripEvaluation.h"
#include "PlayerDisarm.h"
#include "RandomRoll.h"
#include "Settings.h"

namespace
{
	constexpr std::string_view kRightSwingEvent = "weaponSwing";
	constexpr std::string_view kLeftSwingEvent = "weaponLeftSwing";
	constexpr std::string_view kBashEvent = "bashStart";
	constexpr std::string_view kBashPowerEvent = "bashPowerStart";
	constexpr std::string_view kBashIntroEvent = "SBF_BashIntroStart";
	constexpr std::string_view kBashPowerIntroEvent = "SBF_BashPowerStart";
	constexpr std::string_view kPreHitFrameEvent = "preHitFrame";
	constexpr std::string_view kHitFrameEvent = "HitFrame";

	struct AttackDetails
	{
		RE::NiPointer<RE::BGSAttackData> attackData;
		RE::TESForm*                    attackingForm{ nullptr };
		RE::TESObjectWEAP*              weapon{ nullptr };
		bool                            leftHand{ false };
		bool                            powerAttack{ false };
		bool                            bash{ false };
	};

	struct AttackBoundaryState
	{
		struct StartSnapshot
		{
			std::optional<float> staminaPercent;
			std::optional<bool> inferredLeftHand;
			std::string boundary;
			bool attempted{ false };
		};

		bool armed{ false };
		bool emitted{ false };
		StartSnapshot startSnapshot;
	};

	AttackBoundaryState attackBoundaryState;

	RE::ActorValue GetRelevantSkill(const AttackDetails& a_details)
	{
		if (a_details.bash) {
			return RE::ActorValue::kBlock;
		}

		if (const auto* armor = a_details.attackingForm ? a_details.attackingForm->As<RE::TESObjectARMO>() : nullptr;
			armor && armor->IsShield()) {
			return RE::ActorValue::kBlock;
		}

		return CombatContextUtils::RelevantWeaponSkill(a_details.weapon);
	}

	GripEvaluation::SkillType GetGripSkillType(RE::ActorValue a_actorValue)
	{
		switch (a_actorValue) {
		case RE::ActorValue::kOneHanded:
			return GripEvaluation::SkillType::kOneHanded;
		case RE::ActorValue::kTwoHanded:
			return GripEvaluation::SkillType::kTwoHanded;
		case RE::ActorValue::kBlock:
			return GripEvaluation::SkillType::kBlock;
		default:
			return GripEvaluation::SkillType::kUnavailable;
		}
	}

	AttackBoundaryState::StartSnapshot CaptureAttackStartSnapshot(
		const RE::PlayerCharacter* a_player,
		std::string_view a_eventName)
	{
		AttackBoundaryState::StartSnapshot snapshot;
		snapshot.attempted = true;
		snapshot.boundary = a_eventName;
		snapshot.staminaPercent = ActorValueUtils::ReadStaminaPercent(a_player);
		if (a_eventName == kLeftSwingEvent) {
			snapshot.inferredLeftHand = true;
		} else if (a_eventName == kRightSwingEvent) {
			snapshot.inferredLeftHand = false;
		}
		return snapshot;
	}

	AttackDetails ResolveAttack(RE::PlayerCharacter* a_player, std::string_view a_eventName)
	{
		AttackDetails details;
		if (!a_player) {
			return details;
		}

		const auto* process = a_player->GetActorRuntimeData().currentProcess;
		const auto* high = process ? process->high : nullptr;
		if (high && high->attackData) {
			details.attackData = high->attackData;
			details.leftHand = details.attackData->IsLeftAttack();
			details.powerAttack = details.attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
			details.bash = details.attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack);
		}

		if (a_eventName == kLeftSwingEvent) {
			details.leftHand = true;
		} else if (a_eventName == kRightSwingEvent) {
			details.leftHand = false;
		} else if (!details.attackData) {
			const auto* leftForm = a_player->GetEquippedObject(true);
			const auto* leftArmor = leftForm ? leftForm->As<RE::TESObjectARMO>() : nullptr;
			details.leftHand = leftArmor && leftArmor->IsShield();
		}

		details.attackingForm = a_player->GetEquippedObject(details.leftHand);
		details.weapon = details.attackingForm ? details.attackingForm->As<RE::TESObjectWEAP>() : nullptr;
		return details;
	}

	bool IsBashArmEvent(std::string_view a_eventName)
	{
		return a_eventName == kBashEvent ||
		       a_eventName == kBashPowerEvent ||
		       a_eventName == kBashIntroEvent ||
		       a_eventName == kBashPowerIntroEvent ||
		       a_eventName == kPreHitFrameEvent;
	}

	bool IsDiagnosticBoundary(std::string_view a_eventName)
	{
		return a_eventName == kRightSwingEvent ||
		       a_eventName == kLeftSwingEvent ||
		       a_eventName == kHitFrameEvent ||
		       IsBashArmEvent(a_eventName);
	}

	bool IsAttackArmEvent(std::string_view a_eventName)
	{
		return a_eventName == kRightSwingEvent ||
		       a_eventName == kLeftSwingEvent ||
		       IsBashArmEvent(a_eventName);
	}

	std::string_view GetAttackClassification(const AttackDetails& a_details)
	{
		if (a_details.bash) {
			return a_details.powerAttack ? "PowerBash" : "Bash";
		}
		return a_details.powerAttack ? "PowerAttack" : "Attack";
	}

	bool IsMeleeAttack(const AttackDetails& a_details)
	{
		return a_details.bash || !a_details.attackingForm || (a_details.weapon && a_details.weapon->IsMelee());
	}

	void LogAttackStaminaTiming(
		const AttackBoundaryState::StartSnapshot& a_startSnapshot,
		const AttackDetails& a_details,
		std::string_view a_skillName,
		const std::optional<float>& a_hitFrameStamina,
		const std::optional<float>& a_effectiveThreshold)
	{
		const auto hitFrameText = a_hitFrameStamina ?
			std::format("{:.1f}%", *a_hitFrameStamina) :
			std::string{ "Unavailable" };
		const auto thresholdText = a_effectiveThreshold ?
			std::format("{:.1f}%", *a_effectiveThreshold) :
			std::string{ "Unavailable" };
		const auto boundary = a_startSnapshot.boundary.empty() ?
			std::string_view{ "Unavailable" } :
			std::string_view{ a_startSnapshot.boundary };

		if (!a_startSnapshot.attempted) {
			logs::info(
				"Attack stamina timing: classification={} hand={} skill={} startBoundary={} startStamina=Unavailable "
				"hitFrameStamina={} delta=Unavailable effectiveThreshold={} reason=missing_start_snapshot",
				GetAttackClassification(a_details),
				a_details.leftHand ? "left" : "right",
				a_skillName,
				boundary,
				hitFrameText,
				thresholdText);
			return;
		}

		if (a_startSnapshot.inferredLeftHand && *a_startSnapshot.inferredLeftHand != a_details.leftHand) {
			logs::info(
				"Attack stamina timing: classification={} hand={} skill={} startBoundary={} startStamina=Ambiguous "
				"hitFrameStamina={} delta=Unavailable effectiveThreshold={} reason=start_hand_mismatch",
				GetAttackClassification(a_details),
				a_details.leftHand ? "left" : "right",
				a_skillName,
				boundary,
				hitFrameText,
				thresholdText);
			return;
		}

		if (!a_startSnapshot.staminaPercent) {
			logs::info(
				"Attack stamina timing: classification={} hand={} skill={} startBoundary={} startStamina=Unavailable "
				"hitFrameStamina={} delta=Unavailable effectiveThreshold={} reason=start_stamina_unavailable",
				GetAttackClassification(a_details),
				a_details.leftHand ? "left" : "right",
				a_skillName,
				boundary,
				hitFrameText,
				thresholdText);
			return;
		}

		const auto startText = std::format("{:.1f}%", *a_startSnapshot.staminaPercent);
		if (!a_hitFrameStamina) {
			logs::info(
				"Attack stamina timing: classification={} hand={} skill={} startBoundary={} startStamina={} "
				"hitFrameStamina=Unavailable delta=Unavailable effectiveThreshold={} reason=hit_frame_stamina_unavailable",
				GetAttackClassification(a_details),
				a_details.leftHand ? "left" : "right",
				a_skillName,
				boundary,
				startText,
				thresholdText);
			return;
		}

		logs::info(
			"Attack stamina timing: classification={} hand={} skill={} startBoundary={} startStamina={} "
			"hitFrameStamina={} delta={:+.1f}pp effectiveThreshold={}",
			GetAttackClassification(a_details),
			a_details.leftHand ? "left" : "right",
			a_skillName,
			boundary,
			startText,
			hitFrameText,
			*a_hitFrameStamina - *a_startSnapshot.staminaPercent,
			thresholdText);
	}

	void ProcessAttack(
		RE::PlayerCharacter* a_player,
		std::string_view a_eventName,
		const AttackDetails& a_details,
		const AttackBoundaryState::StartSnapshot& a_startSnapshot)
	{
		if (!a_player) {
			return;
		}

		if (!IsMeleeAttack(a_details)) {
			return;
		}

		const auto* name = a_details.attackingForm ? a_details.attackingForm->GetName() : nullptr;
		const auto weaponName = name && name[0] != '\0' ? std::string_view{ name } : std::string_view{ "<unarmed>" };
		const auto* armor = a_details.attackingForm ? a_details.attackingForm->As<RE::TESObjectARMO>() : nullptr;
		const auto weaponType = armor && armor->IsShield() ?
			std::string_view{ "Shield" } :
			(a_details.weapon ? CombatContextUtils::WeaponTypeName(a_details.weapon) : std::string_view{ "Unarmed" });
		const auto skill = GetRelevantSkill(a_details);
		const auto skillValue = ActorValueUtils::Read(a_player, skill);
		const auto skillName = skillValue ?
			CombatContextUtils::ActorValueName(skill) : std::string_view{ "Unavailable" };
		const auto skillValueText = skillValue ? std::format("{:.1f}", *skillValue) : std::string{ "Unavailable" };
		const auto staminaPercent = ActorValueUtils::ReadStaminaPercent(a_player);
		const auto staminaText = staminaPercent ? std::format("{:.1f}%", *staminaPercent) : std::string{ "Unavailable" };
		const auto formID = a_details.attackingForm ? a_details.attackingForm->GetFormID() : 0;
		const auto* attackDataEvent = a_details.attackData ? a_details.attackData->event.c_str() : nullptr;
		const auto& settings = Settings::Get();

		if (settings.debugLogging) {
			logs::info(
				"Combat diagnostic: event={} attackDataEvent={} classification={} hand={} weapon=\"{}\" formID={:08X} "
				"type={} powerAttack={} bash={} stamina={} skill={} skillValue={}",
				a_eventName,
				attackDataEvent && attackDataEvent[0] != '\0' ? attackDataEvent : "<unavailable>",
				GetAttackClassification(a_details),
				a_details.leftHand ? "left" : "right",
				weaponName,
				formID,
				weaponType,
				a_details.powerAttack,
				a_details.bash,
				staminaText,
				skillName,
				skillValueText);
		}

		const auto gripSkill = GetGripSkillType(skill);
		const auto gripContext = a_details.bash ? std::string_view{ "Block" } : std::string_view{ "Attack" };
		if (!staminaPercent || !skillValue || gripSkill == GripEvaluation::SkillType::kUnavailable) {
			if (settings.debugLogging) {
				LogAttackStaminaTiming(
					a_startSnapshot,
					a_details,
					GripEvaluation::SkillName(gripSkill),
					staminaPercent,
					std::nullopt);
				logs::info(
					"Grip evaluation: context={} stamina={} skill={} skillValue={} result=Unavailable",
					gripContext,
					staminaText,
					GripEvaluation::SkillName(gripSkill),
					skillValueText);
			}
			return;
		}

		auto formula = GripEvaluation::kDefaultFormula;
		formula.baseStaminaThreshold = static_cast<float>(settings.staminaThreshold);
		const auto evaluation = GripEvaluation::Evaluate({ *staminaPercent, gripSkill, *skillValue }, formula);
		if (settings.debugLogging) {
			LogAttackStaminaTiming(
				a_startSnapshot,
				a_details,
				GripEvaluation::SkillName(gripSkill),
				staminaPercent,
				evaluation.effectiveStaminaThreshold);
			logs::info(
				"Grip evaluation: context={} stamina={:.1f}% skill={} skillValue={:.1f} baseThreshold={:.1f}% "
				"skillModifier={:.3f} effectiveThreshold={:.1f}% fatigueRisk={:.3f}",
				gripContext,
				*staminaPercent,
				GripEvaluation::SkillName(gripSkill),
				*skillValue,
				evaluation.baseStaminaThreshold,
				evaluation.skillModifier,
				evaluation.effectiveStaminaThreshold,
				evaluation.fatigueRisk);
		}

		if (!settings.enabled || !settings.fatigueDisarms ||
		    evaluation.fatigueRisk <= 0.0f || !a_details.weapon) {
			return;
		}

		const PlayerDisarm::Request dropRequest{ a_details.weapon->GetFormID(), a_details.leftHand };
		if (!PlayerDisarm::CanDropAttackingWeapon(dropRequest, settings.debugLogging)) {
			return;
		}

		const auto disarm = DisarmEvaluation::Evaluate(
			evaluation.fatigueRisk,
			static_cast<float>(settings.baseDropChance));
		const auto roll = RandomRoll::Percent();
		const auto shouldDrop = roll < disarm.dropChancePercent;
		if (settings.debugLogging) {
			logs::info(
				"Disarm evaluation: fatigueRisk={:.3f} riskCurve={:.3f} baseChance={:.1f}% "
				"dropChance={:.1f}% roll={:.3f} result={}",
				disarm.fatigueRisk,
				disarm.riskCurve,
				disarm.baseChancePercent,
				disarm.dropChancePercent,
				roll,
				shouldDrop ? "DROP" : "KEEP");
		}

		if (!shouldDrop) {
			return;
		}

		(void)PlayerDisarm::TryDropAttackingWeapon(
			dropRequest, settings.dropPhysics, settings.debugLogging);
	}

	void InspectPlayerAnimationEvent(std::string a_eventName)
	{
		const auto& settings = Settings::Get();
		if (!settings.enabled && !settings.debugLogging) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		if (IsAttackArmEvent(a_eventName)) {
			if (!attackBoundaryState.armed) {
				attackBoundaryState.startSnapshot = settings.debugLogging ?
					CaptureAttackStartSnapshot(player, a_eventName) :
					AttackBoundaryState::StartSnapshot{};
			}
			attackBoundaryState.armed = true;
			attackBoundaryState.emitted = false;
			return;
		}

		if (a_eventName == kHitFrameEvent &&
		    attackBoundaryState.armed && !attackBoundaryState.emitted) {
			const auto details = ResolveAttack(player, a_eventName);
			const auto startSnapshot = attackBoundaryState.startSnapshot;
			ProcessAttack(player, a_eventName, details, startSnapshot);
			attackBoundaryState.armed = false;
			attackBoundaryState.emitted = true;
			attackBoundaryState.startSnapshot = {};
		}
	}

	class AnimationEventSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::BSAnimationGraphEvent* a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
		{
			const auto& settings = Settings::Get();
			if (!a_event || (!settings.enabled && !settings.debugLogging)) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || a_event->holder != player) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto* eventTag = a_event->tag.c_str();
			if (!eventTag || eventTag[0] == '\0') {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!IsDiagnosticBoundary(eventTag)) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto* taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface) {
				logs::warn("Could not queue combat diagnostic: SKSE task interface is unavailable");
				return RE::BSEventNotifyControl::kContinue;
			}

			taskInterface->AddTask([eventName = std::string{ eventTag }]() mutable {
				InspectPlayerAnimationEvent(std::move(eventName));
			});

			return RE::BSEventNotifyControl::kContinue;
		}
	};

	AnimationEventSink animationEventSink;

	bool IsRegistered(const RE::PlayerCharacter& a_player)
	{
		RE::BSAnimationGraphManagerPtr graphManager;
		a_player.GetAnimationGraphManager(graphManager);
		if (!graphManager) {
			return false;
		}

		for (const auto& animationGraph : graphManager->graphs) {
			if (!animationGraph) {
				continue;
			}

			const auto* eventSource = animationGraph->GetEventSource<RE::BSAnimationGraphEvent>();
			if (!eventSource) {
				continue;
			}

			for (const auto* sink : eventSource->sinks) {
				if (sink == std::addressof(animationEventSink)) {
					return true;
				}
			}
		}

		return false;
	}
}

void CombatDiagnostics::Register()
{
	auto* player = RE::PlayerCharacter::GetSingleton();
	if (!player) {
		logs::error("Could not register combat diagnostics: player is unavailable");
		return;
	}

	if (IsRegistered(*player)) {
		logs::debug("Player melee attack diagnostics are already registered");
		return;
	}

	RE::BSAnimationGraphManagerPtr graphManager;
	player->GetAnimationGraphManager(graphManager);
	if (!graphManager || graphManager->graphs.empty() || !graphManager->graphs.front()) {
		logs::error("Could not register combat diagnostics: player animation graph is unavailable");
		return;
	}

	if (!player->AddAnimationGraphEventSink(std::addressof(animationEventSink))) {
		logs::error("Could not register combat diagnostics: player animation graph is unavailable");
		return;
	}

	logs::info("Registered player melee attack diagnostics");
}
