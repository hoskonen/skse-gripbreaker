#include "Settings.h"

#include <SimpleIni.h>

namespace
{
	constexpr auto kSettingsPath = "Data/SKSE/Plugins/GripBreaker.ini";
	constexpr auto kSection = "Settings";

	Settings::Values values;

	bool Validate()
	{
		const auto oldStaminaThreshold = values.staminaThreshold;
		const auto oldBaseDropChance = values.baseDropChance;
		const auto oldPhysicsStrength = values.physicsStrength;

		values.staminaThreshold = std::clamp(values.staminaThreshold, Settings::kMinimumValue, Settings::kMaximumValue);
		values.baseDropChance = std::clamp(values.baseDropChance, Settings::kMinimumValue, Settings::kMaximumValue);
		values.physicsStrength = std::clamp(values.physicsStrength, Settings::kMinimumValue, Settings::kMaximumValue);

		return oldStaminaThreshold != values.staminaThreshold ||
		       oldBaseDropChance != values.baseDropChance ||
		       oldPhysicsStrength != values.physicsStrength;
	}

	void Configure(CSimpleIniA& a_ini)
	{
		a_ini.SetUnicode();
		a_ini.SetQuotes();
	}
}

Settings::Values& Settings::Get() noexcept
{
	return values;
}

void Settings::Load()
{
	values = {};

	try {
		if (!std::filesystem::exists(kSettingsPath)) {
			logs::info("Settings file not found; using defaults");
			return;
		}

		CSimpleIniA ini;
		Configure(ini);

		const auto result = ini.LoadFile(kSettingsPath);
		if (result < 0) {
			logs::error("Failed to load settings from {} (error {})", kSettingsPath, result);
			return;
		}

		values.enabled = ini.GetBoolValue(kSection, "Enabled", values.enabled);
		values.debugLogging = ini.GetBoolValue(kSection, "DebugLogging", values.debugLogging);
		values.staminaThreshold = static_cast<int>(ini.GetLongValue(kSection, "StaminaThreshold", values.staminaThreshold));
		values.baseDropChance = static_cast<int>(ini.GetLongValue(kSection, "BaseDropChance", values.baseDropChance));
		values.physicsStrength = static_cast<int>(ini.GetLongValue(kSection, "PhysicsStrength", values.physicsStrength));

		if (Validate()) {
			logs::warn("Clamped one or more settings from {} to the valid range {}-{}", kSettingsPath, kMinimumValue, kMaximumValue);
		}

		logs::info("Loaded settings from {}", kSettingsPath);
	} catch (const std::exception& exception) {
		values = {};
		logs::error("Failed to load settings from {}: {}", kSettingsPath, exception.what());
	}
}

bool Settings::Save()
{
	Validate();

	try {
		CSimpleIniA ini;
		Configure(ini);

		if (std::filesystem::exists(kSettingsPath)) {
			const auto loadResult = ini.LoadFile(kSettingsPath);
			if (loadResult < 0) {
				logs::error("Failed to read {} before saving settings (error {})", kSettingsPath, loadResult);
				return false;
			}
		}

		ini.SetBoolValue(kSection, "Enabled", values.enabled);
		ini.SetBoolValue(kSection, "DebugLogging", values.debugLogging);
		ini.SetLongValue(kSection, "StaminaThreshold", values.staminaThreshold);
		ini.SetLongValue(kSection, "BaseDropChance", values.baseDropChance);
		ini.SetLongValue(kSection, "PhysicsStrength", values.physicsStrength);

		const auto result = ini.SaveFile(kSettingsPath);
		if (result < 0) {
			logs::error("Failed to save settings to {} (error {})", kSettingsPath, result);
			return false;
		}

		logs::debug("Saved settings to {}", kSettingsPath);
		return true;
	} catch (const std::exception& exception) {
		logs::error("Failed to save settings to {}: {}", kSettingsPath, exception.what());
		return false;
	}
}
