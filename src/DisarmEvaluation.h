#pragma once

namespace DisarmEvaluation
{
	struct Result
	{
		float fatigueRisk{ 0.0f };
		float riskCurve{ 0.0f };
		float baseChancePercent{ 0.0f };
		float dropChancePercent{ 0.0f };
	};

	[[nodiscard]] Result Evaluate(float a_fatigueRisk, float a_baseChancePercent) noexcept;
}
