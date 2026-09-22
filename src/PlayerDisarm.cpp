#include "PlayerDisarm.h"

#include "DropPhysics.h"

namespace
{
	struct Resolution
	{
		RE::PlayerCharacter* player{ nullptr };
		RE::TESObjectWEAP* weapon{ nullptr };
		RE::ExtraDataList* inventoryOwnedExtra{ nullptr };
		std::string weaponName{ "<unavailable>" };
		std::string_view reason{ "unknown" };
		std::size_t inventoryEntryMatchCount{ 0 };
		std::size_t wornMatchCount{ 0 };
		bool inventoryEntryFound{ false };
		bool wornInstanceFound{ false };
		bool valid{ false };
	};

	std::string GetWeaponName(const RE::TESObjectWEAP* a_weapon)
	{
		const auto* name = a_weapon ? a_weapon->GetName() : nullptr;
		return name && name[0] != '\0' ? name : "<unavailable>";
	}

	void LogDroppedReference(
		const RE::ObjectRefHandle& a_handle,
		const RE::TESObjectREFR& a_droppedReference,
		const RE::NiPoint3& a_playerPosition)
	{
		const auto droppedPosition = a_droppedReference.GetPosition();
		const auto distance = droppedPosition.GetDistance(a_playerPosition);
		auto* parentCell = a_droppedReference.GetParentCell();
		const auto* cellName = parentCell ? parentCell->GetName() : nullptr;

		logs::info(
			"Player drop reference: referenceFormID={:08X} handle={:08X} "
			"playerPosition=({:.3f},{:.3f},{:.3f}) droppedPosition=({:.3f},{:.3f},{:.3f}) "
			"distance={:.3f} parentCellFormID={:08X} parentCell=\"{}\"",
			a_droppedReference.GetFormID(),
			a_handle.native_handle(),
			a_playerPosition.x,
			a_playerPosition.y,
			a_playerPosition.z,
			droppedPosition.x,
			droppedPosition.y,
			droppedPosition.z,
			distance,
			parentCell ? parentCell->GetFormID() : 0,
			cellName && cellName[0] != '\0' ? cellName : "<unavailable>");
	}

	void LogResult(
		const PlayerDisarm::Request& a_request,
		const Resolution& a_resolution,
		std::string_view a_result,
		std::string_view a_reason = {})
	{
		if (a_reason.empty()) {
			logs::info(
				"Player disarm: hand={} weapon=\"{}\" inventoryEntryFound={} "
				"wornInstanceFound={} wornMatchCount={} result={}",
				a_request.leftHand ? "left" : "right",
				a_resolution.weaponName,
				a_resolution.inventoryEntryFound,
				a_resolution.wornInstanceFound,
				a_resolution.wornMatchCount,
				a_result);
		} else {
			logs::info(
				"Player disarm: hand={} weapon=\"{}\" inventoryEntryFound={} "
				"wornInstanceFound={} wornMatchCount={} result={} reason={}",
				a_request.leftHand ? "left" : "right",
				a_resolution.weaponName,
				a_resolution.inventoryEntryFound,
				a_resolution.wornInstanceFound,
				a_resolution.wornMatchCount,
				a_result,
				a_reason);
		}
	}

	Resolution ResolveCandidate(const PlayerDisarm::Request& a_request)
	{
		Resolution result;
		if (a_request.expectedWeaponFormID == 0) {
			result.reason = "invalid_request";
			return result;
		}

		result.player = RE::PlayerCharacter::GetSingleton();
		if (!result.player) {
			result.reason = "player_unavailable";
			return result;
		}

		auto* equippedForm = result.player->GetEquippedObject(a_request.leftHand);
		result.weapon = equippedForm ? equippedForm->As<RE::TESObjectWEAP>() : nullptr;
		result.weaponName = GetWeaponName(result.weapon);
		if (!result.weapon || result.weapon->GetFormID() != a_request.expectedWeaponFormID) {
			result.reason = "equipped_weapon_changed";
			return result;
		}
		if (!result.weapon->IsMelee()) {
			result.reason = "not_melee_weapon";
			return result;
		}
		if (result.weapon->GetWeaponType() == RE::WEAPON_TYPE::kHandToHandMelee) {
			result.reason = "hand_to_hand";
			return result;
		}
		if (result.weapon->IsBound()) {
			result.reason = "bound_weapon";
			return result;
		}

		auto* inventoryChanges = result.player->GetInventoryChanges();
		if (!inventoryChanges || !inventoryChanges->entryList) {
			result.reason = "inventory_unavailable";
			return result;
		}

		RE::InventoryEntryData* matchingEntry = nullptr;
		for (auto* entry : *inventoryChanges->entryList) {
			if (!entry || entry->object != result.weapon) {
				continue;
			}

			++result.inventoryEntryMatchCount;
			matchingEntry = entry;
		}

		result.inventoryEntryFound = result.inventoryEntryMatchCount > 0;
		if (!result.inventoryEntryFound) {
			result.reason = "inventory_entry_missing";
			return result;
		}
		if (result.inventoryEntryMatchCount != 1 || !matchingEntry) {
			result.reason = "ambiguous_inventory_entry";
			return result;
		}
		if (matchingEntry->IsQuestObject()) {
			result.reason = "quest_object";
			return result;
		}
		if (!matchingEntry->extraLists) {
			result.reason = "worn_instance_missing";
			return result;
		}

		bool ambiguousHandMarkers = false;
		for (auto* extraList : *matchingEntry->extraLists) {
			if (!extraList) {
				continue;
			}

			const auto wornRight = extraList->HasType<RE::ExtraWorn>();
			const auto wornLeft = extraList->HasType<RE::ExtraWornLeft>();
			if (!(a_request.leftHand ? wornLeft : wornRight)) {
				continue;
			}

			++result.wornMatchCount;
			result.inventoryOwnedExtra = extraList;
			ambiguousHandMarkers |= wornRight && wornLeft;
		}

		result.wornInstanceFound = result.wornMatchCount > 0;
		if (!result.wornInstanceFound) {
			result.reason = "worn_instance_missing";
			return result;
		}
		if (result.wornMatchCount != 1 || ambiguousHandMarkers) {
			result.inventoryOwnedExtra = nullptr;
			result.reason = "ambiguous_worn_instance";
			return result;
		}
		if (result.inventoryOwnedExtra->HasQuestObjectAlias()) {
			result.inventoryOwnedExtra = nullptr;
			result.reason = "quest_object";
			return result;
		}

		result.valid = true;
		result.reason = {};
		return result;
	}

	RE::ExtraDataList* FindProcessCacheExtraForDiagnostics(
		const Resolution& a_resolution,
		bool a_leftHand)
	{
		// This pointer is displayed for comparison only. It must never be used for mutation.
		auto* entry = a_resolution.player ? a_resolution.player->GetEquippedEntryData(a_leftHand) : nullptr;
		if (!entry || entry->object != a_resolution.weapon || !entry->extraLists) {
			return nullptr;
		}

		RE::ExtraDataList* match = nullptr;
		for (auto* extraList : *entry->extraLists) {
			if (!extraList) {
				continue;
			}

			const auto matchesHand = a_leftHand ?
				extraList->HasType<RE::ExtraWornLeft>() :
				extraList->HasType<RE::ExtraWorn>();
			if (!matchesHand) {
				continue;
			}
			if (match) {
				return nullptr;
			}
			match = extraList;
		}
		return match;
	}
}

bool PlayerDisarm::CanDropAttackingWeapon(const Request& a_request, bool a_logResult)
{
	const auto resolution = ResolveCandidate(a_request);
	if (!resolution.valid && a_logResult) {
		LogResult(a_request, resolution, "SKIPPED", resolution.reason);
	}
	return resolution.valid;
}

PlayerDisarm::DropResult PlayerDisarm::TryDropAttackingWeapon(const Request& a_request, bool a_logResult)
{
	// Resolve again immediately before mutation. Mutation pointers must originate from the
	// authoritative InventoryChanges::entryList, never from GetEquippedEntryData().
	auto resolution = ResolveCandidate(a_request);
	if (!resolution.valid) {
		if (a_logResult) {
			LogResult(a_request, resolution, "SKIPPED", resolution.reason);
		}
		return DropResult::kSkipped;
	}

	if (a_logResult) {
		auto* processCacheExtra = FindProcessCacheExtraForDiagnostics(resolution, a_request.leftHand);
		logs::info(
			"Player disarm pointer audit: processCacheExtra={} inventoryOwnedExtra={}",
			static_cast<const void*>(processCacheExtra),
			static_cast<const void*>(resolution.inventoryOwnedExtra));
	}

	const auto dropPosition = resolution.player->GetPosition();
	const auto dropAngle = resolution.player->GetAngle();
	const auto droppedHandle = resolution.player->RemoveItem(
		resolution.weapon,
		1,
		RE::ITEM_REMOVE_REASON::kDropping,
		resolution.inventoryOwnedExtra,
		resolution.player,
		std::addressof(dropPosition),
		std::addressof(dropAngle));

	// The inventory-owned ExtraDataList may have been consumed or invalidated by RemoveItem.
	// Do not inspect any candidate inventory pointers after this point.
	if (!droppedHandle) {
		if (a_logResult) {
			LogResult(a_request, resolution, "FAILED", "remove_item_failed");
		}
		return DropResult::kFailed;
	}

	const auto droppedReference = droppedHandle.get();
	if (!droppedReference) {
		if (a_logResult) {
			LogResult(a_request, resolution, "UNVERIFIED", "dropped_reference_unresolved");
		}
		return DropResult::kUnverified;
	}
	if (droppedReference->GetBaseObject() != resolution.weapon) {
		if (a_logResult) {
			LogResult(a_request, resolution, "UNVERIFIED", "dropped_reference_base_mismatch");
		}
		return DropResult::kUnverified;
	}

	LogDroppedReference(droppedHandle, *droppedReference, resolution.player->GetPosition());

	if (a_logResult) {
		LogResult(a_request, resolution, "DROPPED");
	}

	DropPhysics::Queue(
		droppedHandle,
		a_request.expectedWeaponFormID,
		DropPhysics::MakeTestImpulse(resolution.player->GetAngleZ()));
	return DropResult::kDropped;
}
