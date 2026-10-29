#pragma once

#include <string_view>

namespace SkillGripEvaluation
{
	enum class UnavailableReason
	{
		kNone,
		kSkillNotFinite,
		kInfluenceNotFinite,
		kInvalidResult
	};

	struct Input
	{
		float skillValue{ 0.0f };
		float influencePercent{ 0.0f };
	};

	struct Result
	{
		bool available{ false };
		float skillValue{ 0.0f };
		float influence{ 0.0f };
		float fullInfluenceFactor{ 0.0f };
		float skillFactor{ 0.0f };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	inline constexpr float kBaselineSkill = 15.0f;
	inline constexpr float kMaximumSkill = 100.0f;
	inline constexpr float kMaximumSkillFactor = 1.25f;

	[[nodiscard]] Result Evaluate(const Input& a_input) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
