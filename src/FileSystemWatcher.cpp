#include "../include/FileSystemWatcher.h"

#include <tchar.h>

namespace fs
{
    FileSystemWatcher::FileSystemWatcher()
    {
        InitializeFileSystemWatcher();
    }
    FileSystemWatcher::~FileSystemWatcher()
    {
        if (watching)
            StopWatching();
    }

    void FileSystemWatcher::StartWatching(const std::filesystem::path& watchPath)
    {
        if (watching)
            StopWatching();

        osFileWatcher->StartWatching(watchPath);
        watching = true;
    }
    void FileSystemWatcher::StopWatching()
    {
        osFileWatcher->StopWatching();
        watching = false;
    }

    void FileSystemWatcher::AddFileEvent(const FileEvent& fileEvent)
    {
        std::lock_guard mutex_guard{ fileEventsMutex };
        fileEvents.push(fileEvent);
    }
    FileEvent FileSystemWatcher::RetrieveFileEvent()
    {
        std::lock_guard mutex_guard{ fileEventsMutex };
        FileEvent fileEvent = fileEvents.front();
        fileEvents.pop();
        return fileEvent;
    }

    bool FileSystemWatcher::HasFileEvents()
    {
        std::lock_guard mutex_guard{ fileEventsMutex };
        return !fileEvents.empty();
    }
    size_t FileSystemWatcher::FileEventsAvailable()
    {
        std::lock_guard mutex_guard{ fileEventsMutex };
        return fileEvents.size();
    }

    void FileSystemWatcher::InitializeFileSystemWatcher()
    {
#ifdef _WIN32
        osFileWatcher = std::make_unique<WinFileSystemWatcher>(this);
#elif  __linux__
        osFileWatcher = std::make_unique<LinuxFileSystemWatcher>(this);
#endif
    }
}