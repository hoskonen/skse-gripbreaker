#pragma once

#include "ClashCombatContext.h"
#include "ClashProbabilityEvaluation.h"
#include "ClashSeverityEvaluation.h"

namespace ClashDisarm
{
	// Safe to call from the normalizer worker. The mutation itself is queued to
	// the game thread. One validated player roll gates the existing mutation path.
	void Queue(
		const ClashCombatContext::Snapshot& a_snapshot,
		const ClashSeverityEvaluation::Result& a_playerSeverity,
		const ClashProbabilityEvaluation::Result& a_playerProbability);
}
