#include "GripConfigurationEvaluation.h"

#include <cmath>
#include <iostream>

namespace
{
	using Type = WeaponClassification::Type;

	bool ExpectAvailable(
		const std::string_view a_name,
		const Type a_type,
		const int a_expectedHands,
		const float a_expectedFactor)
	{
		const auto result = GripConfigurationEvaluation::Evaluate(a_type);
		if (result.available && result.gripHands == a_expectedHands &&
		    std::fabs(result.gripFactor - a_expectedFactor) <= 0.0001f) {
			return true;
		}

		std::cerr << a_name << ": expected hands=" << a_expectedHands
		          << " factor=" << a_expectedFactor
		          << ", got available=" << result.available
		          << " hands=" << result.gripHands
		          << " factor=" << result.gripFactor << '\n';
		return false;
	}

	bool ExpectUnavailable(
		const std::string_view a_name,
		const Type a_type,
		const GripConfigurationEvaluation::UnavailableReason a_expectedReason)
	{
		const auto result = GripConfigurationEvaluation::Evaluate(a_type);
		if (!result.available && result.reason == a_expectedReason) {
			return true;
		}

		std::cerr << a_name << ": expected unavailable reason="
		          << GripConfigurationEvaluation::ReasonName(a_expectedReason)
		          << ", got available=" << result.available
		          << " reason=" << GripConfigurationEvaluation::ReasonName(result.reason)
		          << '\n';
		return false;
	}
}

int main()
{
	bool passed = true;

	passed &= ExpectAvailable("OneHandDagger", Type::kOneHandDagger, 1, 1.00f);
	passed &= ExpectAvailable("OneHandSword", Type::kOneHandSword, 1, 1.00f);
	passed &= ExpectAvailable("OneHandAxe", Type::kOneHandAxe, 1, 1.00f);
	passed &= ExpectAvailable("OneHandMace", Type::kOneHandMace, 1, 1.00f);
	passed &= ExpectAvailable("TwoHandSword", Type::kTwoHandSword, 2, 1.25f);
	passed &= ExpectAvailable("TwoHandAxe", Type::kTwoHandAxe, 2, 1.25f);
	passed &= ExpectUnavailable(
		"HandToHand",
		Type::kHandToHand,
		GripConfigurationEvaluation::UnavailableReason::kHandToHand);
	passed &= ExpectUnavailable(
		"Unsupported",
		Type::kUnknown,
		GripConfigurationEvaluation::UnavailableReason::kUnsupportedWeaponType);

	const auto first = GripConfigurationEvaluation::Evaluate(Type::kTwoHandSword);
	const auto second = GripConfigurationEvaluation::Evaluate(Type::kTwoHandSword);
	if (first.available != second.available || first.gripHands != second.gripHands ||
	    first.gripFactor != second.gripFactor || first.reason != second.reason) {
		std::cerr << "determinism: identical weapon types produced different results\n";
		passed = false;
	}

	return passed ? 0 : 1;
}
