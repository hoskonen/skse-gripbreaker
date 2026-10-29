#include "Settings.h"
#include "UI.h"

#include <SKSEMenuFramework.h>

namespace
{
	void __stdcall RenderSettings()
	{
		auto& settings = Settings::Get();

		bool changed = false;
		ImGuiMCP::Text("General");
		changed |= ImGuiMCP::Checkbox("Enable GripBreaker", &settings.enabled);
		ImGuiMCP::TextWrapped(
			"Enables or disables all GripBreaker gameplay mechanics.");
		ImGuiMCP::Spacing();
		changed |= ImGuiMCP::Checkbox("Debug logging", &settings.debugLogging);
		ImGuiMCP::TextWrapped(
			"Writes detailed combat and clash diagnostics to the GripBreaker log.");

		ImGuiMCP::Separator();
		ImGuiMCP::Text("Fatigue Disarms");
		changed |= ImGuiMCP::Checkbox("Enable fatigue disarms", &settings.fatigueDisarms);
		ImGuiMCP::TextWrapped(
			"Allows attacks performed at very low stamina to cause the player's weapon to slip from their grip.");
		ImGuiMCP::Spacing();
		ImGuiMCP::BeginDisabled(!settings.fatigueDisarms);
		changed |= ImGuiMCP::SliderInt(
			"Stamina threshold",
			&settings.staminaThreshold,
			Settings::kMinimumValue,
			Settings::kMaximumValue,
			"%d%%");
		ImGuiMCP::TextWrapped(
			"Sets the stamina level where fatigue begins to affect weapon retention. Weapon skill lowers "
			"the effective threshold.");
		ImGuiMCP::Spacing();
		changed |= ImGuiMCP::SliderInt(
			"Maximum drop chance",
			&settings.baseDropChance,
			Settings::kMinimumValue,
			Settings::kMaximumValue,
			"%d%%");
		ImGuiMCP::TextWrapped(
			"Sets the highest fatigue-disarm chance at complete exhaustion. The chance decreases rapidly "
			"as stamina rises.");
		ImGuiMCP::EndDisabled();

		ImGuiMCP::Separator();
		ImGuiMCP::Text("Weapon Clash Disarms");
		changed |= ImGuiMCP::Checkbox("Enable clash disarms", &settings.clashDisarms);
		ImGuiMCP::TextWrapped(
			"Allows weapon-on-weapon clashes to break the player's grip.");
		ImGuiMCP::Spacing();
		ImGuiMCP::BeginDisabled(!settings.clashDisarms);
		changed |= ImGuiMCP::SliderInt(
			"Stamina influence",
			&settings.staminaInfluence,
			Settings::kMinimumValue,
			Settings::kMaximumValue,
			"%d%%");
		ImGuiMCP::TextWrapped(
			"Controls how strongly low stamina weakens weapon grip during clashes.");
		ImGuiMCP::Spacing();
		changed |= ImGuiMCP::SliderInt(
			"Skill influence",
			&settings.skillInfluence,
			Settings::kMinimumValue,
			Settings::kMaximumValue,
			"%d%%");
		ImGuiMCP::TextWrapped(
			"Controls how strongly weapon skill improves grip during clashes.");
		ImGuiMCP::EndDisabled();

		ImGuiMCP::Separator();
		ImGuiMCP::Text("Dropped Weapon Physics");
		changed |= ImGuiMCP::Checkbox("Enable throw physics", &settings.dropPhysics);
		ImGuiMCP::TextWrapped(
			"Applies physical motion to weapons after GripBreaker drops them into the world.");
		ImGuiMCP::Spacing();
		ImGuiMCP::BeginDisabled(!settings.dropPhysics);
		changed |= ImGuiMCP::SliderInt(
			"Throw strength",
			&settings.physicsStrength,
			Settings::kMinimumValue,
			Settings::kMaximumValue);
		ImGuiMCP::TextWrapped(
			"Prototype setting; physical throw tuning is still under development.");
		ImGuiMCP::EndDisabled();

		if (changed) {
			Settings::Save();
		}
	}
}

void UI::Register()
{
	if (!SKSEMenuFramework::IsInstalled()) {
		logs::info("SKSE Menu Framework is not installed; GripBreaker settings UI is disabled");
		return;
	}

	SKSEMenuFramework::SetSection("GripBreaker");
	SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
	logs::info("Registered GripBreaker settings with SKSE Menu Framework");
}
