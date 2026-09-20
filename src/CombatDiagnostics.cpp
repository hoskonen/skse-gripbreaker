#include "CombatDiagnostics.h"

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

	struct BashBoundaryState
	{
		bool armed{ false };
		bool emitted{ false };
	};

	BashBoundaryState bashBoundaryState;

	std::string_view GetWeaponTypeName(const RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return "Unarmed";
		}

		switch (a_weapon->GetWeaponType()) {
		case RE::WEAPON_TYPE::kHandToHandMelee:
			return "HandToHand";
		case RE::WEAPON_TYPE::kOneHandSword:
			return "OneHandSword";
		case RE::WEAPON_TYPE::kOneHandDagger:
			return "OneHandDagger";
		case RE::WEAPON_TYPE::kOneHandAxe:
			return "OneHandAxe";
		case RE::WEAPON_TYPE::kOneHandMace:
			return "OneHandMace";
		case RE::WEAPON_TYPE::kTwoHandSword:
			return "TwoHandSword";
		case RE::WEAPON_TYPE::kTwoHandAxe:
			return "TwoHandAxe";
		case RE::WEAPON_TYPE::kBow:
			return "Bow";
		case RE::WEAPON_TYPE::kStaff:
			return "Staff";
		case RE::WEAPON_TYPE::kCrossbow:
			return "Crossbow";
		default:
			return "Unknown";
		}
	}

	std::string_view GetActorValueName(RE::ActorValue a_actorValue)
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

	RE::ActorValue GetRelevantSkill(const AttackDetails& a_details)
	{
		if (const auto* armor = a_details.attackingForm ? a_details.attackingForm->As<RE::TESObjectARMO>() : nullptr;
			armor && armor->IsShield()) {
			return RE::ActorValue::kBlock;
		}

		if (!a_details.weapon) {
			return RE::ActorValue::kNone;
		}

		const auto skill = a_details.weapon->weaponData.skill.get();
		switch (skill) {
		case RE::ActorValue::kOneHanded:
		case RE::ActorValue::kTwoHanded:
		case RE::ActorValue::kBlock:
			return skill;
		default:
			return RE::ActorValue::kNone;
		}
	}

	std::optional<float> ReadActorValue(const RE::Actor* a_actor, RE::ActorValue a_actorValue)
	{
		if (!a_actor || a_actorValue == RE::ActorValue::kNone) {
			return std::nullopt;
		}

		const auto* actorValueOwner = a_actor->AsActorValueOwner();
		if (!actorValueOwner) {
			return std::nullopt;
		}

		const auto value = actorValueOwner->GetActorValue(a_actorValue);
		return std::isfinite(value) ? std::optional<float>{ value } : std::nullopt;
	}

	std::optional<float> GetStaminaPercent(const RE::Actor* a_actor)
	{
		if (!a_actor) {
			return std::nullopt;
		}

		const auto* actorValueOwner = a_actor->AsActorValueOwner();
		if (!actorValueOwner) {
			return std::nullopt;
		}

		const auto current = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
		const auto maximum = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina) +
		                     a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
		if (!std::isfinite(current) || !std::isfinite(maximum) || maximum <= 0.0f) {
			return std::nullopt;
		}

		return std::clamp((current / maximum) * 100.0f, 0.0f, 100.0f);
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

	void LogAttack(
		RE::PlayerCharacter* a_player,
		std::string_view a_eventName,
		const AttackDetails& a_details)
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
		const auto weaponType = armor && armor->IsShield() ? std::string_view{ "Shield" } : GetWeaponTypeName(a_details.weapon);
		const auto skill = GetRelevantSkill(a_details);
		const auto skillValue = ReadActorValue(a_player, skill);
		const auto skillName = skillValue ? GetActorValueName(skill) : std::string_view{ "Unavailable" };
		const auto skillValueText = skillValue ? std::format("{:.1f}", *skillValue) : std::string{ "Unavailable" };
		const auto staminaPercent = GetStaminaPercent(a_player);
		const auto staminaText = staminaPercent ? std::format("{:.1f}%", *staminaPercent) : std::string{ "Unavailable" };
		const auto formID = a_details.attackingForm ? a_details.attackingForm->GetFormID() : 0;
		const auto* attackDataEvent = a_details.attackData ? a_details.attackData->event.c_str() : nullptr;

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

	void InspectPlayerAnimationEvent(std::string a_eventName)
	{
		if (!Settings::Get().debugLogging) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		const auto details = ResolveAttack(player, a_eventName);
		if (details.bash) {
			if (IsBashArmEvent(a_eventName)) {
				bashBoundaryState.armed = true;
				bashBoundaryState.emitted = false;
				return;
			}

			if (a_eventName == kHitFrameEvent &&
			    (bashBoundaryState.armed || !bashBoundaryState.emitted)) {
				LogAttack(player, a_eventName, details);
				bashBoundaryState.armed = false;
				bashBoundaryState.emitted = true;
			}
			return;
		}

		if (a_eventName == kRightSwingEvent || a_eventName == kLeftSwingEvent) {
			bashBoundaryState = {};
			LogAttack(player, a_eventName, details);
		}
	}

	class AnimationEventSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::BSAnimationGraphEvent* a_event,
			RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
		{
			if (!a_event || !Settings::Get().debugLogging) {
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
