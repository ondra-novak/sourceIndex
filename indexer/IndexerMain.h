#pragma once

#include <lightspeed/base/framework/app.h>
#include <lightspeed/utils/FilePath.h>

namespace SourceIndex {

	using namespace LightSpeed;

	class IndexerMain : public App
	{
	public:
		IndexerMain();
		~IndexerMain();

		virtual integer start(const Args &args) override;
		void indexFile(ConstStrW args);
		void doCleanup();
		void doHelp();
		void indexDirectory(const Args & args);
		void initPaths(FilePath  path);
		void refreshIndex();
		void reindexPath(FilePath docIndexFld);
	protected:
		FilePath wordIndexFld;
		FilePath wordListFile;
		FilePath docIndexFld;
		FilePath deletedDocsFile;
	};



}
