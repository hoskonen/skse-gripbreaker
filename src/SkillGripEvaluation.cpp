#include "SkillGripEvaluation.h"

#include <algorithm>
#include <cmath>

SkillGripEvaluation::Result SkillGripEvaluation::Evaluate(const Input& a_input) noexcept
{
	Result result;
	if (!std::isfinite(a_input.skillValue)) {
		result.reason = UnavailableReason::kSkillNotFinite;
		return result;
	}
	if (!std::isfinite(a_input.influencePercent)) {
		result.reason = UnavailableReason::kInfluenceNotFinite;
		return result;
	}

	result.skillValue = std::clamp(a_input.skillValue, 0.0f, kMaximumSkill);
	result.influence = std::clamp(a_input.influencePercent / 100.0f, 0.0f, 1.0f);
	const auto skillProgress = std::clamp(
		(result.skillValue - kBaselineSkill) / (kMaximumSkill - kBaselineSkill),
		0.0f,
		1.0f);
	result.fullInfluenceFactor =
		1.0f + (kMaximumSkillFactor - 1.0f) * skillProgress;
	result.skillFactor =
		1.0f + (result.fullInfluenceFactor - 1.0f) * result.influence;
	result.skillFactor = std::clamp(result.skillFactor, 1.0f, kMaximumSkillFactor);

	if (!std::isfinite(result.fullInfluenceFactor) ||
	    !std::isfinite(result.skillFactor)) {
		result.fullInfluenceFactor = 0.0f;
		result.skillFactor = 0.0f;
		result.reason = UnavailableReason::kInvalidResult;
		return result;
	}

	result.available = true;
	return result;
}

std::string_view SkillGripEvaluation::ReasonName(const UnavailableReason a_reason) noexcept
{
	switch (a_reason) {
	case UnavailableReason::kSkillNotFinite:
		return "skill_not_finite";
	case UnavailableReason::kInfluenceNotFinite:
		return "influence_not_finite";
	case UnavailableReason::kInvalidResult:
		return "invalid_result";
	default:
		return "none";
	}
}
