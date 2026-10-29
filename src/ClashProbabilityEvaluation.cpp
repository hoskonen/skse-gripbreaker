#include "ClashProbabilityEvaluation.h"

#include <cmath>

ClashProbabilityEvaluation::Result ClashProbabilityEvaluation::Evaluate(
	const std::optional<float> a_severity) noexcept
{
	if (!a_severity) {
		return { false, 0.0f, UnavailableReason::kSeverityUnavailable };
	}
	if (!std::isfinite(*a_severity)) {
		return { false, 0.0f, UnavailableReason::kSeverityNotFinite };
	}
	if (*a_severity < 0.0f) {
		return { false, 0.0f, UnavailableReason::kSeverityNegative };
	}
	if (*a_severity == 0.0f) {
		return { true, 0.0f, UnavailableReason::kNone };
	}

	// Hill curve: max * s^n / (half^n + s^n). Reciprocal form avoids
	// large powers of severity; double intermediates cover all finite float inputs.
	const auto ratio = Curve::kHalfMaximumSeverity / static_cast<double>(*a_severity);
	const auto chance = Curve::kMaximumChancePercent /
	                    (1.0 + std::pow(ratio, Curve::kExponent));
	return { true, static_cast<float>(chance), UnavailableReason::kNone };
}

std::string_view ClashProbabilityEvaluation::ReasonName(const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kSeverityUnavailable:
		return "severity_unavailable";
	case UnavailableReason::kSeverityNotFinite:
		return "severity_not_finite";
	case UnavailableReason::kSeverityNegative:
		return "severity_negative";
	default:
		return "none";
	}
}
