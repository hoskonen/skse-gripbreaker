#pragma once

#include <optional>
#include <string_view>

namespace MassMatchupEvaluation
{
	enum class Category
	{
		kUnavailable,
		kMajorPlayerAdvantage,
		kPlayerAdvantage,
		kEven,
		kOpponentAdvantage,
		kMajorOpponentAdvantage
	};

	enum class UnavailableReason
	{
		kNone,
		kPlayerActorUnavailable,
		kOpponentActorUnavailable,
		kPlayerWeaponUnsupported,
		kOpponentWeaponUnsupported,
		kPlayerWeightUnavailable,
		kOpponentWeightUnavailable,
		kPlayerWeightNotFinite,
		kOpponentWeightNotFinite,
		kPlayerWeightNotPositive,
		kOpponentWeightNotPositive,
		kInvalidDenominator,
		kInvalidResult
	};

	struct Input
	{
		bool playerActorAvailable{ false };
		bool opponentActorAvailable{ false };
		bool playerWeaponSupported{ false };
		bool opponentWeaponSupported{ false };
		std::optional<float> playerWeight;
		std::optional<float> opponentWeight;
	};

	struct Result
	{
		bool available{ false };
		float playerWeight{ 0.0f };
		float opponentWeight{ 0.0f };
		float massAdvantage{ 0.0f };
		Category category{ Category::kUnavailable };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	inline constexpr float kMajorAdvantageThreshold = 0.30f;
	inline constexpr float kEvenThreshold = 0.10f;

	[[nodiscard]] Result Evaluate(const Input& a_input) noexcept;
	[[nodiscard]] std::string_view CategoryName(Category a_category) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
