#pragma once

#include <string_view>

namespace StaminaGripEvaluation
{
	enum class UnavailableReason
	{
		kNone,
		kStaminaNotFinite,
		kInfluenceNotFinite,
		kBaseGripNotFinite,
		kBaseGripNotPositive,
		kInvalidResult
	};

	struct Input
	{
		float normalizedStamina{ 0.0f };
		float influencePercent{ 0.0f };
		float baseGripFactor{ 0.0f };
	};

	struct Result
	{
		bool available{ false };
		float normalizedStamina{ 0.0f };
		float influence{ 0.0f };
		float fullInfluenceFactor{ 0.0f };
		float staminaFactor{ 0.0f };
		float effectiveGrip{ 0.0f };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	inline constexpr float kMaximumStaminaPenalty = 0.70f;
	inline constexpr float kMinimumFullInfluenceFactor = 1.0f - kMaximumStaminaPenalty;

	[[nodiscard]] Result Evaluate(const Input& a_input) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
