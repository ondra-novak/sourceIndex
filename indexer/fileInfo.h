#pragma once
#include "lightspeed\utils\FilePath.h"
#include "lightspeed\base\timestamp.h"
#include "lightspeed\base\compare.h"
namespace SourceIndex {

	using namespace LightSpeed;

	class FileInfo: public ComparableLess<FileInfo>
	{
	public:

		FilePath fullPath;
		TimeStamp modified;

		FileInfo(FilePath fullPath, TimeStamp modified);

		static FileInfo getFileInfo(ConstStrW fname);


		bool lessThan(const FileInfo &nfo) const;
		
	};


}