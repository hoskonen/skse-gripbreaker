#include "ClashProbabilityEvaluation.h"
#include "ClashSeverityEvaluation.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
	using namespace ClashProbabilityEvaluation;
	bool passed = true;
	struct Target
	{
		float severity;
		float minimum;
		float maximum;
	};
	constexpr std::array targets{
		Target{ 0.5f, 2.0f, 5.0f },
		Target{ 0.75f, 5.0f, 8.0f },
		Target{ 1.0f, 10.0f, 15.0f },
		Target{ 1.25f, 18.0f, 25.0f },
		Target{ 1.5f, 30.0f, 40.0f },
		Target{ 2.0f, 50.0f, 60.0f },
		Target{ 3.0f, 70.0f, 80.0f },
		Target{ 5.0f, 85.0f, 90.0f },
		Target{ 8.0f, 90.0f, 95.0f }
	};
	for (const auto& target : targets) {
		const auto result = Evaluate(target.severity);
		if (!result.available || result.dropChancePercent < target.minimum ||
		    result.dropChancePercent > target.maximum) {
			std::cerr << "target range failed at severity=" << target.severity << '\n';
			passed = false;
		}
	}

	const auto zero = Evaluate(0.0f);
	if (!zero.available || zero.dropChancePercent != 0.0f) {
		std::cerr << "zero severity must produce zero chance\n";
		passed = false;
	}
	float previous = 0.0f;
	for (int i = 0; i <= 10000; ++i) {
		const auto result = Evaluate(static_cast<float>(i) / 100.0f);
		if (!result.available || !std::isfinite(result.dropChancePercent) ||
		    result.dropChancePercent < previous ||
		    result.dropChancePercent > Curve::kMaximumChancePercent ||
		    result.dropChancePercent >= 100.0f) {
			std::cerr << "monotonic/bounded output failed at sample=" << i << '\n';
			passed = false;
			break;
		}
		previous = result.dropChancePercent;
	}
	for (const auto severity : { std::numeric_limits<float>::denorm_min(),
		     std::numeric_limits<float>::min(), std::numeric_limits<float>::max() }) {
		const auto result = Evaluate(severity);
		if (!result.available || !std::isfinite(result.dropChancePercent) ||
		    result.dropChancePercent < 0.0f || result.dropChancePercent > 95.0f) {
			std::cerr << "extreme finite severity produced invalid chance\n";
			passed = false;
		}
	}

	for (const auto severity : { std::numeric_limits<float>::quiet_NaN(),
		     std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() }) {
		const auto result = Evaluate(severity);
		if (result.available || result.reason != UnavailableReason::kSeverityNotFinite) {
			std::cerr << "non-finite severity accepted\n";
			passed = false;
		}
	}
	if (Evaluate(-1.0f).available ||
	    Evaluate(-1.0f).reason != UnavailableReason::kSeverityNegative ||
	    Evaluate(std::nullopt).available ||
	    Evaluate(std::nullopt).reason != UnavailableReason::kSeverityUnavailable) {
		std::cerr << "negative/missing severity validation failed\n";
		passed = false;
	}

	// Existing severity output feeds the estimate directly, without another state read.
	const auto valid = ClashSeverityEvaluation::Evaluate({ true, true, 10.0f, 10.0f, 1.0f });
	const auto invalid = ClashSeverityEvaluation::Evaluate({ false, true, 10.0f, 10.0f, 1.0f });
	const auto validChance = Evaluate(valid.available ? std::optional{ valid.severity } : std::nullopt);
	const auto invalidChance = Evaluate(invalid.available ? std::optional{ invalid.severity } : std::nullopt);
	if (!validChance.available || invalidChance.available) {
		std::cerr << "upstream availability propagation failed\n";
		passed = false;
	}
	return passed ? 0 : 1;
}
