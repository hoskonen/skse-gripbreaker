#include "ClashDisarmDecision.h"

#include <cmath>

bool ClashDisarmDecision::CanRoll(const std::optional<float> a_chancePercent) noexcept
{
	return a_chancePercent && std::isfinite(*a_chancePercent) &&
	       *a_chancePercent >= 0.0f && *a_chancePercent <= 100.0f;
}

ClashDisarmDecision::Result ClashDisarmDecision::Evaluate(
	const std::optional<float> a_chancePercent, const float a_roll) noexcept
{
	if (!a_chancePercent) {
		return { Outcome::kSkipped, std::nullopt, "probability_unavailable" };
	}
	if (!CanRoll(a_chancePercent)) {
		return { Outcome::kSkipped, std::nullopt, "probability_invalid" };
	}
	if (!std::isfinite(a_roll) || a_roll < 0.0f || a_roll >= 100.0f) {
		return { Outcome::kSkipped, std::nullopt, "roll_invalid" };
	}
	return { a_roll < *a_chancePercent ? Outcome::kDrop : Outcome::kKeep, a_roll, {} };
}
