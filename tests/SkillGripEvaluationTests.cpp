#include "GripConfigurationEvaluation.h"
#include "SkillGripEvaluation.h"
#include "StaminaGripEvaluation.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
	constexpr std::array<float, 6> kSkillSamples{ 0.0f, 15.0f, 25.0f, 50.0f, 75.0f, 100.0f };
	constexpr std::array<float, 3> kInfluenceSamples{ 0.0f, 50.0f, 100.0f };

	bool IsNear(const float a_actual, const float a_expected, const float a_tolerance = 0.0001f)
	{
		return std::fabs(a_actual - a_expected) <= a_tolerance;
	}

	float CombinedGrip(
		const WeaponClassification::Type a_weaponType,
		const float a_stamina,
		const float a_skill)
	{
		const auto grip = GripConfigurationEvaluation::Evaluate(a_weaponType);
		const auto stamina = StaminaGripEvaluation::Evaluate({ a_stamina, 100.0f, grip.gripFactor });
		const auto skill = SkillGripEvaluation::Evaluate({ a_skill, 100.0f });
		return stamina.effectiveGrip * skill.skillFactor;
	}
}

int main()
{
	bool passed = true;

	for (const auto influence : kInfluenceSamples) {
		float previousFactor = 1.0f;
		for (const auto skillValue : kSkillSamples) {
			const auto result = SkillGripEvaluation::Evaluate({ skillValue, influence });
			if (!result.available || !std::isfinite(result.skillFactor) ||
			    result.skillFactor < 1.0f ||
			    result.skillFactor > SkillGripEvaluation::kMaximumSkillFactor) {
				std::cerr << "invalid factor: skill=" << skillValue
				          << " influence=" << influence
				          << " factor=" << result.skillFactor << '\n';
				passed = false;
			}
			if (result.skillFactor + 0.0001f < previousFactor) {
				std::cerr << "increasing skill reduced grip at influence=" << influence << '\n';
				passed = false;
			}
			if (influence == 0.0f && !IsNear(result.skillFactor, 1.0f)) {
				std::cerr << "zero influence produced a skill bonus\n";
				passed = false;
			}
			previousFactor = result.skillFactor;
		}
	}

	for (const auto skillValue : kSkillSamples) {
		const auto noInfluence = SkillGripEvaluation::Evaluate({ skillValue, 0.0f });
		const auto halfInfluence = SkillGripEvaluation::Evaluate({ skillValue, 50.0f });
		const auto fullInfluence = SkillGripEvaluation::Evaluate({ skillValue, 100.0f });
		if (halfInfluence.skillFactor + 0.0001f < noInfluence.skillFactor ||
		    fullInfluence.skillFactor + 0.0001f < halfInfluence.skillFactor) {
			std::cerr << "increasing influence reduced the bonus at skill=" << skillValue << '\n';
			passed = false;
		}
	}

	passed &= IsNear(
		CombinedGrip(WeaponClassification::Type::kOneHandSword, 1.0f, 10.0f),
		1.0f);
	passed &= IsNear(
		CombinedGrip(WeaponClassification::Type::kOneHandSword, 0.10f, 100.0f),
		0.433f * 1.25f);
	passed &= IsNear(
		CombinedGrip(WeaponClassification::Type::kTwoHandSword, 1.0f, 100.0f),
		1.25f * 1.25f);
	passed &= IsNear(
		CombinedGrip(WeaponClassification::Type::kTwoHandSword, 0.10f, 100.0f),
		1.25f * 0.433f * 1.25f);

	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto infinity = std::numeric_limits<float>::infinity();
	if (SkillGripEvaluation::Evaluate({ nan, 50.0f }).available ||
	    SkillGripEvaluation::Evaluate({ 50.0f, infinity }).available) {
		std::cerr << "invalid/non-finite input was accepted\n";
		passed = false;
	}

	return passed ? 0 : 1;
}
