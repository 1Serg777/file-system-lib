#pragma once

#include "FileSystemApi.h"
#include "FileSystemCommon.h"

#ifdef _WIN32
#include "WinFileWatcher.h"
#elif  __linux__
#include "LinuxFileWatcher.h"
#endif

#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>

namespace fs
{
	class FileSystemWatcher
	{
	public:

		FS_API FileSystemWatcher();
		FS_API ~FileSystemWatcher();

		FS_API void StartWatching(const std::filesystem::path& watchPath);
		FS_API void StopWatching();

		FS_API void AddFileEvent(const FileEvent& fileEvent);
		FS_API FileEvent RetrieveFileEvent();

		FS_API bool HasFileEvents();
		FS_API size_t FileEventsAvailable();

	private:

		void InitializeFileSystemWatcher();

#ifdef _WIN32
		std::unique_ptr<WinFileSystemWatcher> osFileWatcher;
#elif  __linux__
		std::unique_ptr<LinuxFileSystemWatcher> osFileWatcher;
#endif

		bool watching{ false };

		std::queue<FileEvent> fileEvents;
		std::mutex fileEventsMutex;
	};
}