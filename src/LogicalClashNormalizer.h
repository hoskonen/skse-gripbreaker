#pragma once

namespace LogicalClashNormalizer
{
	enum class Hand : std::uint8_t
	{
		kUnknown,
		kRight,
		kLeft
	};

	struct Participant
	{
		std::uint32_t actorHandle{ 0 };
		RE::FormID actorFormID{ 0 };
		RE::FormID weaponFormID{ 0 };
		Hand hand{ Hand::kUnknown };
		std::string actorName;
		std::string weaponName;
	};

	struct CollisionSample
	{
		Participant attacker;
		Participant target;
		RE::NiPoint3 position;
		RE::NiPoint3 hitPointVelocity;
		std::chrono::steady_clock::time_point collisionTime;
	};

	void Record(CollisionSample a_sample);
}
