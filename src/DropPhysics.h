#pragma once

namespace DropPhysics
{
	[[nodiscard]] RE::hkVector4 MakeTestImpulse(float a_playerHeadingRadians);

	void Queue(
		RE::ObjectRefHandle a_droppedHandle,
		RE::FormID a_expectedWeaponFormID,
		RE::hkVector4 a_impulse);
}
