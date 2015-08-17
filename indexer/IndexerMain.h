#pragma once

#include <lightspeed/base/framework/app.h>
#include <lightspeed/utils/FilePath.h>
#include "lightspeed/base/containers/set.h"
namespace SourceIndex {

	using namespace LightSpeed;

	class DocIndex;
	class WordIndex;

	class IndexerMain : public App
	{
	public:
		IndexerMain();
		~IndexerMain();

		enum ReindexMode {
			///add new files, do not reindex existing files (faster)
			rmAdd,
			///merge new files with files in the index - also reindex changed files
			rmMerge,
			///remove files which are not in the list
			rmFilter,
			///erase files
			rmErase,
			///force reindex files - they will be reindex even if they are marked unchanged
			rmReindex
		};

		virtual integer start(const Args &args) override;
		void indexFile(ConstStrW args);
		void indexFiles(ConstStrW fileList, ReindexMode mode);
		void doCleanup();
		void doHelp();
		void initPaths(FilePath  path);
		bool refreshIndex();
		bool refreshIndex(Set<ConstStrW> &fileset, ReindexMode mode);
		void reindexPath(FilePath docIndexFld);
		bool addFileToIndex(DocIndex &didx, WordIndex &widx, ConstStrW fname);

	protected:
		FilePath wordIndexFld;
		FilePath wordListFile;
		FilePath docIndexFld;
		FilePath deletedDocsFile;
	};



}
