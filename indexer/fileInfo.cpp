#include "fileInfo.h"
#include "lightspeed/base/streams/fileio_ifc.h"

namespace SourceIndex {

	

	FileInfo::FileInfo(FilePath fullPath, TimeStamp modified)
		:fullPath(fullPath), modified(modified)
	{

	}

	SourceIndex::FileInfo FileInfo::getFileInfo(ConstStrW fname)
	{
		IFileIOServices &svc = IFileIOServices::getIOServices();

		PFolderIterator iter = svc.getFileInfo(fname);
		return FileInfo(FilePath(iter->getFullPath(), false), iter->getModifiedTime());

	}

	bool FileInfo::lessThan(const FileInfo &nfo) const
	{
		return this->modified < nfo.modified;
	}

}