#pragma once

#include <chrono>

namespace Enola2
{
	double GetProgramElapsedSeconds()
	{
		using Clock = std::chrono::steady_clock;
		static const auto programStart = Clock::now();
		const auto now = Clock::now();
		return std::chrono::duration<double>(now - programStart).count();
	}
	class Timer
	{
	private:
	public:
		virtual void Tick(double t_s) {}
		void SetTickInterval(double intervalS)
		{
		}
		static void RegTimer(Timer* timer)
		{
		}
	};
}