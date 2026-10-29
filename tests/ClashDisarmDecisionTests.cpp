#include "ClashDisarmDecision.h"

#include <array>
#include <iostream>
#include <limits>

int main()
{
	using namespace ClashDisarmDecision;
	bool passed = true;
	struct Case
	{
		float chance;
		float roll;
		Outcome expected;
	};
	constexpr std::array cases{
		Case{ 0.0f, 0.0f, Outcome::kKeep },
		Case{ 0.0f, 99.9f, Outcome::kKeep },
		Case{ 17.4f, 10.0f, Outcome::kDrop },
		Case{ 17.4f, 17.4f, Outcome::kKeep },
		Case{ 17.4f, 20.0f, Outcome::kKeep },
		Case{ 51.4f, 40.0f, Outcome::kDrop },
		Case{ 51.4f, 51.4f, Outcome::kKeep },
		Case{ 51.4f, 60.0f, Outcome::kKeep },
		Case{ 93.2f, 90.0f, Outcome::kDrop },
		Case{ 93.2f, 93.2f, Outcome::kKeep },
		Case{ 93.2f, 95.0f, Outcome::kKeep },
		Case{ 95.0f, 99.999f, Outcome::kKeep }
	};
	for (const auto& test : cases) {
		int rolls = 0;
		const auto result = Roll(test.chance, [&] {
			++rolls;
			return test.roll;
		});
		if (result.outcome != test.expected || rolls != 1 ||
		    result.roll != test.roll || !result.reason.empty()) {
			std::cerr << "decision failed: chance=" << test.chance << " roll=" << test.roll << '\n';
			passed = false;
		}
	}

	// No RNG invocation (and no DROP) for invalid or unavailable probability.
	const std::array<std::optional<float>, 6> invalidProbabilities{
		std::nullopt, -1.0f, 101.0f,
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()
	};
	for (const auto probability : invalidProbabilities) {
		int rolls = 0;
		const auto result = Roll(probability, [&] {
			++rolls;
			return 0.0f;
		});
		if (result.outcome != Outcome::kSkipped || rolls != 0 ||
		    result.roll || result.reason.empty()) {
			std::cerr << "invalid probability consumed a roll or allowed a decision\n";
			passed = false;
		}
	}
	for (const auto roll : { -1.0f, 100.0f, std::numeric_limits<float>::infinity(),
		     std::numeric_limits<float>::quiet_NaN() }) {
		if (Evaluate(95.0f, roll).outcome != Outcome::kSkipped) {
			std::cerr << "invalid roll accepted\n";
			passed = false;
		}
	}
	return passed ? 0 : 1;
}
