#include "ActorValueUtils.h"

std::optional<float> ActorValueUtils::Read(
	const RE::Actor* a_actor,
	const RE::ActorValue a_actorValue)
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

std::optional<float> ActorValueUtils::ReadStaminaPercent(const RE::Actor* a_actor)
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
	                     a_actor->GetActorValueModifier(
					 RE::ACTOR_VALUE_MODIFIER::kTemporary,
					 RE::ActorValue::kStamina);
	if (!std::isfinite(current) || !std::isfinite(maximum) || maximum <= 0.0f) {
		return std::nullopt;
	}

	return std::clamp((current / maximum) * 100.0f, 0.0f, 100.0f);
}
