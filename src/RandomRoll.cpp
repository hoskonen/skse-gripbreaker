#include "RandomRoll.h"

#include <random>

float RandomRoll::Percent()
{
	static std::mt19937 generator{ std::random_device{}() };
	return std::generate_canonical<float, 24>(generator) * 100.0f;
}
