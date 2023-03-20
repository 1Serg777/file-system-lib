#include "../include/DirectoryTree.h"

#include <algorithm>
#include <cassert>

namespace fs
{
	void DirectoryTree::AddDirTreeEventListener(DirectoryTreeEventListener* listener)
	{
		listeners.push_back(listener);
	}
	void DirectoryTree::RemoveDirTreeEventListener(DirectoryTreeEventListener* listener)
	{
		listeners.erase(
			std::remove(listeners.begin(), listeners.end(), listener),
			listeners.end());
	}

	void DirectoryTree::BuildRootTree(const std::filesystem::path& rootDirAbsPath)
	{
		this->rootDirAbsParentPath = rootDirAbsPath.parent_path();

		// "Assets"
		std::filesystem::path rootDirRelPath = rootDirAbsPath.filename();
		rootDir = BuildTree(rootDirRelPath);

		// TEST
		// auto entries = rootDir->GetDirEntries();
		// auto entriesRecursive = rootDir->GetDirEntriesRecursive();

		// TEST
		// auto filesRecursive = rootDir->GetFilesRecursive();
		// auto dirsRecursive = rootDir->GetDirectoriesRecursive();
	}

	void DirectoryTree::ClearTree()
	{
		// TODO
		// Don't forget to delete all the entries and notify listeners
		// about each one of them!
		rootDir.reset();
	}

	void DirectoryTree::AddNewFile(const std::filesystem::path& filePath)
	{
		std::filesystem::path parentDirRelPath = filePath.parent_path();

		std::shared_ptr<Directory> parentDir = GetDirectory(parentDirRelPath);
		assert(parentDir && "Can't add a file into a directory that doesn't exist");

		//std::shared_ptr<File> newFile = std::make_shared<File>(
		//	filePath,
		//	DetectFileAssetType(filePath.extension().generic_string()));
		std::shared_ptr<File> newFile = CreateFile(filePath);

		Directory::AddFileToDirectory(parentDir, newFile);

		NotifyFileAdded(newFile);
	}
	void DirectoryTree::AddNewDirectory(const std::filesystem::path& dirPath)
	{
		std::filesystem::path parentDirRelPath = dirPath.parent_path();

		std::shared_ptr<Directory> parentDir = GetDirectory(parentDirRelPath);
		assert(parentDir && "Can't add a directory into a directory that doesn't exist");

		std::shared_ptr<Directory> newDir = BuildTree(dirPath);

		Directory::AddDirectoryToDirectory(parentDir, newDir);

		NotifyDirectoryAdded(newDir);
	}

	void DirectoryTree::RemoveFile(const std::filesystem::path& filePath)
	{
		std::filesystem::path parentDirRelPath = filePath.parent_path();

		std::shared_ptr<Directory> parentDir = GetDirectory(parentDirRelPath);
		assert(parentDir && "Can't remove a file from a directory that doesn't exist");

		std::shared_ptr<File> fileToDelete =
			parentDir->GetFile(filePath.filename().generic_string());
		if (!fileToDelete)
			return;

		// This place is really important because it'll have direct impact on how
		// we're going to create tasks and notify the 'TaskManager' about them.
		// So, Here's the thing.
		// If we want listeners to know what entities we're deleting and
		// the information about them should be 'old', (before changes after deleting them will take place)
		// then we have to first notify the entities and then actually delete them
		// If we want the opposite result, when we'd like to be notified about these entities
		// with new information (after the delete operation), then change the order of operations.

		NotifyFileRemoved(fileToDelete);

		parentDir->DeleteFile(fileToDelete);
	}
	void DirectoryTree::RemoveDirectory(const std::filesystem::path& dirPath)
	{
		std::filesystem::path parentDirRelPath = dirPath.parent_path();

		std::shared_ptr<Directory> parentDir = GetDirectory(parentDirRelPath);
		assert(parentDir && "Can't remove a directory from a directory that doesn't exist");

		std::shared_ptr<Directory> dirToDelete =
			parentDir->GetDirectory(dirPath.filename().generic_string());
		if (!dirToDelete)
			return;

		// This place is really important because it'll have direct impact on how
		// we're going to create tasks and notify the 'TaskManager' about them.
		// So, Here's the thing.
		// If we want listeners to know what entities we're deleting and
		// the information about them should be 'old', (before changes after deleting them will take place)
		// then we have to first notify the entities and then actually delete them
		// If we want the opposite result, when we'd like to be notified about these entities
		// with new information (after the delete operation), then change the order of operations.

		for (auto& dirEntity : dirToDelete->GetDirEntriesRecursive())
		{
			if (dirEntity->IsDirectory())
			{
				directories.erase(dirEntity->GetPath());
				NotifyDirectoryRemoved(std::static_pointer_cast<Directory>(dirEntity));
			}
			else
			{
				NotifyFileRemoved(std::static_pointer_cast<File>(dirEntity));
			}
		}

		parentDir->DeleteDirectory(dirToDelete);
	}

	void DirectoryTree::MoveFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();
		std::filesystem::path newPathParentPath = newPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		std::shared_ptr<Directory> newPathParentDir = GetDirectory(newPathParentPath);

		assert(oldPathParentDir && newPathParentDir && "The old or new directory doesn't exist");

		std::shared_ptr<File> fileToMove = oldPathParentDir->GetFile(oldPath.filename().generic_string());
		
		assert(fileToMove && "Can't move a file that doesn't exist");

		oldPathParentDir->DeleteFile(fileToMove);
		Directory::AddFileToDirectory(newPathParentDir, fileToMove);
	}
	void DirectoryTree::MoveDirectory(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();
		std::filesystem::path newPathParentPath = newPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		std::shared_ptr<Directory> newPathParentDir = GetDirectory(newPathParentPath);

		assert(oldPathParentDir && newPathParentDir && "The old or new directory doesn't exist");

		std::shared_ptr<Directory> directoryToMove = oldPathParentDir->GetDirectory(oldPath.filename().generic_string());

		assert(directoryToMove && "Cannot move a directory that doesn't exist");

		// First, we have to resolve connections of the directorie's entities
		// with their respective old paths. So, basically we have to assing
		// new keys to the entities inside the directory being moved

		// Yeah, naming... Hope I can find better wording here...
		std::filesystem::path relPathToMovedDir = oldPath.filename();
		// ResolveMovedDirectoryMapLinks(newPathParentPath, relPathToMovedDir, directoryToMove);
		ResolveDirectoryMapLinks(newPath, std::filesystem::path{}, directoryToMove);

		oldPathParentDir->DeleteDirectory(directoryToMove);
		Directory::AddDirectoryToDirectory(newPathParentDir, directoryToMove);
	}

	void DirectoryTree::ProcessModifiedFile(const std::filesystem::path& oldPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		assert(oldPathParentDir && "The directory where the modified file should be doesn't exist");

		std::shared_ptr<File> modifiedFile = oldPathParentDir->GetFile(oldPath.filename().generic_string());
		assert(modifiedFile && "Modified file doesn't exist");

		modifiedFile->UpdateStatus(rootDirAbsParentPath);
		if (modifiedFile->Modified())
		{
			NotifyFileModified(modifiedFile);
		}
	}
	void DirectoryTree::ProcessModifiedDirectory(const std::filesystem::path& oldPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		assert(oldPathParentDir && "The directory where the modified directory should be doesn't exist");

		std::shared_ptr<Directory> modifiedDirectory = oldPathParentDir->GetDirectory(oldPath.filename().generic_string());
		assert(modifiedDirectory && "Modified directory doesn't exist");

		modifiedDirectory->UpdateStatus(rootDirAbsParentPath);
		if (modifiedDirectory->Modified())
		{
			NotifyDirectoryModified(modifiedDirectory);
		}
	}

	void DirectoryTree::RenameFile(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();
		std::filesystem::path newPathParentPath = newPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		std::shared_ptr<Directory> newPathParentDir = GetDirectory(newPathParentPath);

		assert(oldPathParentDir && newPathParentDir && "The old or new directory doesn't exist");

		std::shared_ptr<File> fileToRename = oldPathParentDir->GetFile(oldPath.filename().generic_string());

		assert(fileToRename && "Can't rename a file that doesn't exist");

		fileToRename->Rename(newPath.filename().generic_string());
	}
	void DirectoryTree::RenameDirectory(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
	{
		std::filesystem::path oldPathParentPath = oldPath.parent_path();
		std::filesystem::path newPathParentPath = newPath.parent_path();

		std::shared_ptr<Directory> oldPathParentDir = GetDirectory(oldPathParentPath);
		std::shared_ptr<Directory> newPathParentDir = GetDirectory(newPathParentPath);

		assert(oldPathParentDir && newPathParentDir && "The old or new directory doesn't exist");

		std::shared_ptr<Directory> dirToRename = oldPathParentDir->GetDirectory(oldPath.filename().generic_string());

		assert(dirToRename && "Can't rename a directory that doesn't exist");

		ResolveDirectoryMapLinks(newPath, std::filesystem::path{}, dirToRename);

		dirToRename->Rename(newPath.filename().generic_string());
	}

	void DirectoryTree::ProcessDirectoryTree(DirectoryTreeProcessor* processor)
	{
		if (rootDir)
			processor->ProcessDirectoryTree(rootDir);
	}

	std::shared_ptr<Directory> DirectoryTree::GetDirectory(const std::filesystem::path& dirPath) const
	{
		auto find = directories.find(dirPath);
		if (find == directories.end())
			return std::shared_ptr<Directory>{};

		return find->second;
	}

	std::shared_ptr<Directory> DirectoryTree::GetRootDirectory() const
	{
		return rootDir;
	}

	void DirectoryTree::NotifyDirectoryAdded(std::shared_ptr<Directory> dir)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[dir](DirectoryTreeEventListener* listener) {
				listener->OnDirectoryAdded(dir);
		});
	}
	void DirectoryTree::NotifyDirectoryRemoved(std::shared_ptr<Directory> dir)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[dir](DirectoryTreeEventListener* listener) {
				listener->OnDirectoryRemoved(dir);
		});
	}
	void DirectoryTree::NotifyDirectoryModified(std::shared_ptr<Directory> dir)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[dir](DirectoryTreeEventListener* listener) {
				listener->OnDirectoryModified(dir);
			});
	}

	void DirectoryTree::NotifyFileAdded(std::shared_ptr<File> file)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[file](DirectoryTreeEventListener* listener) {
				listener->OnFileAdded(file);
		});
	}
	void DirectoryTree::NotifyFileRemoved(std::shared_ptr<File> file)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[file](DirectoryTreeEventListener* listener) {
				listener->OnFileRemoved(file);
		});
	}
	void DirectoryTree::NotifyFileModified(std::shared_ptr<File> file)
	{
		std::for_each(
			listeners.begin(), listeners.end(),
			[file](DirectoryTreeEventListener* listener) {
				listener->OnFileModified(file);
			});
	}

	std::shared_ptr<File> DirectoryTree::CreateFile(const std::filesystem::path& relPath) const
	{
		std::shared_ptr<File> newFile = std::make_shared<File>(relPath);
		newFile->UpdateStatus(rootDirAbsParentPath); // Set 'lastWriteTime' variable
		newFile->UpdateStatus(rootDirAbsParentPath); // Reset 'modified' to false
		return newFile;
	}
	std::shared_ptr<Directory> DirectoryTree::CreateDirectory(const std::filesystem::path& relPath) const
	{
		std::shared_ptr<Directory> newDir = std::make_shared<Directory>(relPath);
		newDir->UpdateStatus(rootDirAbsParentPath); // Set 'lastWriteTime' variable
		newDir->UpdateStatus(rootDirAbsParentPath); // Reset 'modified' to false
		return newDir;
	}

	std::shared_ptr<Directory> DirectoryTree::BuildTree(const std::filesystem::path& parentDirPath)
	{
		// std::shared_ptr<Directory> parentDir = std::make_shared<Directory>(parentDirPath);
		std::shared_ptr<Directory> parentDir = CreateDirectory(parentDirPath);
		directories.insert({ parentDirPath, parentDir });
		
		for (const auto& entry : std::filesystem::directory_iterator{ rootDirAbsParentPath / parentDirPath })
		{
			if (entry.is_regular_file())
			{
				std::filesystem::path fileName = entry.path().filename();

				// std::shared_ptr<File> newFile = std::make_shared<File>(parentDirPath / fileName, assetType);
				std::shared_ptr<File> newFile = CreateFile(parentDirPath / fileName);

				Directory::AddFileToDirectory(parentDir, newFile);

				NotifyFileAdded(newFile);
			}
			if (entry.is_directory())
			{
				std::filesystem::path dirName = entry.path().filename();
				std::shared_ptr<Directory> newDir = BuildTree(parentDirPath / dirName);

				Directory::AddDirectoryToDirectory(parentDir, newDir);

				NotifyDirectoryAdded(newDir);
			}
		}

		return parentDir;
	}

	void DirectoryTree::ResolveDirectoryMapLinks(
		const std::filesystem::path& newDirPath,
		const std::filesystem::path& relPathToDir,
		std::shared_ptr<Directory> relPathDir)
	{
		std::filesystem::path newKeyPath = newDirPath;
		if (!relPathToDir.empty())
			newKeyPath = newDirPath / relPathToDir;

		auto search = directories.find(relPathDir->GetPath());
		assert(search != directories.end() && "Can't find the directory with the old key");
		directories.erase(search);

		directories.insert({ newKeyPath, relPathDir });

		for (auto& dir : relPathDir->GetDirectories())
		{
			auto search = directories.find(dir->GetPath());
			assert(search != directories.end() && "Can't find the directory currently being iterated over in the map");
			std::shared_ptr<Directory> newRelPathDir = search->second;

			ResolveDirectoryMapLinks(newDirPath, relPathToDir / dir->GetPath().filename(), newRelPathDir);
		}
	}
}