#pragma once

#include <optional>
#include <string_view>

namespace ClashSeverityEvaluation
{
	enum class Category
	{
		kUnavailable,
		kLow,
		kModerate,
		kHigh,
		kSevere
	};

	enum class UnavailableReason
	{
		kNone,
		kOwnWeaponUnsupported,
		kIncomingWeaponUnsupported,
		kOwnWeightUnavailable,
		kIncomingWeightUnavailable,
		kOwnWeightNotFinite,
		kIncomingWeightNotFinite,
		kOwnWeightNotPositive,
		kIncomingWeightNotPositive,
		kEffectiveGripUnavailable,
		kEffectiveGripNotFinite,
		kEffectiveGripNotPositive,
		kInvalidMassPressure,
		kInvalidSeverity
	};

	struct Input
	{
		bool ownWeaponSupported{ false };
		bool incomingWeaponSupported{ false };
		std::optional<float> ownWeaponWeight;
		std::optional<float> incomingWeaponWeight;
		std::optional<float> effectiveGrip;
	};

	struct Result
	{
		bool available{ false };
		float incomingMassPressure{ 0.0f };
		float effectiveGrip{ 0.0f };
		float severity{ 0.0f };
		Category category{ Category::kUnavailable };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	inline constexpr float kModerateThreshold = 0.75f;
	inline constexpr float kHighThreshold = 1.00f;
	inline constexpr float kSevereThreshold = 1.50f;

	[[nodiscard]] Result Evaluate(const Input& a_input) noexcept;
	[[nodiscard]] std::string_view CategoryName(Category a_category) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
