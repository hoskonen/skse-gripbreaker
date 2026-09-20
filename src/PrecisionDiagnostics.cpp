#include "PrecisionDiagnostics.h"

#include "LogicalClashNormalizer.h"
#include "PrecisionAPI.h"
#include "Settings.h"

namespace
{
	constexpr std::string_view kUnavailable = "<unavailable>";

	std::string_view GetReferenceName(const RE::TESObjectREFR* a_reference)
	{
		if (!a_reference) {
			return kUnavailable;
		}

		const auto* name = a_reference->GetName();
		return name && name[0] != '\0' ? std::string_view{ name } : kUnavailable;
	}

	std::string CopyReferenceName(const RE::TESObjectREFR* a_reference)
	{
		return std::string{ GetReferenceName(a_reference) };
	}

	LogicalClashNormalizer::Participant MakeParticipant(RE::Actor* a_actor)
	{
		return {
			a_actor ? a_actor->GetHandle().native_handle() : 0,
			a_actor ? a_actor->GetFormID() : 0,
			0,
			LogicalClashNormalizer::Hand::kUnknown,
			CopyReferenceName(a_actor),
			std::string{ kUnavailable }
		};
	}

	PRECISION_API::WeaponCollisionCallbackReturn OnWeaponWeaponCollision(
		const PRECISION_API::PrecisionHitData& a_hitData)
	{
		// Precision defaults this field to true. Diagnostics must never suppress a collision.
		PRECISION_API::WeaponCollisionCallbackReturn result;
		result.bIgnoreHit = false;

		if (!Settings::Get().debugLogging) {
			return result;
		}

		auto* attacker = a_hitData.attacker;
		auto* target = a_hitData.target;
		auto* targetActor = target ? target->As<RE::Actor>() : nullptr;
		const auto collisionTime = std::chrono::steady_clock::now();
		const auto collisionTimestampMs =
			std::chrono::duration_cast<std::chrono::milliseconds>(collisionTime.time_since_epoch()).count();

		logs::info(
			"Precision weapon collision: collisionTimestampMs={} attacker=\"{}\" attackerFormID={:08X} attackerWeapon=<deferred> "
			"target=\"{}\" targetFormID={:08X} targetWeapon=<deferred> "
			"position=({:.3f}, {:.3f}, {:.3f}) velocity=({:.3f}, {:.3f}, {:.3f}) normal=({:.3f}, {:.3f}, {:.3f}) "
			"hittingBody={} hittingShapeKey=0x{:08X} hitBody={} hitShapeKey=0x{:08X}",
			collisionTimestampMs,
			GetReferenceName(attacker),
			attacker ? attacker->GetFormID() : 0,
			GetReferenceName(target),
			target ? target->GetFormID() : 0,
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

		LogicalClashNormalizer::Record({
			MakeParticipant(attacker),
			MakeParticipant(targetActor),
			a_hitData.hitPos,
			a_hitData.hitPointVelocity,
			collisionTime
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
