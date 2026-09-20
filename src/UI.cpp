#include "UI.h"

#include <SKSEMenuFramework.h>

namespace
{
	struct Settings
	{
		bool enabled{ true };
		bool debugLogging{ false };
		int staminaThreshold{ 30 };
		int baseDropChance{ 30 };
		int physicsStrength{ 30 };
	};

	Settings settings;

	void __stdcall RenderSettings()
	{
		ImGuiMCP::Checkbox("Enabled", &settings.enabled);
		ImGuiMCP::Checkbox("Debug logging", &settings.debugLogging);
		ImGuiMCP::SliderInt("Stamina threshold", &settings.staminaThreshold, 0, 100, "%d%%");
		ImGuiMCP::SliderInt("Base drop chance", &settings.baseDropChance, 0, 100, "%d%%");
		ImGuiMCP::SliderInt("Physics strength", &settings.physicsStrength, 0, 100);
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
