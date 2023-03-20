#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

#include "Utility.h"

namespace impl
{
	class Timer
	{
	public:

		// Stops the timer if it's currently running!
		void SetTimer(int64_t timerMs);

		void Start();
		void Pause();
		void Resume();
		void Stop();

		// Don't forget that we have to call
		// one of the two functions below
		// before the timer finishes!

		void WaitForFinish();
		void Detach();

		int AddTimerFinishCallback(std::function<void()> callback);
		void RemoveTimerFinishCallback(int id);

	private:

		void MainLoop();

		void NotifyTimerFinishEvent();

		std::vector<Callback> callbacks;

		std::thread execution_thread;

		std::chrono::time_point<std::chrono::steady_clock> startTimePoint{};

		int64_t currentMs{ 0 };
		int64_t timerMs{ 0 };

		bool running{ false };
		bool exit{ false };
	};
}