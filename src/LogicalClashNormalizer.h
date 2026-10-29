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
		RE::ActorHandle actorHandle;
	};

	struct CollisionSample
	{
		Participant attacker;
		Participant target;
		RE::NiPoint3 position;
		RE::NiPoint3 hitPointVelocity;
		std::chrono::steady_clock::time_point collisionTime;
		bool logDiagnostics{ false };
		bool requestClashDisarm{ false };
	};

	void Record(CollisionSample a_sample);
}
