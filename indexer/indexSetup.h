/*
 * indexSetup.h
 *
 *  Created on: 18. 8. 2015
 *      Author: ondra
 */

#pragma once
#include <lightspeed/base/timestamp.h>
#include "lightspeed/utils/FilePath.h"

#include "string.h"

#include "lightspeed/base/containers/map.h"

#include "lightspeed/base/streams/fileio.h"
namespace SourceIndex {
using namespace LightSpeed;


class IndexSetup {
public:
	IndexSetup();
	virtual ~IndexSetup();


	struct Entry {
		FilePath rootPath;
		bool includeHidden;
		bool followLinks;
		String extensions;
		Map<ConstStrA, bool> extensionFilter;
		TimeStamp lastUpdate;
	};

	typedef AutoArray<Entry> PathSetup;


	void loadSetup(SeqFileInput file);
	void saveSetup(SeqFileOutput file) const;

	struct FileState {
		bool changed;
		bool indexIt;
	};

	typedef Map<String, FileState> FileList;


	FileList generateFileList() const;


protected:
	PathSetup pathSetup;
};


}

