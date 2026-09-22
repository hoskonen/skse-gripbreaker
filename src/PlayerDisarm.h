#pragma once

namespace PlayerDisarm
{
	struct Request
	{
		RE::FormID expectedWeaponFormID{ 0 };
		bool leftHand{ false };
	};

	enum class DropResult
	{
		kDropped,
		kSkipped,
		kFailed,
		kUnverified
	};

	[[nodiscard]] bool CanDropAttackingWeapon(const Request& a_request, bool a_logResult = false);
	[[nodiscard]] DropResult TryDropAttackingWeapon(const Request& a_request, bool a_logResult = false);
}
