#include "MassMatchupEvaluation.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	using Category = MassMatchupEvaluation::Category;

	bool IsNear(const float a_actual, const float a_expected, const float a_tolerance = 0.0001f)
	{
		return std::fabs(a_actual - a_expected) <= a_tolerance;
	}

	bool ExpectAvailable(
		const std::string_view a_name,
		const MassMatchupEvaluation::Result& a_result,
		const float a_expectedAdvantage,
		const Category a_expectedCategory)
	{
		if (a_result.available &&
		    IsNear(a_result.massAdvantage, a_expectedAdvantage) &&
		    a_result.category == a_expectedCategory) {
			return true;
		}

		std::cerr << a_name << ": expected advantage " << a_expectedAdvantage
		          << " and category " << MassMatchupEvaluation::CategoryName(a_expectedCategory)
		          << ", got available=" << a_result.available
		          << " advantage=" << a_result.massAdvantage
		          << " category=" << MassMatchupEvaluation::CategoryName(a_result.category)
		          << '\n';
		return false;
	}

	MassMatchupEvaluation::Result Evaluate(const float a_playerWeight, const float a_opponentWeight)
	{
		return MassMatchupEvaluation::Evaluate({
			true,
			true,
			true,
			true,
			a_playerWeight,
			a_opponentWeight
		});
	}
}

int main()
{
	bool passed = true;

	passed &= ExpectAvailable("equal weights", Evaluate(15.0f, 15.0f), 0.0f, Category::kEven);
	passed &= ExpectAvailable("opponent advantage", Evaluate(10.0f, 15.0f), 0.2f, Category::kOpponentAdvantage);
	passed &= ExpectAvailable("player advantage", Evaluate(15.0f, 10.0f), -0.2f, Category::kPlayerAdvantage);
	passed &= ExpectAvailable(
		"major opponent advantage",
		Evaluate(10.0f, 25.0f),
		15.0f / 35.0f,
		Category::kMajorOpponentAdvantage);

	const auto forward = Evaluate(10.0f, 15.0f);
	const auto reversed = Evaluate(15.0f, 10.0f);
	if (!forward.available || !reversed.available ||
	    !IsNear(forward.massAdvantage, -reversed.massAdvantage) ||
	    !IsNear(std::fabs(forward.massAdvantage), std::fabs(reversed.massAdvantage))) {
		std::cerr << "symmetry: swapping weights did not preserve magnitude and flip sign\n";
		passed = false;
	}

	const auto unsupported = MassMatchupEvaluation::Evaluate({
		true, true, true, false, 10.0f, 15.0f
	});
	if (unsupported.available ||
	    unsupported.reason != MassMatchupEvaluation::UnavailableReason::kOpponentWeaponUnsupported) {
		std::cerr << "unsupported weapon validation failed\n";
		passed = false;
	}

	const auto zeroWeight = MassMatchupEvaluation::Evaluate({
		true, true, true, true, 0.0f, 15.0f
	});
	if (zeroWeight.available ||
	    zeroWeight.reason != MassMatchupEvaluation::UnavailableReason::kPlayerWeightNotPositive) {
		std::cerr << "non-positive weight validation failed\n";
		passed = false;
	}

	const auto nonFiniteWeight = MassMatchupEvaluation::Evaluate({
		true,
		true,
		true,
		true,
		std::numeric_limits<float>::infinity(),
		15.0f
	});
	if (nonFiniteWeight.available ||
	    nonFiniteWeight.reason != MassMatchupEvaluation::UnavailableReason::kPlayerWeightNotFinite) {
		std::cerr << "non-finite weight validation failed\n";
		passed = false;
	}

	return passed ? 0 : 1;
}
