#pragma once

#include "FileSystemApi.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs
{
	enum class FileEventType
	{
		ADDED,
		REMOVED,
		MOVED,
		MODIFIED,
		RENAMED
	};

	struct FileEvent
	{
		FS_API static FileEvent CreateAddedEvent(const std::filesystem::path& newPath);
		FS_API static FileEvent CreateRemovedEvent(const std::filesystem::path& oldPath);
		FS_API static FileEvent CreateMovedEvent(
			const std::filesystem::path& oldPath,
			const std::filesystem::path& newPath);
		FS_API static FileEvent CreateModifiedEvent(const std::filesystem::path& oldPath);
		FS_API static FileEvent CreateRenamedEvent(
			const std::filesystem::path& oldPath,
			const std::filesystem::path& newPath);

		// ADDED messages use only 'newPath'
		//
		// REMOVED messages use only 'oldPath'
		//
		// MOVED messages use both 'oldPath' and 'newPath'
		// 
		// MODIFIED messages use only 'oldPath
		//
		// RENAMED messages use both 'oldPath' and 'newPath'

		std::filesystem::path oldPath;
		std::filesystem::path newPath;

		FileEventType type;
	};

	enum class DirectoryEntryType
	{
		DIRECTORY,
		FILE,
		UNDEFINED
	};

	enum class DirEntrySortType
	{
		// [...]_L_TO_H means "Lower to Higher"
		// [...]_H_TO_L means "Higher to Lower"

		ALPHABETICAL_L_TO_H,
		ALPHABETICAL_H_TO_L,
		LAST_WRITE_TIME_L_TO_H,
		LAST_WRITE_TIME_H_TO_L,
	};

	class Directory;

	// DirectoryEntry

	class DirectoryEntry
	{
	public:

		FS_API DirectoryEntry(const std::filesystem::path& dirEntryPath);

		virtual bool IsFile() const = 0;
		virtual bool IsDirectory() const = 0;
		virtual DirectoryEntryType GetDirectoryEntryType() const = 0;
		virtual std::string GetName() const = 0;

		FS_API void Rename(const std::string& newName);

		FS_API const std::filesystem::path& GetPath() const;
		FS_API virtual void UpdatePath();

		FS_API void SetParentDirectory(std::shared_ptr<Directory> parentDir);
		FS_API void ClearParentDirectory();

		FS_API std::shared_ptr<Directory> GetParentDirectory() const;

		FS_API bool Exists() const;

		FS_API void UpdateStatus(const std::filesystem::path& absPart);
		FS_API bool Modified() const;

		FS_API std::filesystem::file_time_type GetLastWriteTime() const;

	protected:

		std::filesystem::path dirEntryPath;

		std::filesystem::file_time_type lastWriteTime{};

		std::weak_ptr<Directory> parentDir;

		bool modified{ false };
	};

	class File;
	class Sorter;

	// Directory

	class Directory : public DirectoryEntry
	{
	public:

		using Directories = std::vector<std::shared_ptr<Directory>>;
		using Files = std::vector<std::shared_ptr<File>>;
		using DirectoryEntries = std::vector<std::shared_ptr<DirectoryEntry>>;

		FS_API static void AddEntryToDirectory(std::shared_ptr<Directory> dir, std::shared_ptr<DirectoryEntry> entry);
		FS_API static void AddDirectoryToDirectory(std::shared_ptr<Directory> where, std::shared_ptr<Directory> what);
		FS_API static void AddFileToDirectory(std::shared_ptr<Directory> where, std::shared_ptr<File> file);

		FS_API Directory(const std::filesystem::path& dirPath);

		FS_API bool IsFile() const override;
		FS_API bool IsDirectory() const override;
		FS_API DirectoryEntryType GetDirectoryEntryType() const override;
		FS_API std::string GetName() const override;

		FS_API void UpdatePath() override;

		FS_API void AddDirectoryEntry(std::shared_ptr<DirectoryEntry> entry);
		FS_API void AddDirectory(std::shared_ptr<Directory> dir);
		FS_API void AddFile(std::shared_ptr<File> file);

		FS_API void DeleteDirectoryEntry(std::shared_ptr<DirectoryEntry> entry);
		FS_API void DeleteDirectory(const std::string& dirName);
		FS_API void DeleteDirectory(std::shared_ptr<Directory> dir);
		FS_API void DeleteFile(const std::string& fileName);
		FS_API void DeleteFile(std::shared_ptr<File> file);

		// Returns a default constructed 'shared_ptr<Directory>' object
		// if the directory being searched for doesn't exist
		FS_API std::shared_ptr<Directory> GetDirectory(const std::string& dirName);
		// Returns a default constructed 'shared_ptr<File>' object
		// if the file being searched for doesn't exist
		FS_API std::shared_ptr<File> GetFile(const std::string& fileName);

		FS_API bool DirectoryExists(const std::string& dirName);
		FS_API bool FileExists(const std::string& fileName);

		FS_API bool IsEmpty() const;

		FS_API std::string GetDirectoryName() const;

		FS_API std::vector<std::shared_ptr<Directory>> GetDirectories() const;
		FS_API std::vector<std::shared_ptr<Directory>> GetDirectoriesRecursive() const;
		FS_API std::vector<std::shared_ptr<File>> GetFiles() const;
		FS_API std::vector<std::shared_ptr<File>> GetFilesRecursive() const;

		FS_API std::vector<std::shared_ptr<DirectoryEntry>> GetDirEntries() const;
		FS_API std::vector<std::shared_ptr<DirectoryEntry>> GetDirEntriesRecursive() const;

		FS_API DirEntrySortType GetSortingType() const;
		FS_API void SetSortingType(DirEntrySortType sortType);

	private:

		void SortDirectories();
		void SortFiles();

		void InsertDirectorySorted(std::shared_ptr<Directory> dir);
		void InsertFileSorted(std::shared_ptr<File> file);

		Directories directories;
		Files files;

		std::shared_ptr<Sorter> sorter;
		DirEntrySortType sortType{ DirEntrySortType::ALPHABETICAL_L_TO_H };
	};

	// File

	class File : public DirectoryEntry
	{
	public:

		FS_API File(const std::filesystem::path& filePath);

		FS_API bool IsFile() const override;
		FS_API bool IsDirectory() const override;
		FS_API DirectoryEntryType GetDirectoryEntryType() const override;
		FS_API std::string GetName() const override;

		FS_API void UpdatePath() override;

		FS_API std::string GetFullFileName() const;
		FS_API std::string GetFileName() const;
		FS_API std::string GetFileExtension() const;

	private:

	};

	// Sorters

	class Sorter
	{
	public:

		enum class SortCompFun
		{
			LESS,
			GREATER
		};

		FS_API void SetSortingCompFun(SortCompFun comp);
		FS_API SortCompFun GetSortingCompFun() const;

		virtual void SortDirectories(
			std::vector<std::shared_ptr<Directory>>& directories) = 0;
		virtual void SortFiles(
			std::vector<std::shared_ptr<File>>& files) = 0;

		virtual void InsertDirectorySorted(
			std::shared_ptr<Directory> directory,
			std::vector<std::shared_ptr<Directory>>& directories) = 0;
		virtual void InsertFileSorted(
			std::shared_ptr<File> file,
			std::vector<std::shared_ptr<File>>& files) = 0;

	private:

		SortCompFun comp{ SortCompFun::LESS };
	};

	class AlphabeticalSorter : public Sorter
	{
	public:

		FS_API void SortDirectories(
			std::vector<std::shared_ptr<Directory>>& directories) override;
		FS_API void SortFiles(
			std::vector<std::shared_ptr<File>>& files) override;

		FS_API void InsertDirectorySorted(
			std::shared_ptr<Directory> directory,
			std::vector<std::shared_ptr<Directory>>& directories) override;
		FS_API void InsertFileSorted(
			std::shared_ptr<File> file,
			std::vector<std::shared_ptr<File>>& files) override;
	};

	class LastWriteTimeSorter : public Sorter
	{
	public:

		FS_API void SortDirectories(
			std::vector<std::shared_ptr<Directory>>& directories) override;
		FS_API void SortFiles(
			std::vector<std::shared_ptr<File>>& files) override;

		FS_API void InsertDirectorySorted(
			std::shared_ptr<Directory> directory,
			std::vector<std::shared_ptr<Directory>>& directories) override;
		FS_API void InsertFileSorted(
			std::shared_ptr<File> file,
			std::vector<std::shared_ptr<File>>& files) override;
	};
}