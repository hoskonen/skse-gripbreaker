#include "StaminaGripEvaluation.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	constexpr std::array<float, 6> kStaminaSamples{ 1.0f, 0.75f, 0.50f, 0.25f, 0.10f, 0.0f };
	constexpr std::array<float, 3> kInfluenceSamples{ 0.0f, 50.0f, 100.0f };

	bool IsNear(const float a_actual, const float a_expected, const float a_tolerance = 0.0001f)
	{
		return std::fabs(a_actual - a_expected) <= a_tolerance;
	}

	bool CheckAvailableResult(
		const float a_stamina,
		const float a_influence,
		const float a_baseGrip,
		const StaminaGripEvaluation::Result& a_result)
	{
		if (!a_result.available || !std::isfinite(a_result.staminaFactor) ||
		    !std::isfinite(a_result.effectiveGrip) ||
		    a_result.staminaFactor < StaminaGripEvaluation::kMinimumFullInfluenceFactor ||
		    a_result.staminaFactor > 1.0f ||
		    !IsNear(a_result.effectiveGrip, a_baseGrip * a_result.staminaFactor)) {
			std::cerr << "invalid result: stamina=" << a_stamina
			          << " influence=" << a_influence
			          << " baseGrip=" << a_baseGrip
			          << " factor=" << a_result.staminaFactor
			          << " effectiveGrip=" << a_result.effectiveGrip << '\n';
			return false;
		}
		return true;
	}
}

int main()
{
	bool passed = true;

	for (const auto influence : kInfluenceSamples) {
		float previousFactor = 1.0f;
		for (const auto stamina : kStaminaSamples) {
			const auto oneHand = StaminaGripEvaluation::Evaluate({ stamina, influence, 1.00f });
			const auto twoHand = StaminaGripEvaluation::Evaluate({ stamina, influence, 1.25f });
			passed &= CheckAvailableResult(stamina, influence, 1.00f, oneHand);
			passed &= CheckAvailableResult(stamina, influence, 1.25f, twoHand);

			if (oneHand.staminaFactor > previousFactor + 0.0001f) {
				std::cerr << "lower stamina increased grip at influence=" << influence << '\n';
				passed = false;
			}
			previousFactor = oneHand.staminaFactor;

			if (stamina == 1.0f && !IsNear(oneHand.staminaFactor, 1.0f)) {
				std::cerr << "full stamina did not produce factor 1 at influence=" << influence << '\n';
				passed = false;
			}
			if (influence == 0.0f && !IsNear(oneHand.staminaFactor, 1.0f)) {
				std::cerr << "zero influence did not produce factor 1 at stamina=" << stamina << '\n';
				passed = false;
			}
		}
	}

	for (const auto stamina : kStaminaSamples) {
		const auto noInfluence = StaminaGripEvaluation::Evaluate({ stamina, 0.0f, 1.0f });
		const auto halfInfluence = StaminaGripEvaluation::Evaluate({ stamina, 50.0f, 1.0f });
		const auto fullInfluence = StaminaGripEvaluation::Evaluate({ stamina, 100.0f, 1.0f });
		if (halfInfluence.staminaFactor > noInfluence.staminaFactor + 0.0001f ||
		    fullInfluence.staminaFactor > halfInfluence.staminaFactor + 0.0001f) {
			std::cerr << "increasing influence improved grip at stamina=" << stamina << '\n';
			passed = false;
		}
	}

	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto infinity = std::numeric_limits<float>::infinity();
	const auto invalidStamina = StaminaGripEvaluation::Evaluate({ nan, 50.0f, 1.0f });
	const auto invalidInfluence = StaminaGripEvaluation::Evaluate({ 0.5f, infinity, 1.0f });
	const auto invalidBaseGrip = StaminaGripEvaluation::Evaluate({ 0.5f, 50.0f, nan });
	const auto zeroBaseGrip = StaminaGripEvaluation::Evaluate({ 0.5f, 50.0f, 0.0f });
	if (invalidStamina.available || invalidInfluence.available ||
	    invalidBaseGrip.available || zeroBaseGrip.available) {
		std::cerr << "invalid/non-finite input was accepted\n";
		passed = false;
	}

	return passed ? 0 : 1;
}
