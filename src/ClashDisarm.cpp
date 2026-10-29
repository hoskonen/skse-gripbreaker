#include "ClashDisarm.h"

#include "ClashDisarmDecision.h"
#include "PlayerDisarm.h"
#include "RandomRoll.h"
#include "Settings.h"

namespace
{
	using Hand = LogicalClashNormalizer::Hand;

	std::string_view HandName(const Hand a_hand)
	{
		switch (a_hand) {
		case Hand::kRight:
			return "right";
		case Hand::kLeft:
			return "left";
		default:
			return "unknown";
		}
	}
}

void ClashDisarm::Queue(
	const ClashCombatContext::Snapshot& a_snapshot,
	const ClashSeverityEvaluation::Result& a_playerSeverity,
	const ClashProbabilityEvaluation::Result& a_playerProbability)
{
	const auto& player = a_snapshot.attacker.isPlayer ? a_snapshot.attacker : a_snapshot.target;
	const auto playerRole = std::string{ player.role };
	const auto hand = player.attackHand;
	const auto weaponFormID = player.weaponFormID;
	const auto weaponName = player.weaponName;
	const auto validActors = a_snapshot.playerInvolved && player.available && player.isPlayer &&
	                         a_snapshot.attacker.available && a_snapshot.target.available;

	auto* taskInterface = SKSE::GetTaskInterface();
	if (!taskInterface) {
		return;
	}

	// A completed logical burst queues this task exactly once. Only copied values
	// cross threads. No RNG work occurs on the normalizer or Precision threads.
	taskInterface->AddTask(
		[playerRole, hand, weaponFormID, weaponName, validActors,
		 playerSeverity = a_playerSeverity, playerProbability = a_playerProbability]() {
			const auto& settings = Settings::Get();
			if (!settings.enabled || !settings.clashDisarms) {
				return;
			}

			if (!validActors || !playerSeverity.available || !playerProbability.available) {
				const auto reason = !validActors ? std::string_view{ "actor_unavailable" } :
					(!playerSeverity.available ? ClashSeverityEvaluation::ReasonName(playerSeverity.reason) :
					                            ClashProbabilityEvaluation::ReasonName(playerProbability.reason));
				logs::info("Clash disarm roll: result=SKIPPED reason={}", reason);
				return;
			}

			if (hand == Hand::kUnknown || weaponFormID == 0) {
				logs::info(
					"Clash disarm roll: result=SKIPPED reason=invalid_clash_weapon");
				return;
			}

			const auto decision = ClashDisarmDecision::Roll(
				playerProbability.dropChancePercent, RandomRoll::Percent);
			if (decision.outcome == ClashDisarmDecision::Outcome::kSkipped) {
				logs::info("Clash disarm roll: result=SKIPPED reason={}", decision.reason);
				return;
			}
			logs::info(
				"Clash disarm roll: playerSeverity={:.3f} chance={:.1f}% roll={:.3f} result={}",
				playerSeverity.severity,
				playerProbability.dropChancePercent,
				*decision.roll,
				decision.outcome == ClashDisarmDecision::Outcome::kDrop ? "DROP" : "KEEP");
			if (decision.outcome != ClashDisarmDecision::Outcome::kDrop) {
				return;
			}

			const PlayerDisarm::Request request{
				weaponFormID,
				hand == Hand::kLeft
			};
			const auto outcome = PlayerDisarm::TryDropAttackingWeaponDetailed(
				request, settings.dropPhysics, false);

			switch (outcome.result) {
			case PlayerDisarm::DropResult::kDropped:
				logs::info(
					"Clash disarm: playerRole={} hand={} weapon=\"{}\" result=DROPPED",
					playerRole,
					HandName(hand),
					outcome.weaponName);
				break;
			case PlayerDisarm::DropResult::kSkipped: {
				const auto reason = outcome.reason == "equipped_weapon_changed" ?
					std::string_view{ "weapon_changed" } : std::string_view{ outcome.reason };
				logs::info(
					"Clash disarm: playerRole={} hand={} weapon=\"{}\" "
					"result=SKIPPED reason={}",
					playerRole,
					HandName(hand),
					weaponName,
					reason.empty() ? "unknown" : reason);
				break;
			}
			case PlayerDisarm::DropResult::kFailed:
				logs::info(
					"Clash disarm: playerRole={} hand={} weapon=\"{}\" "
					"result=FAILED reason={}",
					playerRole,
					HandName(hand),
					weaponName,
					outcome.reason);
				break;
			case PlayerDisarm::DropResult::kUnverified:
				logs::info(
					"Clash disarm: playerRole={} hand={} weapon=\"{}\" "
					"result=UNVERIFIED reason={}",
					playerRole,
					HandName(hand),
					weaponName,
					outcome.reason);
				break;
			}
		});
}
