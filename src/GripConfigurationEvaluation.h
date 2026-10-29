#pragma once

#include "WeaponClassification.h"

namespace GripConfigurationEvaluation
{
	enum class UnavailableReason
	{
		kNone,
		kWeaponUnavailable,
		kHandToHand,
		kUnsupportedWeaponType
	};

	struct Result
	{
		bool available{ false };
		int gripHands{ 0 };
		float gripFactor{ 0.0f };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	inline constexpr float kOneHandGripFactor = 1.00f;
	inline constexpr float kTwoHandGripFactor = 1.25f;

	[[nodiscard]] Result Evaluate(WeaponClassification::Type a_weaponType) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
