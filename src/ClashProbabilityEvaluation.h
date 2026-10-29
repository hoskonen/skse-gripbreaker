#pragma once

#include <optional>
#include <string_view>

namespace ClashProbabilityEvaluation
{
	enum class UnavailableReason
	{
		kNone,
		kSeverityUnavailable,
		kSeverityNotFinite,
		kSeverityNegative
	};

	struct Result
	{
		bool available{ false };
		float dropChancePercent{ 0.0f };
		UnavailableReason reason{ UnavailableReason::kNone };
	};

	// Canonical curve. These are internal tuning constants, not settings.
	struct Curve
	{
		static constexpr double kMaximumChancePercent = 95.0;
		static constexpr double kHalfMaximumSeverity = 1.85;
		static constexpr double kExponent = 2.8;
	};

	// Value-only estimate; never rolls or dispatches gameplay actions.
	[[nodiscard]] Result Evaluate(std::optional<float> a_severity) noexcept;
	[[nodiscard]] std::string_view ReasonName(UnavailableReason a_reason) noexcept;
}
