#include "PrecisionDiagnostics.h"

#include "LogicalClashNormalizer.h"
#include "PrecisionAPI.h"
#include "Settings.h"

namespace
{
	PRECISION_API::WeaponCollisionCallbackReturn OnWeaponWeaponCollision(
		const PRECISION_API::PrecisionHitData& a_hitData)
	{
		// Precision defaults this field to true. Diagnostics must never suppress a collision.
		PRECISION_API::WeaponCollisionCallbackReturn result;
		result.bIgnoreHit = false;

		const auto& settings = Settings::Get();
		const auto logDiagnostics = settings.debugLogging;
		const auto normalizeForClashDisarm = settings.enabled && settings.clashDisarms;
		if (!logDiagnostics && !normalizeForClashDisarm) {
			return result;
		}

		auto* attacker = a_hitData.attacker;
		auto* target = a_hitData.target;
		auto* targetActor = target ? target->As<RE::Actor>() : nullptr;
		const auto collisionTime = std::chrono::steady_clock::now();
		const auto collisionTimestampMs =
			std::chrono::duration_cast<std::chrono::milliseconds>(collisionTime.time_since_epoch()).count();

		const auto attackerHandle = attacker ? attacker->GetHandle() : RE::ActorHandle{};
		const auto targetHandle = targetActor ? targetActor->GetHandle() : RE::ActorHandle{};
		if (logDiagnostics) {
			logs::info(
				"Precision weapon collision: collisionTimestampMs={} attackerHandle=0x{:08X} "
				"targetHandle=0x{:08X} "
				"position=({:.3f}, {:.3f}, {:.3f}) velocity=({:.3f}, {:.3f}, {:.3f}) normal=({:.3f}, {:.3f}, {:.3f}) "
				"hittingBody={} hittingShapeKey=0x{:08X} hitBody={} hitShapeKey=0x{:08X}",
				collisionTimestampMs,
				attackerHandle.native_handle(),
				targetHandle.native_handle(),
				a_hitData.hitPos.x,
				a_hitData.hitPos.y,
				a_hitData.hitPos.z,
				a_hitData.hitPointVelocity.x,
				a_hitData.hitPointVelocity.y,
				a_hitData.hitPointVelocity.z,
				a_hitData.separatingNormal.x,
				a_hitData.separatingNormal.y,
				a_hitData.separatingNormal.z,
				static_cast<const void*>(a_hitData.hittingRigidBody),
				static_cast<std::uint32_t>(a_hitData.hittingBodyShapeKey),
				static_cast<const void*>(a_hitData.hitRigidBody),
				static_cast<std::uint32_t>(a_hitData.hitBodyShapeKey));
		}

		LogicalClashNormalizer::Record({
			{ attackerHandle },
			{ targetHandle },
			a_hitData.hitPos,
			a_hitData.hitPointVelocity,
			collisionTime,
			logDiagnostics,
			normalizeForClashDisarm
		});

		return result;
	}
}

void PrecisionDiagnostics::Register()
{
	auto* precision = static_cast<PRECISION_API::IVPrecision2*>(
		PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V2));
	if (!precision) {
		logs::info("Precision API V2 is unavailable; weapon collision diagnostics are disabled");
		return;
	}

	const auto registration = precision->AddWeaponWeaponCollisionCallback(
		SKSE::GetPluginHandle(), OnWeaponWeaponCollision);
	switch (registration) {
	case PRECISION_API::APIResult::OK:
		logs::info("Registered Precision weapon collision diagnostics (API V2)");
		break;
	case PRECISION_API::APIResult::AlreadyRegistered:
		logs::debug("Precision weapon collision diagnostics are already registered");
		break;
	default:
		logs::warn("Could not register Precision weapon collision diagnostics (result {})",
			static_cast<std::uint32_t>(registration));
		break;
	}
}
