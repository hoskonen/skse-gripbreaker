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
		changed |= ImGuiMCP::SliderInt("Stamina threshold", &settings.staminaThreshold, Settings::kMinimumValue, Settings::kMaximumValue, "%d%%");
		changed |= ImGuiMCP::SliderInt("Base drop chance", &settings.baseDropChance, Settings::kMinimumValue, Settings::kMaximumValue, "%d%%");
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
