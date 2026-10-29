#pragma once

namespace Settings
{
	inline constexpr int kMinimumValue = 0;
	inline constexpr int kMaximumValue = 100;

	struct Values
	{
		bool enabled{ true };
		bool debugLogging{ false };
		bool fatigueDisarms{ true };
		bool clashDisarms{ false };
		bool dropPhysics{ true };
		int staminaThreshold{ 30 };
		int baseDropChance{ 30 };
		int staminaInfluence{ 50 };
		int skillInfluence{ 50 };
		int physicsStrength{ 30 };
	};

	Values& Get() noexcept;
	void Load();
	bool Save();
}
