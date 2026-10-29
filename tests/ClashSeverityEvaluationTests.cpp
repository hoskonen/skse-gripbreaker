#include "ClashSeverityEvaluation.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	using Category = ClashSeverityEvaluation::Category;
	using Reason = ClashSeverityEvaluation::UnavailableReason;

	bool IsNear(const float a_actual, const float a_expected, const float a_tolerance = 0.001f)
	{
		return std::fabs(a_actual - a_expected) <= a_tolerance;
	}

	ClashSeverityEvaluation::Result Evaluate(
		const float a_ownWeight,
		const float a_incomingWeight,
		const float a_effectiveGrip)
	{
		return ClashSeverityEvaluation::Evaluate({
			true,
			true,
			a_ownWeight,
			a_incomingWeight,
			a_effectiveGrip
		});
	}

	bool ExpectAvailable(
		const std::string_view a_name,
		const ClashSeverityEvaluation::Result& a_result,
		const float a_expectedPressure,
		const float a_expectedSeverity,
		const Category a_expectedCategory)
	{
		if (a_result.available &&
		    std::isfinite(a_result.incomingMassPressure) &&
		    std::isfinite(a_result.severity) &&
		    IsNear(a_result.incomingMassPressure, a_expectedPressure) &&
		    IsNear(a_result.severity, a_expectedSeverity) &&
		    a_result.category == a_expectedCategory) {
			return true;
		}

		std::cerr << a_name
		          << ": available=" << a_result.available
		          << " pressure=" << a_result.incomingMassPressure
		          << " severity=" << a_result.severity
		          << " category=" << ClashSeverityEvaluation::CategoryName(a_result.category)
		          << " reason=" << ClashSeverityEvaluation::ReasonName(a_result.reason)
		          << '\n';
		return false;
	}

	bool ExpectUnavailable(
		const std::string_view a_name,
		const ClashSeverityEvaluation::Result& a_result,
		const Reason a_expectedReason)
	{
		if (!a_result.available && a_result.reason == a_expectedReason) {
			return true;
		}
		std::cerr << a_name << ": unexpected result/reason\n";
		return false;
	}
}

int main()
{
	bool passed = true;

	passed &= ExpectAvailable("equal", Evaluate(10.0f, 10.0f, 1.0f), 1.0f, 1.0f, Category::kHigh);
	passed &= ExpectAvailable("heavy incoming", Evaluate(2.0f, 11.0f, 1.0f), 5.5f, 5.5f, Category::kSevere);
	passed &= ExpectAvailable("light incoming", Evaluate(16.0f, 11.0f, 1.0f), 0.6875f, 0.6875f, Category::kLow);
	passed &= ExpectAvailable("low grip", Evaluate(10.0f, 10.0f, 0.5f), 1.0f, 2.0f, Category::kSevere);
	passed &= ExpectAvailable("high grip", Evaluate(10.0f, 10.0f, 2.0f), 1.0f, 0.5f, Category::kLow);

	const auto heavierIncoming = Evaluate(10.0f, 20.0f, 1.0f);
	const auto lighterIncoming = Evaluate(10.0f, 5.0f, 1.0f);
	if (!heavierIncoming.available || !lighterIncoming.available ||
	    heavierIncoming.severity <= lighterIncoming.severity) {
		std::cerr << "heavier incoming weapon did not increase severity\n";
		passed = false;
	}

	const auto weakerGrip = Evaluate(10.0f, 10.0f, 0.5f);
	const auto strongerGrip = Evaluate(10.0f, 10.0f, 1.5f);
	if (!weakerGrip.available || !strongerGrip.available ||
	    weakerGrip.severity <= strongerGrip.severity) {
		std::cerr << "stronger grip did not reduce severity\n";
		passed = false;
	}

	const auto forward = Evaluate(10.0f, 20.0f, 1.0f);
	const auto reversed = Evaluate(20.0f, 10.0f, 1.0f);
	if (!forward.available || !reversed.available ||
	    !IsNear(forward.incomingMassPressure * reversed.incomingMassPressure, 1.0f)) {
		std::cerr << "swapping weights did not invert directional pressure\n";
		passed = false;
	}

	passed &= ExpectAvailable(
		"greatsword some stamina",
		Evaluate(16.0f, 11.0f, 1.042f),
		0.6875f,
		0.6875f / 1.042f,
		Category::kLow);
	passed &= ExpectAvailable(
		"greatsword exhausted",
		Evaluate(16.0f, 11.0f, 0.827f),
		0.6875f,
		0.6875f / 0.827f,
		Category::kModerate);
	passed &= ExpectAvailable(
		"dagger exhausted",
		Evaluate(2.0f, 11.0f, 0.665f),
		5.5f,
		5.5f / 0.665f,
		Category::kSevere);

	const auto infinity = std::numeric_limits<float>::infinity();
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	passed &= ExpectUnavailable("zero own weight", Evaluate(0.0f, 10.0f, 1.0f), Reason::kOwnWeightNotPositive);
	passed &= ExpectUnavailable("negative incoming weight", Evaluate(10.0f, -1.0f, 1.0f), Reason::kIncomingWeightNotPositive);
	passed &= ExpectUnavailable("non-finite own weight", Evaluate(infinity, 10.0f, 1.0f), Reason::kOwnWeightNotFinite);
	passed &= ExpectUnavailable("non-finite incoming weight", Evaluate(10.0f, nan, 1.0f), Reason::kIncomingWeightNotFinite);
	passed &= ExpectUnavailable("zero grip", Evaluate(10.0f, 10.0f, 0.0f), Reason::kEffectiveGripNotPositive);
	passed &= ExpectUnavailable("non-finite grip", Evaluate(10.0f, 10.0f, infinity), Reason::kEffectiveGripNotFinite);
	passed &= ExpectUnavailable(
		"unsupported own weapon",
		ClashSeverityEvaluation::Evaluate({ false, true, 10.0f, 10.0f, 1.0f }),
		Reason::kOwnWeaponUnsupported);
	passed &= ExpectUnavailable(
		"unsupported incoming weapon",
		ClashSeverityEvaluation::Evaluate({ true, false, 10.0f, 10.0f, 1.0f }),
		Reason::kIncomingWeaponUnsupported);
	passed &= ExpectUnavailable(
		"missing own weight",
		ClashSeverityEvaluation::Evaluate({ true, true, std::nullopt, 10.0f, 1.0f }),
		Reason::kOwnWeightUnavailable);
	passed &= ExpectUnavailable(
		"missing incoming weight",
		ClashSeverityEvaluation::Evaluate({ true, true, 10.0f, std::nullopt, 1.0f }),
		Reason::kIncomingWeightUnavailable);
	passed &= ExpectUnavailable(
		"missing effective grip",
		ClashSeverityEvaluation::Evaluate({ true, true, 10.0f, 10.0f, std::nullopt }),
		Reason::kEffectiveGripUnavailable);

	return passed ? 0 : 1;
}
