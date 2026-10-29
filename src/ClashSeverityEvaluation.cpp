#include "ClashSeverityEvaluation.h"

#include <cmath>

namespace
{
	ClashSeverityEvaluation::Category Classify(const float a_severity) noexcept
	{
		using Category = ClashSeverityEvaluation::Category;
		using namespace ClashSeverityEvaluation;

		if (a_severity < kModerateThreshold) {
			return Category::kLow;
		}
		if (a_severity < kHighThreshold) {
			return Category::kModerate;
		}
		if (a_severity < kSevereThreshold) {
			return Category::kHigh;
		}
		return Category::kSevere;
	}
}

ClashSeverityEvaluation::Result ClashSeverityEvaluation::Evaluate(const Input& a_input) noexcept
{
	Result result;
	if (!a_input.ownWeaponSupported) {
		result.reason = UnavailableReason::kOwnWeaponUnsupported;
		return result;
	}
	if (!a_input.incomingWeaponSupported) {
		result.reason = UnavailableReason::kIncomingWeaponUnsupported;
		return result;
	}
	if (!a_input.ownWeaponWeight) {
		result.reason = UnavailableReason::kOwnWeightUnavailable;
		return result;
	}
	if (!a_input.incomingWeaponWeight) {
		result.reason = UnavailableReason::kIncomingWeightUnavailable;
		return result;
	}
	if (!a_input.effectiveGrip) {
		result.reason = UnavailableReason::kEffectiveGripUnavailable;
		return result;
	}

	const auto ownWeight = *a_input.ownWeaponWeight;
	const auto incomingWeight = *a_input.incomingWeaponWeight;
	result.effectiveGrip = *a_input.effectiveGrip;
	if (!std::isfinite(ownWeight)) {
		result.reason = UnavailableReason::kOwnWeightNotFinite;
		return result;
	}
	if (!std::isfinite(incomingWeight)) {
		result.reason = UnavailableReason::kIncomingWeightNotFinite;
		return result;
	}
	if (ownWeight <= 0.0f) {
		result.reason = UnavailableReason::kOwnWeightNotPositive;
		return result;
	}
	if (incomingWeight <= 0.0f) {
		result.reason = UnavailableReason::kIncomingWeightNotPositive;
		return result;
	}
	if (!std::isfinite(result.effectiveGrip)) {
		result.reason = UnavailableReason::kEffectiveGripNotFinite;
		return result;
	}
	if (result.effectiveGrip <= 0.0f) {
		result.reason = UnavailableReason::kEffectiveGripNotPositive;
		return result;
	}

	result.incomingMassPressure = incomingWeight / ownWeight;
	if (!std::isfinite(result.incomingMassPressure)) {
		result.incomingMassPressure = 0.0f;
		result.reason = UnavailableReason::kInvalidMassPressure;
		return result;
	}

	result.severity = result.incomingMassPressure / result.effectiveGrip;
	if (!std::isfinite(result.severity)) {
		result.severity = 0.0f;
		result.reason = UnavailableReason::kInvalidSeverity;
		return result;
	}

	result.available = true;
	result.category = Classify(result.severity);
	return result;
}

std::string_view ClashSeverityEvaluation::CategoryName(const Category a_category) noexcept
{
	switch (a_category) {
	case Category::kLow:
		return "Low";
	case Category::kModerate:
		return "Moderate";
	case Category::kHigh:
		return "High";
	case Category::kSevere:
		return "Severe";
	default:
		return "Unavailable";
	}
}

std::string_view ClashSeverityEvaluation::ReasonName(const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kOwnWeaponUnsupported:
		return "own_weapon_unsupported";
	case UnavailableReason::kIncomingWeaponUnsupported:
		return "incoming_weapon_unsupported";
	case UnavailableReason::kOwnWeightUnavailable:
		return "own_weight_unavailable";
	case UnavailableReason::kIncomingWeightUnavailable:
		return "incoming_weight_unavailable";
	case UnavailableReason::kOwnWeightNotFinite:
		return "own_weight_not_finite";
	case UnavailableReason::kIncomingWeightNotFinite:
		return "incoming_weight_not_finite";
	case UnavailableReason::kOwnWeightNotPositive:
		return "own_weight_not_positive";
	case UnavailableReason::kIncomingWeightNotPositive:
		return "incoming_weight_not_positive";
	case UnavailableReason::kEffectiveGripUnavailable:
		return "effective_grip_unavailable";
	case UnavailableReason::kEffectiveGripNotFinite:
		return "effective_grip_not_finite";
	case UnavailableReason::kEffectiveGripNotPositive:
		return "effective_grip_not_positive";
	case UnavailableReason::kInvalidMassPressure:
		return "invalid_mass_pressure";
	case UnavailableReason::kInvalidSeverity:
		return "invalid_severity";
	default:
		return "none";
	}
}
