#include "MassMatchupEvaluation.h"

#include <cmath>

namespace
{
	MassMatchupEvaluation::Category Classify(const float a_massAdvantage) noexcept
	{
		using Category = MassMatchupEvaluation::Category;
		using namespace MassMatchupEvaluation;

		if (a_massAdvantage < -kMajorAdvantageThreshold) {
			return Category::kMajorPlayerAdvantage;
		}
		if (a_massAdvantage < -kEvenThreshold) {
			return Category::kPlayerAdvantage;
		}
		if (a_massAdvantage <= kEvenThreshold) {
			return Category::kEven;
		}
		if (a_massAdvantage <= kMajorAdvantageThreshold) {
			return Category::kOpponentAdvantage;
		}
		return Category::kMajorOpponentAdvantage;
	}
}

MassMatchupEvaluation::Result MassMatchupEvaluation::Evaluate(const Input& a_input) noexcept
{
	Result result;
	if (!a_input.playerActorAvailable) {
		result.reason = UnavailableReason::kPlayerActorUnavailable;
		return result;
	}
	if (!a_input.opponentActorAvailable) {
		result.reason = UnavailableReason::kOpponentActorUnavailable;
		return result;
	}
	if (!a_input.playerWeaponSupported) {
		result.reason = UnavailableReason::kPlayerWeaponUnsupported;
		return result;
	}
	if (!a_input.opponentWeaponSupported) {
		result.reason = UnavailableReason::kOpponentWeaponUnsupported;
		return result;
	}
	if (!a_input.playerWeight) {
		result.reason = UnavailableReason::kPlayerWeightUnavailable;
		return result;
	}
	if (!a_input.opponentWeight) {
		result.reason = UnavailableReason::kOpponentWeightUnavailable;
		return result;
	}

	result.playerWeight = *a_input.playerWeight;
	result.opponentWeight = *a_input.opponentWeight;
	if (!std::isfinite(result.playerWeight)) {
		result.reason = UnavailableReason::kPlayerWeightNotFinite;
		return result;
	}
	if (!std::isfinite(result.opponentWeight)) {
		result.reason = UnavailableReason::kOpponentWeightNotFinite;
		return result;
	}
	if (result.playerWeight <= 0.0f) {
		result.reason = UnavailableReason::kPlayerWeightNotPositive;
		return result;
	}
	if (result.opponentWeight <= 0.0f) {
		result.reason = UnavailableReason::kOpponentWeightNotPositive;
		return result;
	}

	const auto denominator = result.opponentWeight + result.playerWeight;
	if (!std::isfinite(denominator) || denominator <= 0.0f) {
		result.reason = UnavailableReason::kInvalidDenominator;
		return result;
	}

	result.massAdvantage =
		(result.opponentWeight - result.playerWeight) / denominator;
	if (!std::isfinite(result.massAdvantage)) {
		result.massAdvantage = 0.0f;
		result.reason = UnavailableReason::kInvalidResult;
		return result;
	}

	result.available = true;
	result.category = Classify(result.massAdvantage);
	return result;
}

std::string_view MassMatchupEvaluation::CategoryName(const Category a_category) noexcept
{
	switch (a_category) {
	case Category::kMajorPlayerAdvantage:
		return "MajorPlayerAdvantage";
	case Category::kPlayerAdvantage:
		return "PlayerAdvantage";
	case Category::kEven:
		return "Even";
	case Category::kOpponentAdvantage:
		return "OpponentAdvantage";
	case Category::kMajorOpponentAdvantage:
		return "MajorOpponentAdvantage";
	default:
		return "Unavailable";
	}
}

std::string_view MassMatchupEvaluation::ReasonName(const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kPlayerActorUnavailable:
		return "player_actor_unavailable";
	case UnavailableReason::kOpponentActorUnavailable:
		return "opponent_actor_unavailable";
	case UnavailableReason::kPlayerWeaponUnsupported:
		return "player_weapon_unsupported";
	case UnavailableReason::kOpponentWeaponUnsupported:
		return "opponent_weapon_unsupported";
	case UnavailableReason::kPlayerWeightUnavailable:
		return "player_weight_unavailable";
	case UnavailableReason::kOpponentWeightUnavailable:
		return "opponent_weight_unavailable";
	case UnavailableReason::kPlayerWeightNotFinite:
		return "player_weight_not_finite";
	case UnavailableReason::kOpponentWeightNotFinite:
		return "opponent_weight_not_finite";
	case UnavailableReason::kPlayerWeightNotPositive:
		return "player_weight_not_positive";
	case UnavailableReason::kOpponentWeightNotPositive:
		return "opponent_weight_not_positive";
	case UnavailableReason::kInvalidDenominator:
		return "invalid_denominator";
	case UnavailableReason::kInvalidResult:
		return "invalid_result";
	default:
		return "none";
	}
}
