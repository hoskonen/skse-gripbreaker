#pragma once

namespace RandomRoll
{
	// Game-thread only. Each call draws a fresh percentage in [0, 100).
	[[nodiscard]] float Percent();
}
