#pragma once

#include <string_view>

namespace GripEvaluation
{
	enum class SkillType
	{
		kUnavailable,
		kOneHanded,
		kTwoHanded,
		kBlock
	};

	struct FormulaConstants
	{
		float baseStaminaThreshold{ 30.0f };
		float minimumSkillValue{ 0.0f };
		float maximumSkillValue{ 100.0f };
		float minimumSkillModifier{ 0.5f };
		float maximumSkillModifier{ 1.0f };
	};

	inline constexpr FormulaConstants kDefaultFormula{};

	struct Input
	{
		float staminaPercent{ 0.0f };
		SkillType skill{ SkillType::kUnavailable };
		float skillValue{ 0.0f };
	};

	struct Result
	{
		float baseStaminaThreshold{ 0.0f };
		float skillModifier{ 1.0f };
		float effectiveStaminaThreshold{ 0.0f };
		float fatigueRisk{ 0.0f };
	};

	[[nodiscard]] float CalculateSkillModifier(
		float a_skillValue,
		const FormulaConstants& a_formula = kDefaultFormula) noexcept;
	[[nodiscard]] float CalculateEffectiveThreshold(
		float a_baseThreshold,
		float a_skillModifier) noexcept;
	[[nodiscard]] float CalculateFatigueRisk(
		float a_staminaPercent,
		float a_effectiveThreshold) noexcept;
	[[nodiscard]] Result Evaluate(
		const Input& a_input,
		const FormulaConstants& a_formula = kDefaultFormula) noexcept;
	[[nodiscard]] std::string_view SkillName(SkillType a_skill) noexcept;
}
