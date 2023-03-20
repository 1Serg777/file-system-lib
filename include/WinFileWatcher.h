#pragma once

#include <filesystem>
#include <mutex>
#include <thread>
#include <Windows.h>

#include "FileSystemCommon.h"
#include "Timer.h"

namespace fs
{
	class FileSystemWatcher;

	class WinFileSystemWatcher
	{
	public:

		WinFileSystemWatcher(FileSystemWatcher* fileWatcher);
		~WinFileSystemWatcher();

		void StartWatching(const std::filesystem::path& watchPath);
		void StopWatching();

	private:

		void InitializeWinFileWatcher();
		void InitializeWindowsSpecificObjects();
		void ClearWindowsSpecificObjects();

		void MainLoop();

		void MovedEventTimerExpired();

		void ProcessActions(FILE_NOTIFY_INFORMATION* info);

		std::filesystem::path watchPath;
		std::thread watcherThread;
		FileSystemWatcher* fileSystemWatcher{ nullptr };

		std::mutex moved_event_mutex;

		impl::Timer movedEventTimer;
		int timerCallbackId{ 0 };
		bool movedEventWaiting{ false };

		FileEvent stashedRemovedEvent;

		HANDLE dirHandle{ NULL };
		HANDLE dirWatchEvent{ NULL };
		OVERLAPPED dirChangesIO{ NULL };
	};
}