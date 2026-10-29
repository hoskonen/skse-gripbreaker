#pragma once

#include <optional>
#include <string_view>

namespace ClashDisarmDecision
{
	enum class Outcome
	{
		kSkipped,
		kKeep,
		kDrop
	};

	struct Result
	{
		Outcome outcome{ Outcome::kSkipped };
		std::optional<float> roll;
		std::string_view reason;
	};

	[[nodiscard]] bool CanRoll(std::optional<float> a_chancePercent) noexcept;
	[[nodiscard]] Result Evaluate(std::optional<float> a_chancePercent, float a_roll) noexcept;

	// Inject the RNG so unavailable probabilities provably never consume a roll.
	template <class RollGenerator>
	[[nodiscard]] Result Roll(std::optional<float> a_chancePercent, RollGenerator&& a_generate)
	{
		if (!CanRoll(a_chancePercent)) {
			return Evaluate(a_chancePercent, 0.0f);
		}
		return Evaluate(a_chancePercent, a_generate());
	}
}
