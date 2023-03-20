#include "../include/Timer.h"

#include <algorithm>
#include <iostream>

using namespace std::chrono;

namespace impl
{
	void Timer::SetTimer(int64_t timerMs)
	{
		Stop();
		this->timerMs = timerMs;
	}

	void Timer::Start()
	{
		// Otherwise we won't be able to recognize
		// what thread finished its execution:
		// the one that worked before or the one we just launched.
		// 
		// WaitForFinish();

		exit = false;
		running = true;
		execution_thread = std::thread{ &Timer::MainLoop, this };
	}
	void Timer::Pause()
	{
		running = false;
	}
	void Timer::Resume()
	{
		running = true;
	}
	void Timer::Stop()
	{
		running = false;
		exit = true;
		currentMs = 0.0f;
		startTimePoint = std::chrono::time_point<std::chrono::steady_clock>{};
	}

	void Timer::WaitForFinish()
	{
		if (execution_thread.joinable())
			execution_thread.join();
	}
	void Timer::Detach()
	{
		if (execution_thread.joinable())
			execution_thread.detach();
	}

	int Timer::AddTimerFinishCallback(std::function<void()> callback)
	{
		int id = GenerateUniqueId<Callback, int>();

		Callback newCallback{};
		newCallback.id = id;
		newCallback.callback = callback;

		callbacks.push_back(std::move(newCallback));

		return id;
	}
	void Timer::RemoveTimerFinishCallback(int id)
	{
		callbacks.erase(
			std::remove_if(callbacks.begin(), callbacks.end(), [=](const Callback& callback) { return callback.id == id; }),
			callbacks.end());
	}

	void Timer::MainLoop()
	{
		startTimePoint = high_resolution_clock::now();
		// std::chrono::duration<long long, std::nano> currentTimePoint{};
		while (!exit && currentMs < timerMs)
		{
			if (running)
			{
				auto endTimePoint = high_resolution_clock::now();

				// This works
				/*
					auto passedTimePoint = endTimePoint - startTimePoint;
					currentTimePoint += passedTimePoint;

					currentMs = duration_cast<milliseconds>(currentTimePoint).count();

					startTimePoint = endTimePoint;
				*/

				// This works as well
				// 
				currentMs = duration_cast<milliseconds>(endTimePoint - startTimePoint).count();

				// std::cout << "Time left: " << timerMs - currentMs << " ms.\n";
				std::cout << "Current time: " << currentMs << " ms.\n";
				// std::cout << "Time passed: " << timePassedMs.count() << " ms.\n";
			}
			else
			{
				std::cout << "Stalling... (Paused)\n";
			}
		}

		running = false;

		if (!exit)
			NotifyTimerFinishEvent();
	}

	void Timer::NotifyTimerFinishEvent()
	{
		for (const Callback& callback : callbacks)
		{
			callback();
		}
	}
}