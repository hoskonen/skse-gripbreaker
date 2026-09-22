#include "DisarmEvaluation.h"
#include "GripEvaluation.h"

#include <cmath>
#include <iostream>

namespace
{
	bool IsNear(float a_actual, float a_expected, float a_tolerance = 0.0001f)
	{
		return std::fabs(a_actual - a_expected) <= a_tolerance;
	}

	bool ExpectNear(std::string_view a_name, float a_actual, float a_expected)
	{
		if (IsNear(a_actual, a_expected)) {
			return true;
		}

		std::cerr << a_name << ": expected " << a_expected << ", got " << a_actual << '\n';
		return false;
	}
}

int main()
{
	bool passed = true;

	const auto skill20 = GripEvaluation::Evaluate({ 23.1f, GripEvaluation::SkillType::kOneHanded, 20.0f });
	passed &= ExpectNear("skill 20 modifier", skill20.skillModifier, 0.9f);
	passed &= ExpectNear("skill 20 effective threshold", skill20.effectiveStaminaThreshold, 27.0f);
	passed &= ExpectNear("proportional fatigue risk", skill20.fatigueRisk, (27.0f - 23.1f) / 27.0f);

	const auto atThreshold = GripEvaluation::Evaluate({ 27.0f, GripEvaluation::SkillType::kOneHanded, 20.0f });
	passed &= ExpectNear("risk at threshold", atThreshold.fatigueRisk, 0.0f);

	const auto aboveThreshold = GripEvaluation::Evaluate({ 80.0f, GripEvaluation::SkillType::kBlock, 20.0f });
	passed &= ExpectNear("risk above threshold", aboveThreshold.fatigueRisk, 0.0f);

	const auto noStamina = GripEvaluation::Evaluate({ 0.0f, GripEvaluation::SkillType::kTwoHanded, 20.0f });
	passed &= ExpectNear("risk without stamina", noStamina.fatigueRisk, 1.0f);

	const auto maximumSkill = GripEvaluation::Evaluate({ 0.0f, GripEvaluation::SkillType::kOneHanded, 100.0f });
	passed &= ExpectNear("maximum skill modifier", maximumSkill.skillModifier, 0.5f);
	passed &= ExpectNear("maximum skill threshold", maximumSkill.effectiveStaminaThreshold, 15.0f);

	const auto clampedSkill = GripEvaluation::Evaluate({ 0.0f, GripEvaluation::SkillType::kOneHanded, 200.0f });
	passed &= ExpectNear("skill clamp", clampedSkill.skillModifier, 0.5f);

	const auto disarm = DisarmEvaluation::Evaluate(0.745f, 30.0f);
	passed &= ExpectNear("disarm risk curve", disarm.riskCurve, 0.745f * 0.745f);
	passed &= ExpectNear("disarm chance", disarm.dropChancePercent, 30.0f * 0.745f * 0.745f);

	const auto noFatigueDisarm = DisarmEvaluation::Evaluate(0.0f, 30.0f);
	passed &= ExpectNear("zero fatigue disarm chance", noFatigueDisarm.dropChancePercent, 0.0f);

	return passed ? 0 : 1;
}
