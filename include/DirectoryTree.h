#pragma once

#include "FileSystemCommon.h"

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace fs
{
	class DirectoryTreeProcessor
	{
	public:
		// 'Directory' and 'File' classes are not multithreading-aware, so please make sure not to store
		// any directories and files inside derived classes of this interface.
		virtual void ProcessDirectoryTree(std::shared_ptr<Directory> root) = 0;
	};

	class DirectoryTreeEventListener
	{
	public:
		virtual void OnDirectoryAdded(std::shared_ptr<Directory> dir) = 0;
		virtual void OnDirectoryRemoved(std::shared_ptr<Directory> dir) = 0;

		virtual void OnFileAdded(std::shared_ptr<File> file) = 0;
		virtual void OnFileRemoved(std::shared_ptr<File> file) = 0;

		virtual void OnFileModified(std::shared_ptr<File> file) = 0;
		virtual void OnDirectoryModified(std::shared_ptr<Directory> dir) = 0;
	};

	class DirectoryTree
	{
	public:

		FS_API void AddDirTreeEventListener(DirectoryTreeEventListener* listener);
		FS_API void RemoveDirTreeEventListener(DirectoryTreeEventListener* listener);

		FS_API void BuildRootTree(const std::filesystem::path& rootDirAbsPath);

		FS_API void ClearTree();

		FS_API void AddNewFile(const std::filesystem::path& filePath);
		FS_API void AddNewDirectory(const std::filesystem::path& dirPath);

		FS_API void RemoveFile(const std::filesystem::path& filePath);
		FS_API void RemoveDirectory(const std::filesystem::path& dirPath);

		FS_API void MoveFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		FS_API void MoveDirectory(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);

		FS_API void ProcessModifiedFile(const std::filesystem::path& oldPath);
		FS_API void ProcessModifiedDirectory(const std::filesystem::path& oldPath);

		FS_API void RenameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
		FS_API void RenameDirectory(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);

		// This is dangerous because we can't know what this object is going to do
		// with our root directory. It can store it, traverse its subderectoires and/or store them as well.
		// So many things that could potentially break our protection of the data that the mutex provides us with.
		FS_API void ProcessDirectoryTree(DirectoryTreeProcessor* processor);

		// Return a default constructed 'std::shared_ptr<Directory>' instance if the directory doesn't exist
		FS_API std::shared_ptr<Directory> GetDirectory(const std::filesystem::path& dirPath) const;

		FS_API std::shared_ptr<Directory> GetRootDirectory() const;

	private:

		void NotifyDirectoryAdded(std::shared_ptr<Directory> dir);
		void NotifyDirectoryRemoved(std::shared_ptr<Directory> dir);
		void NotifyDirectoryModified(std::shared_ptr<Directory> dir);

		void NotifyFileAdded(std::shared_ptr<File> file);
		void NotifyFileRemoved(std::shared_ptr<File> file);
		void NotifyFileModified(std::shared_ptr<File> file);

		std::shared_ptr<File> CreateFile(const std::filesystem::path& relPath) const;
		std::shared_ptr<Directory> CreateDirectory(const std::filesystem::path& relPath) const;

		std::shared_ptr<Directory> BuildTree(const std::filesystem::path& dirPath);

		// Change 'old path' to 'new path' links
		void ResolveDirectoryMapLinks(
			const std::filesystem::path& newDirPath,
			const std::filesystem::path& relPathToDir,
			std::shared_ptr<Directory> relPathDir);

		std::unordered_map<
			std::filesystem::path,
			std::shared_ptr<Directory>> directories;

		std::shared_ptr<Directory> rootDir;
		std::filesystem::path rootDirAbsParentPath;

		// Callbacks

		std::vector<DirectoryTreeEventListener*> listeners;
	};
}