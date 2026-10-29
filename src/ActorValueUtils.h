#pragma once

namespace ActorValueUtils
{
	[[nodiscard]] std::optional<float> Read(
		const RE::Actor* a_actor,
		RE::ActorValue a_actorValue);

	[[nodiscard]] std::optional<float> ReadStaminaPercent(const RE::Actor* a_actor);
}
