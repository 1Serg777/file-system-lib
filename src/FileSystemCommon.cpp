#include "../include/FileSystemCommon.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace fs
{
	// Comparators

	//constexpr auto fileAlphabeticalCompLH = [](std::shared_ptr<File> file1, std::shared_ptr<File> file2)
	//{
	//	return file1->GetFullFileName() < file2->GetFullFileName();
	//};
	//constexpr auto fileAlphabeticalCompHL = [](std::shared_ptr<File> file1, std::shared_ptr<File> file2)
	//{
	//	return file1->GetFullFileName() > file2->GetFullFileName();
	//};

	// File Event

	FileEvent FileEvent::CreateAddedEvent(const std::filesystem::path& newPath)
	{
		FileEvent addedEvent{};
		addedEvent.type = FileEventType::ADDED;
		addedEvent.newPath = newPath;
		return addedEvent;
	}
	FileEvent FileEvent::CreateRemovedEvent(const std::filesystem::path& oldPath)
	{
		FileEvent removedEvent{};
		removedEvent.type = FileEventType::REMOVED;
		removedEvent.oldPath = oldPath;
		return removedEvent;
	}
	FileEvent FileEvent::CreateMovedEvent(
		const std::filesystem::path& oldPath,
		const std::filesystem::path& newPath)
	{
		FileEvent movedEvent{};
		movedEvent.type = FileEventType::MOVED;
		movedEvent.oldPath = oldPath;
		movedEvent.newPath = newPath;
		return movedEvent;
	}
	FileEvent FileEvent::CreateModifiedEvent(const std::filesystem::path& oldPath)
	{
		FileEvent modifiedEvent{};
		modifiedEvent.type = FileEventType::MODIFIED;
		modifiedEvent.oldPath = oldPath;
		return modifiedEvent;
	}
	FileEvent FileEvent::CreateRenamedEvent(
		const std::filesystem::path& oldPath,
		const std::filesystem::path& newPath)
	{
		FileEvent renamedEvent{};
		renamedEvent.type = FileEventType::RENAMED;
		renamedEvent.oldPath = oldPath;
		renamedEvent.newPath = newPath;
		return renamedEvent;
	}

	// Directory Entry

	DirectoryEntry::DirectoryEntry(const std::filesystem::path& dirEntryPath)
		: dirEntryPath(dirEntryPath)
	{
	}

	void DirectoryEntry::Rename(const std::string& newName)
	{
		dirEntryPath.replace_filename(newName);
		UpdatePath();
	}

	const std::filesystem::path& DirectoryEntry::GetPath() const
	{
		return dirEntryPath;
	}
	void DirectoryEntry::UpdatePath()
	{
		std::filesystem::path newPath = dirEntryPath.filename();
		if (!parentDir.expired())
		{
			newPath = parentDir.lock()->GetPath() / newPath;
		}
		dirEntryPath = newPath;
	}

	void DirectoryEntry::SetParentDirectory(std::shared_ptr<Directory> parentDir)
	{
		this->parentDir = parentDir;
		UpdatePath();
	}
	void DirectoryEntry::ClearParentDirectory()
	{
		parentDir.reset();
		UpdatePath();
	}

	bool DirectoryEntry::Exists() const
	{
		return std::filesystem::exists(dirEntryPath);
	}

	void DirectoryEntry::UpdateStatus(const std::filesystem::path& absPart)
	{
		std::filesystem::path absPath = absPart / dirEntryPath;
		try
		{
			std::filesystem::file_time_type time = std::filesystem::last_write_time(absPath);
			if (time > lastWriteTime)
			{
				lastWriteTime = time;
				modified = true;
			}
			else
			{
				modified = false;
			}
		}
		catch (const std::filesystem::filesystem_error& err)
		{
			std::cerr << err.what() << "\n";
			modified = false;
		}
	}
	bool DirectoryEntry::Modified() const
	{
		return modified;
	}

	std::filesystem::file_time_type DirectoryEntry::GetLastWriteTime() const
	{
		return lastWriteTime;
	}

	// Directory

	void Directory::AddEntryToDirectory(std::shared_ptr<Directory> where, std::shared_ptr<DirectoryEntry> what)
	{
		where->AddDirectoryEntry(what);
		what->SetParentDirectory(where);
	}
	void Directory::AddDirectoryToDirectory(std::shared_ptr<Directory> where, std::shared_ptr<Directory> what)
	{
		where->AddDirectory(what);
		what->SetParentDirectory(where);
	}
	void Directory::AddFileToDirectory(std::shared_ptr<Directory> where, std::shared_ptr<File> what)
	{
		where->AddFile(what);
		what->SetParentDirectory(where);
	}

	Directory::Directory(const std::filesystem::path& dirPath)
		: DirectoryEntry(dirPath)
	{
		SetSortingType(sortType);
	}

	bool Directory::IsFile() const
	{
		return false;
	}
	bool Directory::IsDirectory() const
	{
		return true;
	}
	DirectoryEntryType Directory::GetDirectoryEntryType() const
	{
		return DirectoryEntryType::DIRECTORY;
	}
	std::string Directory::GetName() const
	{
		return GetDirectoryName();
	}

	void Directory::UpdatePath()
	{
		DirectoryEntry::UpdatePath();
		for (auto& file : files)
		{
			file->UpdatePath();
		}
		for (auto& dir : directories)
		{
			dir->UpdatePath();
		}
	}

	void Directory::AddDirectoryEntry(std::shared_ptr<DirectoryEntry> entry)
	{
		if (entry->GetDirectoryEntryType() == DirectoryEntryType::DIRECTORY)
		{
			AddDirectory(std::static_pointer_cast<Directory>(entry));
		}
		else if (entry->GetDirectoryEntryType() == DirectoryEntryType::FILE)
		{
			AddFile(std::static_pointer_cast<File>(entry));
		}
		else // UNDEFINED
		{
			assert(false && "Undefined directory entry type");
		}
	}
	void Directory::AddDirectory(std::shared_ptr<Directory> dir)
	{
		// directories.insert({ dir->GetDirectoryName(), dir });

		InsertDirectorySorted(dir);
	}
	void Directory::AddFile(std::shared_ptr<File> file)
	{
		// files.insert({ file->GetFileName(), file });

		InsertFileSorted(file);
	}

	void Directory::DeleteDirectoryEntry(std::shared_ptr<DirectoryEntry> entry)
	{
		if (entry->GetDirectoryEntryType() == DirectoryEntryType::DIRECTORY)
		{
			DeleteDirectory(std::static_pointer_cast<Directory>(entry));
		}
		else if (entry->GetDirectoryEntryType() == DirectoryEntryType::FILE)
		{
			DeleteFile(std::static_pointer_cast<File>(entry));
		}
		else // UNDEFINED
		{
			assert(false && "Undefined directory entry type");
		}
	}
	void Directory::DeleteDirectory(const std::string& dirName)
	{
		auto nameSearch = [&](const std::shared_ptr<Directory>& dir) {
			return dirName == dir->GetDirectoryName();
		};

		auto result = std::find_if(std::begin(directories), std::end(directories), nameSearch);
		if (result == std::end(directories))
			return;

		(*result)->ClearParentDirectory();

		directories.erase(result);
	}
	void Directory::DeleteDirectory(std::shared_ptr<Directory> dir)
	{
		directories.erase(
			std::remove(directories.begin(), directories.end(), dir),
			directories.end());

		dir->ClearParentDirectory();
	}
	void Directory::DeleteFile(const std::string& fileName)
	{
		auto nameSearch = [&](const std::shared_ptr<File>& file) {
			return fileName == file->GetFullFileName();
		};

		auto result = std::find_if(std::begin(files), std::end(files), nameSearch);
		if (result == std::end(files))
			return;

		(*result)->ClearParentDirectory();

		files.erase(result);
	}
	void Directory::DeleteFile(std::shared_ptr<File> file)
	{
		files.erase(
			std::remove(files.begin(), files.end(), file),
			files.end());

		file->ClearParentDirectory();
	}

	std::shared_ptr<Directory> Directory::GetDirectory(const std::string& dirName)
	{
		auto nameSearch = [&](const std::shared_ptr<Directory>& dir) {
			return dirName == dir->GetDirectoryName();
		};
		auto result = std::find_if(std::begin(directories), std::end(directories), nameSearch);

		if (result == std::end(directories))
			return std::shared_ptr<Directory>{};
		return *result;
	}
	std::shared_ptr<File> Directory::GetFile(const std::string& fileName)
	{
		auto nameSearch = [&](const std::shared_ptr<File>& file) {
			return fileName == file->GetFullFileName();
		};
		auto result = std::find_if(std::begin(files), std::end(files), nameSearch);
		
		if (result == std::end(files))
			return std::shared_ptr<File>{};
		return *result;
	}

	bool Directory::DirectoryExists(const std::string& dirName)
	{
		auto nameSearch = [&](std::shared_ptr<Directory> directory) {
			return dirName == directory->GetDirectoryName();
		};
		auto result = std::find_if(std::begin(directories), std::end(directories), nameSearch);

		if (result == std::end(directories))
			return false;
		return true;
	}
	bool Directory::FileExists(const std::string& fileName)
	{
		auto nameSearch = [&](std::shared_ptr<File> file) {
			return fileName == file->GetFullFileName();
		};
		auto result = std::find_if(std::begin(files), std::end(files), nameSearch);

		if (result == std::end(files))
			return false;
		return true;
	}

	std::string Directory::GetDirectoryName() const
	{
		return dirEntryPath.filename().string();
	}

	std::vector<std::shared_ptr<Directory>> Directory::GetDirectories() const
	{
		std::vector<std::shared_ptr<Directory>> dirs;
		dirs.insert(dirs.end(), std::begin(directories), std::end(directories));
		return dirs;
	}
	std::vector<std::shared_ptr<Directory>> Directory::GetDirectoriesRecursive() const
	{
		std::vector<std::shared_ptr<Directory>> result;
		for (const auto& dir : directories)
		{
			result.push_back(dir);
			auto dirEntries = dir->GetDirectoriesRecursive();
			result.insert(result.end(), std::begin(dirEntries), std::end(dirEntries));
		}
		return result;
	}
	std::vector<std::shared_ptr<File>> Directory::GetFiles() const
	{
		std::vector<std::shared_ptr<File>> files;
		files.insert(files.end(), std::begin(this->files), std::end(this->files));
		return files;
	}
	std::vector<std::shared_ptr<File>> Directory::GetFilesRecursive() const
	{
		std::vector<std::shared_ptr<File>> result;
		result.insert(result.end(), std::begin(files), std::end(files));
		for (const auto& dir : directories)
		{
			auto dirEntries = dir->GetFilesRecursive();
			result.insert(result.end(), std::begin(dirEntries), std::end(dirEntries));
		}
		return result;
	}

	std::vector<std::shared_ptr<DirectoryEntry>> Directory::GetDirEntries() const
	{
		std::vector<std::shared_ptr<DirectoryEntry>> entries;
		entries.insert(entries.end(), std::begin(files), std::end(files));
		entries.insert(entries.end(), std::begin(directories), std::end(directories));
		return entries;
	}
	std::vector<std::shared_ptr<DirectoryEntry>> Directory::GetDirEntriesRecursive() const
	{
		std::vector<std::shared_ptr<DirectoryEntry>> result;
		result.insert(result.end(), std::begin(files), std::end(files));
		for (const auto& dir : directories)
		{
			result.push_back(dir);
			auto dirEntries = dir->GetDirEntriesRecursive();
			result.insert(result.end(), std::begin(dirEntries), std::end(dirEntries));
		}
		return result;
	}

	DirEntrySortType Directory::GetSortingType() const
	{
		return sortType;
	}
	void Directory::SetSortingType(DirEntrySortType sortType)
	{
		this->sortType = sortType;
		switch (this->sortType)
		{
		case DirEntrySortType::ALPHABETICAL_L_TO_H:
			sorter = std::make_shared<AlphabeticalSorter>();
			sorter->SetSortingCompFun(Sorter::SortCompFun::LESS);
			break;
		case DirEntrySortType::ALPHABETICAL_H_TO_L:
			sorter = std::make_shared<AlphabeticalSorter>();
			sorter->SetSortingCompFun(Sorter::SortCompFun::GREATER);
			break;
		case DirEntrySortType::LAST_WRITE_TIME_L_TO_H:
			sorter = std::make_shared<LastWriteTimeSorter>();
			sorter->SetSortingCompFun(Sorter::SortCompFun::LESS);
			break;
		case DirEntrySortType::LAST_WRITE_TIME_H_TO_L:
			sorter = std::make_shared<LastWriteTimeSorter>();
			sorter->SetSortingCompFun(Sorter::SortCompFun::GREATER);
			break;
		}
		SortFiles();
		SortDirectories();
	}

	void Directory::SortDirectories()
	{
		sorter->SortDirectories(directories);
	}
	void Directory::SortFiles()
	{
		sorter->SortFiles(files);
	}

	void Directory::InsertDirectorySorted(std::shared_ptr<Directory> dir)
	{
		sorter->InsertDirectorySorted(dir, directories);
	}
	void Directory::InsertFileSorted(std::shared_ptr<File> file)
	{
		sorter->InsertFileSorted(file, files);
	}

	// File

	File::File(const std::filesystem::path& filePath)
		: DirectoryEntry(filePath)
	{
	}

	bool File::IsFile() const
	{
		return true;
	}
	bool File::IsDirectory() const
	{
		return false;
	}
	DirectoryEntryType File::GetDirectoryEntryType() const
	{
		return DirectoryEntryType::FILE;
	}
	std::string File::GetName() const
	{
		return GetFullFileName();
	}

	void File::UpdatePath()
	{
		DirectoryEntry::UpdatePath();
	}

	std::string File::GetFullFileName() const
	{
		return dirEntryPath.filename().string();
	}
	std::string File::GetFileName() const
	{
		std::filesystem::path fileName{ dirEntryPath };
		return fileName.filename().replace_extension().string();
	}
	std::string File::GetFileExtension() const
	{
		return dirEntryPath.extension().string();
	}

	// Sorters

	void Sorter::SetSortingCompFun(SortCompFun comp)
	{
		this->comp = comp;
	}
	Sorter::SortCompFun Sorter::GetSortingCompFun() const
	{
		return comp;
	}

	// AlphabeticalSorter

	void AlphabeticalSorter::SortDirectories(
		std::vector<std::shared_ptr<Directory>>& directories)
	{
		auto comp = [this](std::shared_ptr<Directory> dir1, std::shared_ptr<Directory> dir2)
		{
			if (GetSortingCompFun() == SortCompFun::LESS)
			{
				return std::less<std::string>{}(dir1->GetDirectoryName(), dir2->GetDirectoryName());
			}
			else if (GetSortingCompFun() == SortCompFun::GREATER)
			{
				return std::greater<std::string>{}(dir1->GetDirectoryName(), dir2->GetDirectoryName());
			}
			else
			{
				return false;
			}
		};
		std::sort(directories.begin(), directories.end(), comp);
	}
	void AlphabeticalSorter::SortFiles(
		std::vector<std::shared_ptr<File>>& files)
	{
		auto comp = [this](std::shared_ptr<File> file1, std::shared_ptr<File> file2)
		{
			if (GetSortingCompFun() == SortCompFun::LESS)
			{
				return std::less<std::string>{}(file1->GetFullFileName(), file2->GetFullFileName());
			}
			else if (GetSortingCompFun() == SortCompFun::GREATER)
			{
				return std::greater<std::string>{}(file1->GetFullFileName(), file2->GetFullFileName());
			}
			else
			{
				return false;
			}
		};
		std::sort(files.begin(), files.end(), comp);
	}

	void AlphabeticalSorter::InsertDirectorySorted(
		std::shared_ptr<Directory> dir,
		std::vector<std::shared_ptr<Directory>>& directories)
	{
		auto place = std::upper_bound(
			directories.begin(), directories.end(), dir->GetDirectoryName(),
			[this](const std::string& dirName, std::shared_ptr<Directory> dir)
			{
				if (GetSortingCompFun() == SortCompFun::LESS)
				{
					return std::less<std::string>{}(dirName, dir->GetDirectoryName());
				}
				else if (GetSortingCompFun() == SortCompFun::GREATER)
				{
					return std::greater<std::string>{}(dirName, dir->GetDirectoryName());
				}
				else
				{
					return false;
				}
			});
		directories.insert(place, dir);
	}
	void AlphabeticalSorter::InsertFileSorted(
		std::shared_ptr<File> file,
		std::vector<std::shared_ptr<File>>& files)
	{
		auto place = std::upper_bound(
			files.begin(), files.end(), file->GetFullFileName(),
			[this](const std::string& fileName, std::shared_ptr<File> file)
			{
				if (GetSortingCompFun() == SortCompFun::LESS)
				{
					return std::less<std::string>{}(fileName, file->GetFullFileName());
				}
				else if (GetSortingCompFun() == SortCompFun::GREATER)
				{
					return std::greater<std::string>{}(fileName, file->GetFullFileName());
				}
				else
				{
					return false;
				}
			});
		files.insert(place, file);
	}

	// LastWriteTimeSorter

	void LastWriteTimeSorter::SortDirectories(
		std::vector<std::shared_ptr<Directory>>& directories)
	{
		auto comp = [this](std::shared_ptr<Directory> dir1, std::shared_ptr<Directory> dir2)
		{
			if (GetSortingCompFun() == SortCompFun::LESS)
			{
				return std::less<std::filesystem::file_time_type>{}(dir1->GetLastWriteTime(), dir2->GetLastWriteTime());
			}
			else if (GetSortingCompFun() == SortCompFun::GREATER)
			{
				return std::greater<std::filesystem::file_time_type>{}(dir1->GetLastWriteTime(), dir2->GetLastWriteTime());
			}
			else
			{
				return false;
			}
		};
		std::sort(directories.begin(), directories.end(), comp);
	}
	void LastWriteTimeSorter::SortFiles(
		std::vector<std::shared_ptr<File>>& files)
	{
		auto comp = [this](std::shared_ptr<File> file1, std::shared_ptr<File> file2)
		{
			if (GetSortingCompFun() == SortCompFun::LESS)
			{
				return std::less<std::filesystem::file_time_type>{}(file1->GetLastWriteTime(), file2->GetLastWriteTime());
			}
			else if (GetSortingCompFun() == SortCompFun::GREATER)
			{
				return std::greater<std::filesystem::file_time_type>{}(file1->GetLastWriteTime(), file2->GetLastWriteTime());
			}
			else
			{
				return false;
			}
		};
		std::sort(files.begin(), files.end(), comp);
	}

	void LastWriteTimeSorter::InsertDirectorySorted(
		std::shared_ptr<Directory> directory,
		std::vector<std::shared_ptr<Directory>>& directories)
	{
		auto place = std::upper_bound(
			directories.begin(), directories.end(), directory->GetLastWriteTime(),
			[this](const std::filesystem::file_time_type& lastWriteTime, std::shared_ptr<Directory> dir)
			{
				if (GetSortingCompFun() == SortCompFun::LESS)
				{
					return std::less<std::filesystem::file_time_type>{}(lastWriteTime, dir->GetLastWriteTime());
				}
				else if (GetSortingCompFun() == SortCompFun::GREATER)
				{
					return std::greater<std::filesystem::file_time_type>{}(lastWriteTime, dir->GetLastWriteTime());
				}
				else
				{
					return false;
				}
			});
		directories.insert(place, directory);
	}
	void LastWriteTimeSorter::InsertFileSorted(
		std::shared_ptr<File> file,
		std::vector<std::shared_ptr<File>>& files)
	{
		auto place = std::upper_bound(
			files.begin(), files.end(), file->GetLastWriteTime(),
			[this](const std::filesystem::file_time_type& lastWriteTime, std::shared_ptr<File> file)
			{
				if (GetSortingCompFun() == SortCompFun::LESS)
				{
					return std::less<std::filesystem::file_time_type>{}(lastWriteTime, file->GetLastWriteTime());
				}
				else if (GetSortingCompFun() == SortCompFun::GREATER)
				{
					return std::greater<std::filesystem::file_time_type>{}(lastWriteTime, file->GetLastWriteTime());
				}
				else
				{
					return false;
				}
			});
		files.insert(place, file);
	}
}