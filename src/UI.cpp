#include "Settings.h"
#include "UI.h"

#include <SKSEMenuFramework.h>

namespace
{
	void __stdcall RenderSettings()
	{
		auto& settings = Settings::Get();

		bool changed = false;
		changed |= ImGuiMCP::Checkbox("Enabled", &settings.enabled);
		changed |= ImGuiMCP::Checkbox("Debug logging", &settings.debugLogging);
		changed |= ImGuiMCP::SliderInt("Base stamina threshold", &settings.staminaThreshold, Settings::kMinimumValue, Settings::kMaximumValue, "%d%%");
		ImGuiMCP::TextWrapped(
			"Base stamina level before skill reduction. Relevant weapon skill, or Block for bash/blocking "
			"contexts, lowers the effective threshold. Under the current model, skill 100 can reduce the "
			"threshold by up to 50%%.");
		ImGuiMCP::Spacing();
		changed |= ImGuiMCP::SliderInt("Base drop chance", &settings.baseDropChance, Settings::kMinimumValue, Settings::kMaximumValue, "%d%%");
		ImGuiMCP::TextWrapped(
			"Not a flat chance whenever stamina is below the threshold. This is the maximum fatigue-drop "
			"chance at complete exhaustion; the actual chance is reduced by fatigue risk and its squared "
			"risk curve.");
		ImGuiMCP::Spacing();
		ImGuiMCP::Text("Relevant skill:");
		ImGuiMCP::BulletText("One-Handed / Two-Handed for attacks");
		ImGuiMCP::BulletText("Block for bash/blocking contexts");
		ImGuiMCP::Spacing();
		changed |= ImGuiMCP::SliderInt("Physics strength", &settings.physicsStrength, Settings::kMinimumValue, Settings::kMaximumValue);

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
