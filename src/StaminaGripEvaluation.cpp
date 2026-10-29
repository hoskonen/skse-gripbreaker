#include "StaminaGripEvaluation.h"

#include <algorithm>
#include <cmath>

StaminaGripEvaluation::Result StaminaGripEvaluation::Evaluate(const Input& a_input) noexcept
{
	Result result;
	if (!std::isfinite(a_input.normalizedStamina)) {
		result.reason = UnavailableReason::kStaminaNotFinite;
		return result;
	}
	if (!std::isfinite(a_input.influencePercent)) {
		result.reason = UnavailableReason::kInfluenceNotFinite;
		return result;
	}
	if (!std::isfinite(a_input.baseGripFactor)) {
		result.reason = UnavailableReason::kBaseGripNotFinite;
		return result;
	}
	if (a_input.baseGripFactor <= 0.0f) {
		result.reason = UnavailableReason::kBaseGripNotPositive;
		return result;
	}

	result.normalizedStamina = std::clamp(a_input.normalizedStamina, 0.0f, 1.0f);
	result.influence = std::clamp(a_input.influencePercent / 100.0f, 0.0f, 1.0f);
	const auto exhaustion = 1.0f - result.normalizedStamina;
	result.fullInfluenceFactor =
		1.0f - kMaximumStaminaPenalty * exhaustion * exhaustion;
	result.staminaFactor =
		1.0f + (result.fullInfluenceFactor - 1.0f) * result.influence;
	result.staminaFactor =
		std::clamp(result.staminaFactor, kMinimumFullInfluenceFactor, 1.0f);
	result.effectiveGrip = a_input.baseGripFactor * result.staminaFactor;

	if (!std::isfinite(result.fullInfluenceFactor) ||
	    !std::isfinite(result.staminaFactor) ||
	    !std::isfinite(result.effectiveGrip)) {
		result.fullInfluenceFactor = 0.0f;
		result.staminaFactor = 0.0f;
		result.effectiveGrip = 0.0f;
		result.reason = UnavailableReason::kInvalidResult;
		return result;
	}

	result.available = true;
	return result;
}

std::string_view StaminaGripEvaluation::ReasonName(const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kStaminaNotFinite:
		return "stamina_not_finite";
	case UnavailableReason::kInfluenceNotFinite:
		return "influence_not_finite";
	case UnavailableReason::kBaseGripNotFinite:
		return "base_grip_not_finite";
	case UnavailableReason::kBaseGripNotPositive:
		return "base_grip_not_positive";
	case UnavailableReason::kInvalidResult:
		return "invalid_result";
	default:
		return "none";
	}
}
