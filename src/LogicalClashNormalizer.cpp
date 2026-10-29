#include "LogicalClashNormalizer.h"

#include "ClashCombatContext.h"
#include "ClashDisarm.h"
#include "ClashProbabilityEvaluation.h"
#include "ClashSeverityEvaluation.h"
#include "GripConfigurationEvaluation.h"
#include "MassMatchupEvaluation.h"
#include "SkillGripEvaluation.h"
#include "StaminaGripEvaluation.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace
{
	using Clock = std::chrono::steady_clock;
	using Hand = LogicalClashNormalizer::Hand;
	using Participant = LogicalClashNormalizer::Participant;
	using CollisionSample = LogicalClashNormalizer::CollisionSample;

	constexpr auto kClashWindow = std::chrono::milliseconds{ 75 };

	struct ParticipantKey
	{
		std::uint32_t actorHandle{ 0 };

		auto operator<=>(const ParticipantKey&) const = default;
	};

	struct ClashKey
	{
		ParticipantKey first;
		ParticipantKey second;

		auto operator<=>(const ClashKey&) const = default;
	};

	struct ClashAggregate
	{
		std::uint64_t burstID{ 0 };
		Participant attacker;
		Participant target;
		RE::NiPoint3 firstPosition;
		RE::NiPoint3 strongestPosition;
		RE::NiPoint3 strongestVelocity;
		float maximumVelocitySquared{ 0.0f };
		std::uint32_t collisionCount{ 0 };
		bool logDiagnostics{ false };
		bool requestClashDisarm{ false };
		Clock::time_point firstContact;
		Clock::time_point lastContact;
		std::optional<ClashCombatContext::Snapshot> snapshot;
	};

	struct SnapshotRequest
	{
		std::uint64_t burstID{ 0 };
		Participant attacker;
		Participant target;
	};

	ParticipantKey MakeParticipantKey(const Participant& a_participant)
	{
		return { a_participant.actorHandle.native_handle() };
	}

	ClashKey MakeClashKey(const CollisionSample& a_sample)
	{
		auto first = MakeParticipantKey(a_sample.attacker);
		auto second = MakeParticipantKey(a_sample.target);
		if (second < first) {
			std::swap(first, second);
		}
		return { first, second };
	}

	float VelocitySquared(const RE::NiPoint3& a_velocity)
	{
		return a_velocity.x * a_velocity.x +
		       a_velocity.y * a_velocity.y +
		       a_velocity.z * a_velocity.z;
	}

	std::int64_t TimestampMilliseconds(Clock::time_point a_time)
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(a_time.time_since_epoch()).count();
	}

	std::string_view HandName(Hand a_hand)
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

	std::string OptionalFloat(std::optional<float> a_value, std::string_view a_suffix = {})
	{
		return a_value ? std::format("{:.1f}{}", *a_value, a_suffix) : "Unavailable";
	}

	ClashAggregate MakeAggregate(
		std::uint64_t a_burstID,
		CollisionSample a_sample)
	{
		const auto velocitySquared = VelocitySquared(a_sample.hitPointVelocity);
		return {
			a_burstID,
			std::move(a_sample.attacker),
			std::move(a_sample.target),
			a_sample.position,
			a_sample.position,
			a_sample.hitPointVelocity,
			velocitySquared,
			1,
			a_sample.logDiagnostics,
			a_sample.requestClashDisarm,
			a_sample.collisionTime,
			a_sample.collisionTime,
			std::nullopt
		};
	}

	void AddSample(ClashAggregate& a_aggregate, const CollisionSample& a_sample)
	{
		++a_aggregate.collisionCount;
		a_aggregate.lastContact = a_sample.collisionTime;
		a_aggregate.logDiagnostics = a_aggregate.logDiagnostics || a_sample.logDiagnostics;
		a_aggregate.requestClashDisarm =
			a_aggregate.requestClashDisarm || a_sample.requestClashDisarm;

		const auto velocitySquared = VelocitySquared(a_sample.hitPointVelocity);
		if (velocitySquared > a_aggregate.maximumVelocitySquared) {
			a_aggregate.maximumVelocitySquared = velocitySquared;
			a_aggregate.strongestPosition = a_sample.position;
			a_aggregate.strongestVelocity = a_sample.hitPointVelocity;
		}
	}

	void MergeSnapshot(ClashAggregate& a_aggregate, ClashCombatContext::Snapshot a_snapshot)
	{
		a_aggregate.snapshot = std::move(a_snapshot);
	}

	void LogPlayerClashContext(const ClashAggregate& a_aggregate)
	{
		if (!a_aggregate.snapshot || !a_aggregate.snapshot->playerInvolved) {
			return;
		}

		const auto& snapshot = *a_aggregate.snapshot;
		const auto& player = snapshot.attacker.isPlayer ? snapshot.attacker : snapshot.target;
		const auto& opponent = snapshot.attacker.isPlayer ? snapshot.target : snapshot.attacker;
		const auto snapshotDelay =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				snapshot.capturedAt - a_aggregate.firstContact)
				.count();

		logs::info(
			"Logical clash context: collisionTimestampMs={} snapshotDelayMs={} collisions={} "
			"contactPosition=({:.3f},{:.3f},{:.3f}) maxVelocityMagnitude={:.3f} "
			"strongestVelocity=({:.3f},{:.3f},{:.3f}) "
			"playerRole={} player=\"{}\" playerFormID={:08X} playerHandle=0x{:08X} "
			"playerHand={} playerClassification={} playerWeapon=\"{}\" playerWeaponFormID={:08X} "
			"playerWeaponType={} playerWeaponWeight={} playerStamina={} playerSkill={} playerSkillValue={} "
			"opponentRole={} opponent=\"{}\" opponentFormID={:08X} opponentHandle=0x{:08X} "
			"opponentHand={} opponentClassification={} opponentWeapon=\"{}\" opponentWeaponFormID={:08X} "
			"opponentWeaponType={} opponentWeaponWeight={} opponentStamina={} opponentSkill={} opponentSkillValue={}",
			TimestampMilliseconds(a_aggregate.firstContact),
			snapshotDelay,
			a_aggregate.collisionCount,
			a_aggregate.strongestPosition.x,
			a_aggregate.strongestPosition.y,
			a_aggregate.strongestPosition.z,
			std::sqrt(a_aggregate.maximumVelocitySquared),
			a_aggregate.strongestVelocity.x,
			a_aggregate.strongestVelocity.y,
			a_aggregate.strongestVelocity.z,
			player.role,
			player.actorName,
			player.actorFormID,
			player.actorHandle,
			HandName(player.attackHand),
			player.attackClassification,
			player.weaponName,
			player.weaponFormID,
			WeaponClassification::Name(player.weaponType),
			OptionalFloat(player.weaponWeight),
			OptionalFloat(player.staminaPercent, "%"),
			player.skillName,
			OptionalFloat(player.skillValue),
			opponent.role,
			opponent.actorName,
			opponent.actorFormID,
			opponent.actorHandle,
			HandName(opponent.attackHand),
			opponent.attackClassification,
			opponent.weaponName,
			opponent.weaponFormID,
			WeaponClassification::Name(opponent.weaponType),
			OptionalFloat(opponent.weaponWeight),
			OptionalFloat(opponent.staminaPercent, "%"),
			opponent.skillName,
			OptionalFloat(opponent.skillValue));
	}

	void LogMassMatchupEvaluation(const ClashCombatContext::Snapshot& a_snapshot)
	{
		const auto& player = a_snapshot.attacker.isPlayer ?
			a_snapshot.attacker : a_snapshot.target;
		const auto& opponent = a_snapshot.attacker.isPlayer ?
			a_snapshot.target : a_snapshot.attacker;
		const auto evaluation = MassMatchupEvaluation::Evaluate({
			player.available,
			opponent.available,
			player.supportedMeleeWeapon,
			opponent.supportedMeleeWeapon,
			player.weaponWeight,
			opponent.weaponWeight
		});

		if (!evaluation.available) {
			logs::info(
				"Clash mass evaluation: playerWeapon=\"{}\" playerWeight={} "
				"opponentWeapon=\"{}\" opponentWeight={} result=Unavailable reason={}",
				player.weaponName,
				OptionalFloat(player.weaponWeight),
				opponent.weaponName,
				OptionalFloat(opponent.weaponWeight),
				MassMatchupEvaluation::ReasonName(evaluation.reason));
			return;
		}

		logs::info(
			"Clash mass evaluation: playerWeapon=\"{}\" playerWeight={:.1f} "
			"opponentWeapon=\"{}\" opponentWeight={:.1f} massAdvantage={:+.3f} category={}",
			player.weaponName,
			evaluation.playerWeight,
			opponent.weaponName,
			evaluation.opponentWeight,
			evaluation.massAdvantage,
			MassMatchupEvaluation::CategoryName(evaluation.category));
	}

	std::string FormatGripEvaluation(
		const std::string_view a_side,
		const ClashCombatContext::Combatant& a_combatant)
	{
		const auto evaluation = GripConfigurationEvaluation::Evaluate(a_combatant.weaponType);
		if (!evaluation.available) {
			return std::format(
				"{}Weapon=\"{}\" {}Result=Unavailable {}Reason={}",
				a_side,
				a_combatant.weaponName,
				a_side,
				a_side,
				GripConfigurationEvaluation::ReasonName(evaluation.reason));
		}

		return std::format(
			"{}Weapon=\"{}\" {}GripHands={} {}GripFactor={:.3f}",
			a_side,
			a_combatant.weaponName,
			a_side,
			evaluation.gripHands,
			a_side,
			evaluation.gripFactor);
	}

	void LogGripConfigurationEvaluation(const ClashCombatContext::Snapshot& a_snapshot)
	{
		const auto& player = a_snapshot.attacker.isPlayer ?
			a_snapshot.attacker : a_snapshot.target;
		const auto& opponent = a_snapshot.attacker.isPlayer ?
			a_snapshot.target : a_snapshot.attacker;

		logs::info(
			"Clash grip evaluation: {} {}",
			FormatGripEvaluation("player", player),
			FormatGripEvaluation("opponent", opponent));
	}

	std::string FormatStaminaGripEvaluation(
		const std::string_view a_side,
		const ClashCombatContext::Combatant& a_combatant,
		const int a_staminaInfluence)
	{
		const auto grip = GripConfigurationEvaluation::Evaluate(a_combatant.weaponType);
		if (!grip.available) {
			return std::format(
				"{}Weapon=\"{}\" {}Result=Unavailable {}Reason={}",
				a_side,
				a_combatant.weaponName,
				a_side,
				a_side,
				GripConfigurationEvaluation::ReasonName(grip.reason));
		}
		if (!a_combatant.staminaPercent) {
			return std::format(
				"{}Weapon=\"{}\" {}Result=Unavailable {}Reason=stamina_unavailable",
				a_side,
				a_combatant.weaponName,
				a_side,
				a_side);
		}

		const auto evaluation = StaminaGripEvaluation::Evaluate({
			*a_combatant.staminaPercent / 100.0f,
			static_cast<float>(a_staminaInfluence),
			grip.gripFactor
		});
		if (!evaluation.available) {
			return std::format(
				"{}Weapon=\"{}\" {}Result=Unavailable {}Reason={}",
				a_side,
				a_combatant.weaponName,
				a_side,
				a_side,
				StaminaGripEvaluation::ReasonName(evaluation.reason));
		}

		return std::format(
			"{}Stamina={:.1f}% {}BaseGrip={:.3f} {}StaminaFactor={:.3f} {}EffectiveGrip={:.3f}",
			a_side,
			*a_combatant.staminaPercent,
			a_side,
			grip.gripFactor,
			a_side,
			evaluation.staminaFactor,
			a_side,
			evaluation.effectiveGrip);
	}

	void LogStaminaGripEvaluation(const ClashCombatContext::Snapshot& a_snapshot)
	{
		const auto& player = a_snapshot.attacker.isPlayer ?
			a_snapshot.attacker : a_snapshot.target;
		const auto& opponent = a_snapshot.attacker.isPlayer ?
			a_snapshot.target : a_snapshot.attacker;

		logs::info(
			"Clash stamina grip: {} {} staminaInfluence={}",
			FormatStaminaGripEvaluation("player", player, a_snapshot.staminaInfluence),
			FormatStaminaGripEvaluation("opponent", opponent, a_snapshot.staminaInfluence),
			a_snapshot.staminaInfluence);
	}

	std::string FormatSkillGripEvaluation(
		const std::string_view a_side,
		const ClashCombatContext::Combatant& a_combatant,
		const int a_staminaInfluence,
		const int a_skillInfluence)
	{
		const auto grip = GripConfigurationEvaluation::Evaluate(a_combatant.weaponType);
		if (!grip.available) {
			return std::format(
				"{}Result=Unavailable {}Reason={}",
				a_side,
				a_side,
				GripConfigurationEvaluation::ReasonName(grip.reason));
		}
		if (!a_combatant.staminaPercent) {
			return std::format(
				"{}Result=Unavailable {}Reason=stamina_unavailable",
				a_side,
				a_side);
		}
		if (!a_combatant.weaponSkillValue) {
			return std::format(
				"{}Skill={} {}Result=Unavailable {}Reason=weapon_skill_unavailable",
				a_side,
				a_combatant.weaponSkillName,
				a_side,
				a_side);
		}

		const auto stamina = StaminaGripEvaluation::Evaluate({
			*a_combatant.staminaPercent / 100.0f,
			static_cast<float>(a_staminaInfluence),
			grip.gripFactor
		});
		const auto skill = SkillGripEvaluation::Evaluate({
			*a_combatant.weaponSkillValue,
			static_cast<float>(a_skillInfluence)
		});
		if (!stamina.available) {
			return std::format(
				"{}Skill={} {}SkillValue={:.1f} {}Result=Unavailable {}Reason={}",
				a_side,
				a_combatant.weaponSkillName,
				a_side,
				*a_combatant.weaponSkillValue,
				a_side,
				a_side,
				StaminaGripEvaluation::ReasonName(stamina.reason));
		}
		if (!skill.available) {
			return std::format(
				"{}Skill={} {}SkillValue={:.1f} {}Result=Unavailable {}Reason={}",
				a_side,
				a_combatant.weaponSkillName,
				a_side,
				*a_combatant.weaponSkillValue,
				a_side,
				a_side,
				SkillGripEvaluation::ReasonName(skill.reason));
		}

		const auto effectiveGrip = stamina.effectiveGrip * skill.skillFactor;
		return std::format(
			"{}Skill={} {}SkillValue={:.1f} {}SkillFactor={:.3f} {}EffectiveGrip={:.3f}",
			a_side,
			a_combatant.weaponSkillName,
			a_side,
			*a_combatant.weaponSkillValue,
			a_side,
			skill.skillFactor,
			a_side,
			effectiveGrip);
	}

	void LogSkillGripEvaluation(const ClashCombatContext::Snapshot& a_snapshot)
	{
		const auto& player = a_snapshot.attacker.isPlayer ?
			a_snapshot.attacker : a_snapshot.target;
		const auto& opponent = a_snapshot.attacker.isPlayer ?
			a_snapshot.target : a_snapshot.attacker;

		logs::info(
			"Clash skill grip: {} {} skillInfluence={}",
			FormatSkillGripEvaluation(
				"player", player, a_snapshot.staminaInfluence, a_snapshot.skillInfluence),
			FormatSkillGripEvaluation(
				"opponent", opponent, a_snapshot.staminaInfluence, a_snapshot.skillInfluence),
			a_snapshot.skillInfluence);
	}

	std::optional<float> EvaluateEffectiveGrip(
		const ClashCombatContext::Combatant& a_combatant,
		const int a_staminaInfluence,
		const int a_skillInfluence)
	{
		const auto grip = GripConfigurationEvaluation::Evaluate(a_combatant.weaponType);
		if (!grip.available || !a_combatant.staminaPercent || !a_combatant.weaponSkillValue) {
			return std::nullopt;
		}

		const auto stamina = StaminaGripEvaluation::Evaluate({
			*a_combatant.staminaPercent / 100.0f,
			static_cast<float>(a_staminaInfluence),
			grip.gripFactor
		});
		const auto skill = SkillGripEvaluation::Evaluate({
			*a_combatant.weaponSkillValue,
			static_cast<float>(a_skillInfluence)
		});
		if (!stamina.available || !skill.available) {
			return std::nullopt;
		}

		const auto effectiveGrip = stamina.effectiveGrip * skill.skillFactor;
		return std::isfinite(effectiveGrip) && effectiveGrip > 0.0f ?
			std::optional<float>{ effectiveGrip } : std::nullopt;
	}

	std::string FormatSeverityEvaluation(
		const std::string_view a_side,
		const ClashSeverityEvaluation::Result& a_evaluation)
	{
		if (!a_evaluation.available) {
			return std::format(
				"{}Result=Unavailable {}Reason={}",
				a_side,
				a_side,
				ClashSeverityEvaluation::ReasonName(a_evaluation.reason));
		}

		return std::format(
			"{}IncomingMassPressure={:.3f} {}EffectiveGrip={:.3f} "
			"{}Severity={:.3f} {}Category={}",
			a_side,
			a_evaluation.incomingMassPressure,
			a_side,
			a_evaluation.effectiveGrip,
			a_side,
			a_evaluation.severity,
			a_side,
			ClashSeverityEvaluation::CategoryName(a_evaluation.category));
	}

	std::string FormatProbabilityEvaluation(
		const std::string_view a_side,
		const ClashSeverityEvaluation::Result& a_severity,
		const ClashProbabilityEvaluation::Result& a_probability)
	{
		if (!a_probability.available) {
			return std::format(
				"{}Result=Unavailable {}Reason={}",
				a_side,
				a_side,
				a_severity.available ? ClashProbabilityEvaluation::ReasonName(a_probability.reason) :
				                       ClashSeverityEvaluation::ReasonName(a_severity.reason));
		}
		return std::format(
			"{}Severity={:.3f} {}EstimatedDropChance={:.1f}%",
			a_side,
			a_severity.severity,
			a_side,
			a_probability.dropChancePercent);
	}

	struct DirectionalEvaluation
	{
		ClashSeverityEvaluation::Result severity;
		ClashProbabilityEvaluation::Result probability;
	};

	struct EvaluatedClash
	{
		DirectionalEvaluation player;
		DirectionalEvaluation opponent;
	};

	EvaluatedClash EvaluateClash(const ClashCombatContext::Snapshot& a_snapshot)
	{
		const auto& player = a_snapshot.attacker.isPlayer ?
			a_snapshot.attacker : a_snapshot.target;
		const auto& opponent = a_snapshot.attacker.isPlayer ?
			a_snapshot.target : a_snapshot.attacker;
		const auto playerEffectiveGrip = EvaluateEffectiveGrip(
			player, a_snapshot.staminaInfluence, a_snapshot.skillInfluence);
		const auto opponentEffectiveGrip = EvaluateEffectiveGrip(
			opponent, a_snapshot.staminaInfluence, a_snapshot.skillInfluence);

		const auto playerEvaluation = ClashSeverityEvaluation::Evaluate({
			player.supportedMeleeWeapon,
			opponent.supportedMeleeWeapon,
			player.weaponWeight,
			opponent.weaponWeight,
			playerEffectiveGrip
		});
		const auto opponentEvaluation = ClashSeverityEvaluation::Evaluate({
			opponent.supportedMeleeWeapon,
			player.supportedMeleeWeapon,
			opponent.weaponWeight,
			player.weaponWeight,
			opponentEffectiveGrip
		});

		return {
			{ playerEvaluation, ClashProbabilityEvaluation::Evaluate(
				playerEvaluation.available ? std::optional{ playerEvaluation.severity } : std::nullopt) },
			{ opponentEvaluation, ClashProbabilityEvaluation::Evaluate(
				opponentEvaluation.available ? std::optional{ opponentEvaluation.severity } : std::nullopt) }
		};
	}

	void LogClashSeverityEvaluation(const EvaluatedClash& a_evaluation)
	{
		logs::info(
			"Clash severity: {} {}",
			FormatSeverityEvaluation("player", a_evaluation.player.severity),
			FormatSeverityEvaluation("opponent", a_evaluation.opponent.severity));

		logs::info(
			"Clash drop estimate: {} {} maxDropChance={:.1f}%",
			FormatProbabilityEvaluation("player", a_evaluation.player.severity, a_evaluation.player.probability),
			FormatProbabilityEvaluation("opponent", a_evaluation.opponent.severity, a_evaluation.opponent.probability),
			ClashProbabilityEvaluation::Curve::kMaximumChancePercent);
	}

	void EmitAggregate(const ClashAggregate& a_aggregate)
	{
		if (!a_aggregate.snapshot || !a_aggregate.snapshot->playerInvolved) {
			return;
		}
		// Evaluate once from the captured values, even with debug logging disabled.
		// Both logs and the player decision receive this same canonical result.
		const auto evaluation = EvaluateClash(*a_aggregate.snapshot);
		if (a_aggregate.logDiagnostics) {
			LogPlayerClashContext(a_aggregate);
			LogMassMatchupEvaluation(*a_aggregate.snapshot);
			LogGripConfigurationEvaluation(*a_aggregate.snapshot);
			LogStaminaGripEvaluation(*a_aggregate.snapshot);
			LogSkillGripEvaluation(*a_aggregate.snapshot);
			LogClashSeverityEvaluation(evaluation);
		}
		if (a_aggregate.requestClashDisarm) {
			ClashDisarm::Queue(
				*a_aggregate.snapshot, evaluation.player.severity, evaluation.player.probability);
		}
	}

	class Normalizer
	{
	public:
		Normalizer() :
			worker_([this](std::stop_token a_stopToken) { Run(a_stopToken); })
		{}

		~Normalizer()
		{
			worker_.request_stop();
			condition_.notify_all();
		}

		void Record(CollisionSample a_sample)
		{
			if (a_sample.collisionTime == Clock::time_point{}) {
				a_sample.collisionTime = Clock::now();
			}

			const auto key = MakeClashKey(a_sample);
			std::optional<SnapshotRequest> snapshotRequest;
			std::vector<ClashAggregate> ready;

			{
				std::lock_guard lock{ mutex_ };
				auto found = activeClashes_.find(key);
				if (found == activeClashes_.end()) {
					auto aggregate = MakeAggregate(nextBurstID_++, std::move(a_sample));
					snapshotRequest = MakeSnapshotRequest(aggregate);
					activeClashes_.emplace(key, std::move(aggregate));
				} else if (a_sample.collisionTime - found->second.lastContact <= kClashWindow) {
					AddSample(found->second, a_sample);
				} else {
					FinalizeLocked(std::move(found->second), ready);
					auto aggregate = MakeAggregate(nextBurstID_++, std::move(a_sample));
					snapshotRequest = MakeSnapshotRequest(aggregate);
					found->second = std::move(aggregate);
				}
			}

			condition_.notify_one();
			for (const auto& aggregate : ready) {
				EmitAggregate(aggregate);
			}
			if (snapshotRequest) {
				QueueSnapshot(std::move(*snapshotRequest));
			}
		}

	private:
		std::optional<SnapshotRequest> MakeSnapshotRequest(const ClashAggregate& a_aggregate) const
		{
			return SnapshotRequest{
				a_aggregate.burstID,
				a_aggregate.attacker,
				a_aggregate.target
			};
		}

		void QueueSnapshot(SnapshotRequest a_request)
		{
			const auto* taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface) {
				ClashCombatContext::Snapshot unavailable;
				unavailable.capturedAt = Clock::now();
				CompleteSnapshot(a_request.burstID, std::move(unavailable));
				return;
			}

			taskInterface->AddTask([this, request = std::move(a_request)]() mutable {
				auto snapshot = ClashCombatContext::Capture(request.attacker, request.target);
				CompleteSnapshot(request.burstID, std::move(snapshot));
			});
		}

		void CompleteSnapshot(std::uint64_t a_burstID, ClashCombatContext::Snapshot a_snapshot)
		{
			std::optional<ClashAggregate> ready;
			{
				std::lock_guard lock{ mutex_ };
				for (auto& [key, aggregate] : activeClashes_) {
					(void)key;
					if (aggregate.burstID == a_burstID) {
						MergeSnapshot(aggregate, std::move(a_snapshot));
						return;
					}
				}

				auto waiting = awaitingSnapshots_.find(a_burstID);
				if (waiting != awaitingSnapshots_.end()) {
					MergeSnapshot(waiting->second, std::move(a_snapshot));
					ready.emplace(std::move(waiting->second));
					awaitingSnapshots_.erase(waiting);
				}
			}

			if (ready) {
				EmitAggregate(*ready);
			}
		}

		void FinalizeLocked(ClashAggregate a_aggregate, std::vector<ClashAggregate>& a_ready)
		{
			if (!a_aggregate.snapshot) {
				awaitingSnapshots_.emplace(a_aggregate.burstID, std::move(a_aggregate));
				return;
			}
			a_ready.emplace_back(std::move(a_aggregate));
		}

		void Run(std::stop_token a_stopToken)
		{
			std::unique_lock lock{ mutex_ };
			while (!a_stopToken.stop_requested()) {
				if (activeClashes_.empty()) {
					condition_.wait(lock, [&] {
						return a_stopToken.stop_requested() || !activeClashes_.empty();
					});
					continue;
				}

				auto nextDeadline = (Clock::time_point::max)();
				for (const auto& [key, aggregate] : activeClashes_) {
					(void)key;
					const auto deadline = aggregate.lastContact + kClashWindow;
					if (deadline < nextDeadline) {
						nextDeadline = deadline;
					}
				}

				condition_.wait_until(lock, nextDeadline);
				if (a_stopToken.stop_requested()) {
					break;
				}

				const auto now = Clock::now();
				std::vector<ClashAggregate> ready;
				for (auto it = activeClashes_.begin(); it != activeClashes_.end();) {
					if (now - it->second.lastContact >= kClashWindow) {
						auto aggregate = std::move(it->second);
						it = activeClashes_.erase(it);
						FinalizeLocked(std::move(aggregate), ready);
					} else {
						++it;
					}
				}

				lock.unlock();
				for (const auto& aggregate : ready) {
					EmitAggregate(aggregate);
				}
				lock.lock();
			}
		}

		std::mutex mutex_;
		std::condition_variable condition_;
		std::map<ClashKey, ClashAggregate> activeClashes_;
		std::map<std::uint64_t, ClashAggregate> awaitingSnapshots_;
		std::uint64_t nextBurstID_{ 1 };
		std::jthread worker_;
	};

	Normalizer& GetNormalizer()
	{
		static Normalizer normalizer;
		return normalizer;
	}
}

void LogicalClashNormalizer::Record(CollisionSample a_sample)
{
	GetNormalizer().Record(std::move(a_sample));
}
