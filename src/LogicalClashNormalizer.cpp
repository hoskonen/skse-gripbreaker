#include "LogicalClashNormalizer.h"

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
	constexpr RE::FormID kPlayerFormID = 0x14;

	struct ParticipantKey
	{
		std::uint32_t actorHandle{ 0 };
		RE::FormID actorFormID{ 0 };

		auto operator<=>(const ParticipantKey&) const = default;
	};

	struct ClashKey
	{
		ParticipantKey first;
		ParticipantKey second;

		auto operator<=>(const ClashKey&) const = default;
	};

	struct PlayerSnapshot
	{
		Clock::time_point capturedAt;
		bool available{ false };
		bool playerWasInitialAttacker{ false };
		Participant player;
		Participant opponent;
		std::optional<float> staminaCurrent;
		std::optional<float> staminaMaximum;
		std::optional<float> staminaPercent;
		Hand attackHand{ Hand::kUnknown };
		std::optional<bool> powerAttack;
		std::optional<bool> bash;
		std::optional<bool> blocking;
		std::string playerWeaponType{ "Unavailable" };
		std::optional<float> playerWeaponWeight;
		std::string skillName{ "Unavailable" };
		std::optional<float> skillValue;
		std::string opponentWeaponType{ "Unavailable" };
		std::optional<float> opponentWeaponWeight;
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
		bool playerWasAttacker{ false };
		bool playerWasTarget{ false };
		Clock::time_point firstContact;
		Clock::time_point lastContact;
		std::optional<PlayerSnapshot> playerSnapshot;
	};

	struct SnapshotRequest
	{
		std::uint64_t burstID{ 0 };
		Clock::time_point collisionTime;
		Participant attacker;
		Participant target;
	};

	ParticipantKey MakeParticipantKey(const Participant& a_participant)
	{
		return { a_participant.actorHandle, a_participant.actorFormID };
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

	std::string_view WeaponTypeName(const RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return "Unavailable";
		}

		switch (a_weapon->GetWeaponType()) {
		case RE::WEAPON_TYPE::kHandToHandMelee:
			return "HandToHand";
		case RE::WEAPON_TYPE::kOneHandSword:
			return "OneHandSword";
		case RE::WEAPON_TYPE::kOneHandDagger:
			return "OneHandDagger";
		case RE::WEAPON_TYPE::kOneHandAxe:
			return "OneHandAxe";
		case RE::WEAPON_TYPE::kOneHandMace:
			return "OneHandMace";
		case RE::WEAPON_TYPE::kTwoHandSword:
			return "TwoHandSword";
		case RE::WEAPON_TYPE::kTwoHandAxe:
			return "TwoHandAxe";
		case RE::WEAPON_TYPE::kBow:
			return "Bow";
		case RE::WEAPON_TYPE::kStaff:
			return "Staff";
		case RE::WEAPON_TYPE::kCrossbow:
			return "Crossbow";
		default:
			return "Unknown";
		}
	}

	std::string_view ActorValueName(RE::ActorValue a_actorValue)
	{
		switch (a_actorValue) {
		case RE::ActorValue::kOneHanded:
			return "OneHanded";
		case RE::ActorValue::kTwoHanded:
			return "TwoHanded";
		case RE::ActorValue::kBlock:
			return "Block";
		default:
			return "Unavailable";
		}
	}

	RE::ActorValue RelevantWeaponSkill(const RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return RE::ActorValue::kNone;
		}

		const auto skill = a_weapon->weaponData.skill.get();
		switch (skill) {
		case RE::ActorValue::kOneHanded:
		case RE::ActorValue::kTwoHanded:
		case RE::ActorValue::kBlock:
			return skill;
		default:
			return RE::ActorValue::kNone;
		}
	}

	std::string OptionalFloat(std::optional<float> a_value, std::string_view a_suffix = {})
	{
		return a_value ? std::format("{:.1f}{}", *a_value, a_suffix) : "Unavailable";
	}

	std::string OptionalBool(std::optional<bool> a_value)
	{
		if (!a_value) {
			return "Unavailable";
		}
		return *a_value ? "true" : "false";
	}

	std::optional<float> ReadActorValue(const RE::Actor* a_actor, RE::ActorValue a_actorValue)
	{
		if (!a_actor || a_actorValue == RE::ActorValue::kNone) {
			return std::nullopt;
		}

		const auto* actorValueOwner = a_actor->AsActorValueOwner();
		if (!actorValueOwner) {
			return std::nullopt;
		}

		const auto value = actorValueOwner->GetActorValue(a_actorValue);
		return std::isfinite(value) ? std::optional<float>{ value } : std::nullopt;
	}

	struct ActiveWeapon
	{
		RE::TESObjectWEAP* weapon{ nullptr };
		Hand hand{ Hand::kUnknown };
		RE::NiPointer<RE::BGSAttackData> attackData;
	};

	ActiveWeapon ResolveActiveWeapon(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return {};
		}

		const auto* process = a_actor->GetActorRuntimeData().currentProcess;
		const auto* high = process ? process->high : nullptr;
		RE::NiPointer<RE::BGSAttackData> attackData = high ? high->attackData : nullptr;
		if (!attackData) {
			return {};
		}

		const auto leftHand = attackData->IsLeftAttack();
		auto* equippedForm = a_actor->GetEquippedObject(leftHand);
		return {
			equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr,
			leftHand ? Hand::kLeft : Hand::kRight,
			std::move(attackData)
		};
	}

	void PopulateParticipant(Participant& a_participant, RE::Actor* a_actor, const ActiveWeapon& a_activeWeapon)
	{
		if (!a_actor) {
			return;
		}

		if (const auto* name = a_actor->GetName(); name && name[0] != '\0') {
			a_participant.actorName = name;
		}
		a_participant.actorHandle = a_actor->GetHandle().native_handle();
		a_participant.actorFormID = a_actor->GetFormID();
		a_participant.hand = a_activeWeapon.hand;
		if (a_activeWeapon.weapon) {
			a_participant.weaponFormID = a_activeWeapon.weapon->GetFormID();
			if (const auto* name = a_activeWeapon.weapon->GetName(); name && name[0] != '\0') {
				a_participant.weaponName = name;
			}
		}
	}

	PlayerSnapshot CapturePlayerSnapshot(const SnapshotRequest& a_request)
	{
		PlayerSnapshot snapshot;
		snapshot.capturedAt = Clock::now();
		snapshot.playerWasInitialAttacker = a_request.attacker.actorFormID == kPlayerFormID;
		snapshot.player = snapshot.playerWasInitialAttacker ? a_request.attacker : a_request.target;
		snapshot.opponent = snapshot.playerWasInitialAttacker ? a_request.target : a_request.attacker;

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return snapshot;
		}

		auto* opponent = snapshot.opponent.actorFormID != 0 ?
			RE::TESForm::LookupByID<RE::Actor>(snapshot.opponent.actorFormID) : nullptr;
		const auto playerWeapon = ResolveActiveWeapon(player);
		const auto opponentWeapon = ResolveActiveWeapon(opponent);
		PopulateParticipant(snapshot.player, player, playerWeapon);
		PopulateParticipant(snapshot.opponent, opponent, opponentWeapon);

		snapshot.available = true;
		snapshot.attackHand = playerWeapon.hand;
		if (playerWeapon.attackData) {
			snapshot.powerAttack =
				playerWeapon.attackData->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
			snapshot.bash =
				playerWeapon.attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack);
		}
		snapshot.blocking = player->IsBlocking();

		if (const auto* actorValueOwner = player->AsActorValueOwner()) {
			const auto current = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
			const auto maximum = actorValueOwner->GetPermanentActorValue(RE::ActorValue::kStamina) +
			                     player->GetActorValueModifier(
									 RE::ACTOR_VALUE_MODIFIER::kTemporary,
									 RE::ActorValue::kStamina);
			if (std::isfinite(current)) {
				snapshot.staminaCurrent = current;
			}
			if (std::isfinite(maximum) && maximum > 0.0f) {
				snapshot.staminaMaximum = maximum;
				if (snapshot.staminaCurrent) {
					snapshot.staminaPercent =
						std::clamp((*snapshot.staminaCurrent / maximum) * 100.0f, 0.0f, 100.0f);
				}
			}
		}

		snapshot.playerWeaponType = WeaponTypeName(playerWeapon.weapon);
		if (playerWeapon.weapon) {
			const auto weight = playerWeapon.weapon->GetWeight();
			if (std::isfinite(weight)) {
				snapshot.playerWeaponWeight = weight;
			}
		}

		const auto skill = RelevantWeaponSkill(playerWeapon.weapon);
		snapshot.skillValue = ReadActorValue(player, skill);
		snapshot.skillName = ActorValueName(snapshot.skillValue ? skill : RE::ActorValue::kNone);

		snapshot.opponentWeaponType = WeaponTypeName(opponentWeapon.weapon);
		if (opponentWeapon.weapon) {
			const auto weight = opponentWeapon.weapon->GetWeight();
			if (std::isfinite(weight)) {
				snapshot.opponentWeaponWeight = weight;
			}
		}

		return snapshot;
	}

	ClashAggregate MakeAggregate(
		std::uint64_t a_burstID,
		CollisionSample a_sample)
	{
		const auto velocitySquared = VelocitySquared(a_sample.hitPointVelocity);
		const auto playerWasAttacker = a_sample.attacker.actorFormID == kPlayerFormID;
		const auto playerWasTarget = a_sample.target.actorFormID == kPlayerFormID;
		return {
			a_burstID,
			std::move(a_sample.attacker),
			std::move(a_sample.target),
			a_sample.position,
			a_sample.position,
			a_sample.hitPointVelocity,
			velocitySquared,
			1,
			playerWasAttacker,
			playerWasTarget,
			a_sample.collisionTime,
			a_sample.collisionTime,
			std::nullopt
		};
	}

	void AddSample(ClashAggregate& a_aggregate, const CollisionSample& a_sample)
	{
		++a_aggregate.collisionCount;
		a_aggregate.lastContact = a_sample.collisionTime;
		a_aggregate.playerWasAttacker = a_aggregate.playerWasAttacker ||
		                                      a_sample.attacker.actorFormID == kPlayerFormID;
		a_aggregate.playerWasTarget = a_aggregate.playerWasTarget ||
		                                    a_sample.target.actorFormID == kPlayerFormID;

		const auto velocitySquared = VelocitySquared(a_sample.hitPointVelocity);
		if (velocitySquared > a_aggregate.maximumVelocitySquared) {
			a_aggregate.maximumVelocitySquared = velocitySquared;
			a_aggregate.strongestPosition = a_sample.position;
			a_aggregate.strongestVelocity = a_sample.hitPointVelocity;
		}
	}

	void MergeSnapshot(ClashAggregate& a_aggregate, PlayerSnapshot a_snapshot)
	{
		if (a_snapshot.playerWasInitialAttacker) {
			a_aggregate.attacker = a_snapshot.player;
			a_aggregate.target = a_snapshot.opponent;
		} else {
			a_aggregate.attacker = a_snapshot.opponent;
			a_aggregate.target = a_snapshot.player;
		}
		a_aggregate.playerSnapshot = std::move(a_snapshot);
	}

	void LogAggregate(const ClashAggregate& a_aggregate)
	{
		logs::info(
			"Logical clash: collisionTimestampMs={} attacker=\"{}\" attackerFormID={:08X} attackerHandle=0x{:08X} "
			"attackerWeapon=\"{}\" attackerWeaponFormID={:08X} attackerHand={} "
			"target=\"{}\" targetFormID={:08X} targetHandle=0x{:08X} "
			"targetWeapon=\"{}\" targetWeaponFormID={:08X} targetHand={} collisions={} "
			"firstPosition=({:.3f}, {:.3f}, {:.3f}) strongestPosition=({:.3f}, {:.3f}, {:.3f}) "
			"maxVelocityMagnitude={:.3f} strongestVelocity=({:.3f}, {:.3f}, {:.3f})",
			TimestampMilliseconds(a_aggregate.firstContact),
			a_aggregate.attacker.actorName,
			a_aggregate.attacker.actorFormID,
			a_aggregate.attacker.actorHandle,
			a_aggregate.attacker.weaponName,
			a_aggregate.attacker.weaponFormID,
			HandName(a_aggregate.attacker.hand),
			a_aggregate.target.actorName,
			a_aggregate.target.actorFormID,
			a_aggregate.target.actorHandle,
			a_aggregate.target.weaponName,
			a_aggregate.target.weaponFormID,
			HandName(a_aggregate.target.hand),
			a_aggregate.collisionCount,
			a_aggregate.firstPosition.x,
			a_aggregate.firstPosition.y,
			a_aggregate.firstPosition.z,
			a_aggregate.strongestPosition.x,
			a_aggregate.strongestPosition.y,
			a_aggregate.strongestPosition.z,
			std::sqrt(a_aggregate.maximumVelocitySquared),
			a_aggregate.strongestVelocity.x,
			a_aggregate.strongestVelocity.y,
			a_aggregate.strongestVelocity.z);
	}

	void LogPlayerContext(const ClashAggregate& a_aggregate)
	{
		if (!a_aggregate.playerSnapshot) {
			return;
		}

		const auto& snapshot = *a_aggregate.playerSnapshot;
		const auto snapshotDelay =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				snapshot.capturedAt - a_aggregate.firstContact)
				.count();
		const auto role = a_aggregate.playerWasAttacker && a_aggregate.playerWasTarget ?
			"attacker+target" :
			a_aggregate.playerWasAttacker ? "attacker" : "target";

		logs::info(
			"Player clash context: collisionTimestampMs={} snapshotTimestampMs={} snapshotDelayMs={} "
			"precisionRole={} stamina={}/{} ({}) weaponType={} weaponWeight={} "
			"skill={} skillValue={} attackHand={} powerAttack={} bash={} blocking={} "
			"opponent=\"{}\" opponentFormID={:08X} opponentWeapon=\"{}\" opponentWeaponFormID={:08X} "
			"opponentHand={} opponentWeaponType={} opponentWeaponWeight={}",
			TimestampMilliseconds(a_aggregate.firstContact),
			TimestampMilliseconds(snapshot.capturedAt),
			snapshotDelay,
			role,
			OptionalFloat(snapshot.staminaCurrent),
			OptionalFloat(snapshot.staminaMaximum),
			OptionalFloat(snapshot.staminaPercent, "%"),
			snapshot.playerWeaponType,
			OptionalFloat(snapshot.playerWeaponWeight),
			snapshot.skillName,
			OptionalFloat(snapshot.skillValue),
			HandName(snapshot.attackHand),
			OptionalBool(snapshot.powerAttack),
			OptionalBool(snapshot.bash),
			OptionalBool(snapshot.blocking),
			snapshot.opponent.actorName,
			snapshot.opponent.actorFormID,
			snapshot.opponent.weaponName,
			snapshot.opponent.weaponFormID,
			HandName(snapshot.opponent.hand),
			snapshot.opponentWeaponType,
			OptionalFloat(snapshot.opponentWeaponWeight));
	}

	void EmitAggregate(const ClashAggregate& a_aggregate)
	{
		LogAggregate(a_aggregate);
		if (a_aggregate.playerWasAttacker || a_aggregate.playerWasTarget) {
			LogPlayerContext(a_aggregate);
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
			if (!a_aggregate.playerWasAttacker && !a_aggregate.playerWasTarget) {
				return std::nullopt;
			}
			return SnapshotRequest{
				a_aggregate.burstID,
				a_aggregate.firstContact,
				a_aggregate.attacker,
				a_aggregate.target
			};
		}

		void QueueSnapshot(SnapshotRequest a_request)
		{
			const auto* taskInterface = SKSE::GetTaskInterface();
			if (!taskInterface) {
				PlayerSnapshot unavailable;
				unavailable.capturedAt = Clock::now();
				unavailable.playerWasInitialAttacker =
					a_request.attacker.actorFormID == kPlayerFormID;
				unavailable.player =
					unavailable.playerWasInitialAttacker ? a_request.attacker : a_request.target;
				unavailable.opponent =
					unavailable.playerWasInitialAttacker ? a_request.target : a_request.attacker;
				CompleteSnapshot(a_request.burstID, std::move(unavailable));
				return;
			}

			taskInterface->AddTask([this, request = std::move(a_request)]() mutable {
				auto snapshot = CapturePlayerSnapshot(request);
				CompleteSnapshot(request.burstID, std::move(snapshot));
			});
		}

		void CompleteSnapshot(std::uint64_t a_burstID, PlayerSnapshot a_snapshot)
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
			if ((a_aggregate.playerWasAttacker || a_aggregate.playerWasTarget) &&
			    !a_aggregate.playerSnapshot) {
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
