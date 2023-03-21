#include "../../include/FileSystem/WinFileWatcher.h"

#include "../../include/FileSystem/FileSystemWatcher.h"

#include <array>
#include <cassert>
#include <tchar.h>

using namespace impl;

namespace fs
{
    constexpr int64_t movedMsgWaitingTimeMs{ 100 };

    constexpr size_t NOTIFY_INFO_SIZE{ 4096 };
    std::array<DWORD, NOTIFY_INFO_SIZE> notifyInfo{};

    // WinFileSystemWatcher

    WinFileSystemWatcher::WinFileSystemWatcher(FileSystemWatcher* fileSystemWatcher)
        : fileSystemWatcher(fileSystemWatcher)
    {
        InitializeWinFileWatcher();
    }
    WinFileSystemWatcher::~WinFileSystemWatcher()
    {
        fileSystemWatcher = nullptr;
    }

    void WinFileSystemWatcher::StartWatching(const std::filesystem::path& watchPath)
    {
        this->watchPath = watchPath;
        watcherThread = std::thread{ &WinFileSystemWatcher::MainLoop, this };
    }
    void WinFileSystemWatcher::StopWatching()
    {
        if (dirHandle != NULL)
            CancelIoEx(dirHandle, &dirChangesIO);
        if (watcherThread.joinable())
            watcherThread.join();
    }

    void WinFileSystemWatcher::InitializeWinFileWatcher()
    {
        movedEventTimer.SetTimer(movedMsgWaitingTimeMs);

        timerCallbackId = movedEventTimer.AddTimerFinishCallback(
            std::bind(&WinFileSystemWatcher::MovedEventTimerExpired, this));
    }
    void WinFileSystemWatcher::InitializeWindowsSpecificObjects()
    {
        dirHandle = CreateFile(
            watchPath.c_str(),
            GENERIC_READ, // includes needed /* FILE_LIST_DIRECTORY */
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);

        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND)
        {
            printf("ERROR: Couldn't find the specified directory.\n");
            printf("Make sure the watched directory exists.\n");
            _tprintf(TEXT("Directory: (%s)\n"), watchPath.c_str());
            ExitProcess(GetLastError());
        }
        else if (error == ERROR_PATH_NOT_FOUND)
        {
            printf("ERROR: Couldn't locate the specified path.\n");
            printf("Make sure the watched directory exists.\n");
            _tprintf(TEXT("Directory: (%s)\n"), watchPath.c_str());
            ExitProcess(GetLastError());
        }
        else if (error == ERROR_ACCESS_DENIED)
        {
            printf("ERROR: Couldn't open the specified directory. Access denied.\n");
            printf("Make sure you have rights to open the watched directory for ");
            printf("'shared read', 'shared write' and 'shared delete'.\n");
            _tprintf(TEXT("Directory: (%s)\n"), watchPath.c_str());
            ExitProcess(GetLastError());
        }

        dirWatchEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (dirWatchEvent == NULL)
        {
            printf("ERROR: Couldn't create a directory watch event.\n");
            ExitProcess(GetLastError());
        }

        dirChangesIO.hEvent = dirWatchEvent;
    }
    void WinFileSystemWatcher::ClearWindowsSpecificObjects()
    {
        if (dirHandle != NULL)
        {
            CloseHandle(dirHandle);
        }
        if (dirWatchEvent != NULL)
        {
            CloseHandle(dirWatchEvent);
        }
        dirChangesIO = OVERLAPPED{};
    }

    void WinFileSystemWatcher::MainLoop()
    {
        ClearWindowsSpecificObjects();
        InitializeWindowsSpecificObjects();

        // _tprintf(TEXT("Watching directory (%s) for notifications...\n"), watchPath.c_str());

        while (TRUE)
        {
            if (!ReadDirectoryChangesW(
                dirHandle,
                notifyInfo.data(),
                NOTIFY_INFO_SIZE,
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                NULL,
                &dirChangesIO,
                NULL))
            {
                DWORD err = GetLastError();
                if (err == ERROR_OPERATION_ABORTED)
                {
                    printf("ReadDirectoryChangesW(...) is aborted\n");
                    _tprintf(TEXT("Stop watching the directory: (%s)\n"), watchPath.c_str());
                    break;
                }
                else
                {
                    printf("ERROR: ReadDirectoryChangesW function error.\n");
                    ExitProcess(GetLastError());
                }
            }

            DWORD bytesTransferred{};
            if (!GetOverlappedResult(dirHandle, &dirChangesIO, &bytesTransferred, TRUE))
            {
                // printf("ERROR: Couldn't get an overlapped result from 'read directory changes' operation.\n");
                // ExitProcess(GetLastError());

                DWORD err = GetLastError();
                if (err == ERROR_OPERATION_ABORTED)
                {
                    printf("GetOverlappedResult(...) for ReadDirectoryChangesW(...) is aborted\n");
                    _tprintf(TEXT("Stop watching the directory: (%s)\n"), watchPath.c_str());
                    break;
                }
                else
                {
                    printf("ERROR: GetOverlappedResult function error.\n");
                    ExitProcess(GetLastError());
                }
            }
            else
            {
                FILE_NOTIFY_INFORMATION* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(notifyInfo.data());

                moved_event_mutex.lock();
                ProcessActions(info);
                moved_event_mutex.unlock();

                memset(notifyInfo.data(), 0, NOTIFY_INFO_SIZE);
            }
        }
    }

    void WinFileSystemWatcher::MovedEventTimerExpired()
    {
        std::lock_guard mutex_guard{ moved_event_mutex };
        if (!movedEventWaiting)
            return;
        fileSystemWatcher->AddFileEvent(stashedRemovedEvent);
        movedEventWaiting = false;
    }

    void WinFileSystemWatcher::ProcessActions(FILE_NOTIFY_INFORMATION* info)
    {
        if (info->Action == 0 && info->FileNameLength == 0)
            return;

        switch (info->Action)
        {
        case FILE_ACTION_ADDED:
        {
            size_t addedFileNameLen = info->FileNameLength / 2;
            std::wstring addedFileName{ info->FileName, addedFileNameLen };

            FileEvent addedEvent = FileEvent::CreateAddedEvent(addedFileName);

            if (movedEventWaiting)
            {
                movedEventTimer.Stop();
                movedEventWaiting = false;

                FileEvent movedEvent = FileEvent::CreateMovedEvent(
                    stashedRemovedEvent.oldPath, addedEvent.newPath);

                fileSystemWatcher->AddFileEvent(movedEvent);
            }
            else
            {
                fileSystemWatcher->AddFileEvent(addedEvent);
            }
        }
        break;

        case FILE_ACTION_REMOVED:
        {
            size_t removedFileNameLen = info->FileNameLength / 2;
            std::wstring removedFileName{ info->FileName, removedFileNameLen };

            FileEvent removedEvent = FileEvent::CreateRemovedEvent(removedFileName);

            if (movedEventWaiting)
            {
                fileSystemWatcher->AddFileEvent(stashedRemovedEvent);
                movedEventTimer.Stop();
            }

            stashedRemovedEvent = removedEvent;

            movedEventTimer.Start();
            movedEventTimer.Detach();

            movedEventWaiting = true;
        }
        break;

        case FILE_ACTION_RENAMED_OLD_NAME:
        {
            size_t oldFileNameLength = info->FileNameLength / 2;
            std::wstring oldFileName{ info->FileName, oldFileNameLength };

            size_t offset{ 0 };
            offset = info->NextEntryOffset;
            assert(offset != 0 &&
                "[FILE_ACTION_RENAMED_OLD_NAME] and [FILE_ACTION_RENAMED_NEW_NAME] must be in the same buffer!");

            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + offset);
            assert(info->Action == FILE_ACTION_RENAMED_NEW_NAME &&
                "The next action must be a [FILE_ACTION_RENAMED_NEW_NAME] message!");

            size_t newFileNameLen = info->FileNameLength / 2;
            std::wstring newFileName{ info->FileName, newFileNameLen };

            FileEvent renamedFileEvent = FileEvent::CreateRenamedEvent(oldFileName, newFileName);

            fileSystemWatcher->AddFileEvent(renamedFileEvent);
        }
        break;

        case FILE_ACTION_MODIFIED:
        {
            size_t modifiedFileNameLen = info->FileNameLength / 2;
            std::wstring modifiedFileName{ info->FileName, modifiedFileNameLen };

            FileEvent modifiedFileEvent = FileEvent::CreateModifiedEvent(modifiedFileName);

            fileSystemWatcher->AddFileEvent(modifiedFileEvent);
        }
        break;

        default:
        {
            assert(false && "Unexpected Action occurred!");
        }
        break;
        }

        if (info->NextEntryOffset != 0)
        {
            size_t offset = info->NextEntryOffset;
            info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<BYTE*>(info) + offset);
            ProcessActions(info);
        }
    }
}