#include "GripEvaluation.h"

#include <algorithm>
#include <cmath>

namespace GripEvaluation
{
	float CalculateSkillModifier(float a_skillValue, const FormulaConstants& a_formula) noexcept
	{
		const auto skillRange = a_formula.maximumSkillValue - a_formula.minimumSkillValue;
		if (!std::isfinite(a_skillValue) || !std::isfinite(skillRange) || skillRange <= 0.0f) {
			return std::clamp(a_formula.maximumSkillModifier, 0.0f, 1.0f);
		}

		const auto normalizedSkill = std::clamp(
			(a_skillValue - a_formula.minimumSkillValue) / skillRange,
			0.0f,
			1.0f);
		const auto modifierRange = a_formula.maximumSkillModifier - a_formula.minimumSkillModifier;
		const auto modifier = a_formula.maximumSkillModifier - normalizedSkill * modifierRange;
		return std::clamp(modifier, 0.0f, 1.0f);
	}

	float CalculateEffectiveThreshold(float a_baseThreshold, float a_skillModifier) noexcept
	{
		if (!std::isfinite(a_baseThreshold) || !std::isfinite(a_skillModifier)) {
			return 0.0f;
		}
		return std::clamp(a_baseThreshold * a_skillModifier, 0.0f, 100.0f);
	}

	float CalculateFatigueRisk(float a_staminaPercent, float a_effectiveThreshold) noexcept
	{
		if (!std::isfinite(a_staminaPercent) || !std::isfinite(a_effectiveThreshold) ||
		    a_effectiveThreshold <= 0.0f) {
			return 0.0f;
		}

		const auto stamina = std::clamp(a_staminaPercent, 0.0f, 100.0f);
		if (stamina >= a_effectiveThreshold) {
			return 0.0f;
		}

		return std::clamp(
			(a_effectiveThreshold - stamina) / a_effectiveThreshold,
			0.0f,
			1.0f);
	}

	Result Evaluate(const Input& a_input, const FormulaConstants& a_formula) noexcept
	{
		const auto baseThreshold = std::isfinite(a_formula.baseStaminaThreshold) ?
			std::clamp(a_formula.baseStaminaThreshold, 0.0f, 100.0f) : 0.0f;
		const auto skillModifier = CalculateSkillModifier(a_input.skillValue, a_formula);
		const auto effectiveThreshold = CalculateEffectiveThreshold(baseThreshold, skillModifier);
		return {
			baseThreshold,
			skillModifier,
			effectiveThreshold,
			CalculateFatigueRisk(a_input.staminaPercent, effectiveThreshold)
		};
	}

	std::string_view SkillName(SkillType a_skill) noexcept
	{
		switch (a_skill) {
		case SkillType::kOneHanded:
			return "OneHanded";
		case SkillType::kTwoHanded:
			return "TwoHanded";
		case SkillType::kBlock:
			return "Block";
		default:
			return "Unavailable";
		}
	}
}
