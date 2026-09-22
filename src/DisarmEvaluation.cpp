#include "DisarmEvaluation.h"

#include <algorithm>
#include <cmath>

namespace DisarmEvaluation
{
	Result Evaluate(float a_fatigueRisk, float a_baseChancePercent) noexcept
	{
		const auto fatigueRisk = std::isfinite(a_fatigueRisk) ?
			std::clamp(a_fatigueRisk, 0.0f, 1.0f) : 0.0f;
		const auto baseChance = std::isfinite(a_baseChancePercent) ?
			std::clamp(a_baseChancePercent, 0.0f, 100.0f) : 0.0f;
		const auto riskCurve = fatigueRisk * fatigueRisk;
		return {
			fatigueRisk,
			riskCurve,
			baseChance,
			std::clamp(baseChance * riskCurve, 0.0f, 100.0f)
		};
	}
}
