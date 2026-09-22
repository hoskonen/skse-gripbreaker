#include "DropPhysics.h"

#include <thread>

namespace
{
	using Clock = std::chrono::steady_clock;

	constexpr float kTestImpulseStrength = 100.0f;
	constexpr float kUpwardDirectionComponent = 0.35f;
	constexpr float kMinimumImpulseSquared = 1.0e-6f;
	constexpr std::uint32_t kMaximumAttempts = 3;
	constexpr auto kRetryDelay = std::chrono::milliseconds{ 16 };
	constexpr auto kMaximumReadinessLifetime = std::chrono::milliseconds{ 100 };

	struct VectorComponents
	{
		float x;
		float y;
		float z;
		float w;
	};

	[[nodiscard]] VectorComponents GetComponents(const RE::hkVector4& a_vector)
	{
		alignas(16) float values[4];
		_mm_store_ps(values, a_vector.quad);
		return { values[0], values[1], values[2], values[3] };
	}

	[[nodiscard]] bool IsFiniteNonZeroImpulse(const RE::hkVector4& a_impulse)
	{
		const auto [x, y, z, w] = GetComponents(a_impulse);
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w) &&
			(x * x + y * y + z * z) > kMinimumImpulseSquared;
	}

	[[nodiscard]] bool IsDynamicMotion(const RE::hkpMotion::MotionType a_motionType)
	{
		using MotionType = RE::hkpMotion::MotionType;
		switch (a_motionType) {
		case MotionType::kDynamic:
		case MotionType::kSphereInertia:
		case MotionType::kBoxInertia:
		case MotionType::kThinBoxInertia:
			return true;
		default:
			return false;
		}
	}

	[[nodiscard]] std::int64_t GetElapsedMilliseconds(const Clock::time_point a_startedAt)
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - a_startedAt).count();
	}

	void LogAbandoned(
		const std::uint32_t a_attempt,
		const Clock::time_point a_startedAt,
		std::string_view a_reason)
	{
		logs::info(
			"Drop physics: attempt={} elapsed={}ms result=ABANDONED reason={}",
			a_attempt,
			GetElapsedMilliseconds(a_startedAt),
			a_reason);
	}

	void QueueAttempt(
		RE::ObjectRefHandle a_droppedHandle,
		RE::FormID a_expectedWeaponFormID,
		RE::hkVector4 a_impulse,
		std::uint32_t a_attempt,
		Clock::time_point a_startedAt);

	void QueueAttemptAfterDelay(
		RE::ObjectRefHandle a_droppedHandle,
		const RE::FormID a_expectedWeaponFormID,
		RE::hkVector4 a_impulse,
		const std::uint32_t a_attempt,
		const Clock::time_point a_startedAt)
	{
		// Match the essential scheduling behavior of Seb's readiness helpers: wait
		// off the game thread, then submit a fresh one-shot game-thread task. The
		// detached worker owns no engine pointers and disposes itself after enqueue.
		std::jthread(
			[a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt]() {
				std::this_thread::sleep_for(kRetryDelay);
				QueueAttempt(
					a_droppedHandle,
					a_expectedWeaponFormID,
					a_impulse,
					a_attempt,
					a_startedAt);
			})
			.detach();
	}

	void WaitOrAbandon(
		RE::ObjectRefHandle a_droppedHandle,
		const RE::FormID a_expectedWeaponFormID,
		RE::hkVector4 a_impulse,
		const std::uint32_t a_attempt,
		const Clock::time_point a_startedAt,
		std::string_view a_reason)
	{
		if (a_attempt >= kMaximumAttempts) {
			LogAbandoned(a_attempt, a_startedAt, a_reason);
			return;
		}

		logs::info(
			"Drop physics: attempt={} elapsed={}ms result=WAIT reason={}",
			a_attempt,
			GetElapsedMilliseconds(a_startedAt),
			a_reason);
		QueueAttemptAfterDelay(
			a_droppedHandle,
			a_expectedWeaponFormID,
			a_impulse,
			a_attempt + 1,
			a_startedAt);
	}

	void QueueAttempt(
		RE::ObjectRefHandle a_droppedHandle,
		const RE::FormID a_expectedWeaponFormID,
		RE::hkVector4 a_impulse,
		const std::uint32_t a_attempt,
		const Clock::time_point a_startedAt)
	{
		auto* taskInterface = SKSE::GetTaskInterface();
		if (!taskInterface) {
			LogAbandoned(a_attempt, a_startedAt, "task_interface_unavailable");
			return;
		}

		// Each task retains only the stable reference handle, expected base FormID,
		// copied impulse, and bounded readiness timing/counter state. All engine
		// pointers are task-local.
		taskInterface->AddTask(
			[a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt]() {
				if (!IsFiniteNonZeroImpulse(a_impulse)) {
					LogAbandoned(a_attempt, a_startedAt, "invalid_impulse");
					return;
				}
				if (Clock::now() - a_startedAt > kMaximumReadinessLifetime) {
					LogAbandoned(a_attempt, a_startedAt, "timeout");
					return;
				}

				// Resolve the handle afresh on every attempt. No reference, scene graph,
				// collision, or Havok pointer survives this task invocation.
				const auto droppedReference = a_droppedHandle.get();
				if (!droppedReference) {
					LogAbandoned(a_attempt, a_startedAt, "handle_expired");
					return;
				}

				auto* baseObject = droppedReference->GetBaseObject();
				if (!baseObject || baseObject->GetFormID() != a_expectedWeaponFormID ||
					!baseObject->As<RE::TESObjectWEAP>()) {
					LogAbandoned(a_attempt, a_startedAt, "weapon_mismatch");
					return;
				}

				if (!droppedReference->Is3DLoaded()) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "no_3d");
					return;
				}

				auto* object3D = droppedReference->Get3D();
				if (!object3D) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "no_3d");
					return;
				}

				auto* collisionObject = object3D->GetCollisionObject();
				const RE::NiPointer<RE::bhkRigidBody> bhkRigidBody{
					collisionObject ? collisionObject->GetRigidBody() : nullptr
				};
				auto* hkpRigidBody = bhkRigidBody ? bhkRigidBody->GetRigidBody() : nullptr;
				if (!hkpRigidBody) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "no_rigid_body");
					return;
				}
				if (!hkpRigidBody->world) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "body_not_in_world");
					return;
				}

				const auto mass = hkpRigidBody->motion.GetMass();
				if (!std::isfinite(mass) || mass <= 0.0f) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "invalid_mass");
					return;
				}
				if (!IsDynamicMotion(hkpRigidBody->motion.type.get())) {
					WaitOrAbandon(
						a_droppedHandle, a_expectedWeaponFormID, a_impulse, a_attempt, a_startedAt, "body_not_dynamic");
					return;
				}

				hkpRigidBody->ApplyLinearImpulse(a_impulse);

				const auto [x, y, z, w] = GetComponents(a_impulse);
				(void)w;
				const auto impulseMagnitude = std::sqrt(x * x + y * y + z * z);
				logs::info(
					"Drop physics: result=APPLIED mass={:.3f} impulseMagnitude={:.3f}",
					mass,
					impulseMagnitude);
			});
	}
}

RE::hkVector4 DropPhysics::MakeTestImpulse(const float a_playerHeadingRadians)
{
	// Skyrim heading zero faces +Y. Normalize forward + a small upward component,
	// then apply one fixed prototype magnitude without consulting weapon weight.
	const float normalization =
		1.0f / std::sqrt(1.0f + kUpwardDirectionComponent * kUpwardDirectionComponent);
	const auto scale = kTestImpulseStrength * normalization;
	return RE::hkVector4{
		std::sin(a_playerHeadingRadians) * scale,
		std::cos(a_playerHeadingRadians) * scale,
		kUpwardDirectionComponent * scale,
		0.0f
	};
}

void DropPhysics::Queue(
	RE::ObjectRefHandle a_droppedHandle,
	const RE::FormID a_expectedWeaponFormID,
	RE::hkVector4 a_impulse)
{
	QueueAttempt(a_droppedHandle, a_expectedWeaponFormID, a_impulse, 1, Clock::now());
}
