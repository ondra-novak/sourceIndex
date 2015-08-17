#include "IndexerMain.h"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "tokenizer.h"
#include "wordIndex.h"
#include "docIndex.h"
#include "lightspeed/base/streams/standardIO.tcc"
#include "lightspeed/base/debug/dbglog.h"



namespace SourceIndex {

	using namespace LightSpeed;

	static IndexerMain theApp;

	IndexerMain::IndexerMain()
	{
	}


	IndexerMain::~IndexerMain()
	{
	}

	LightSpeed::integer IndexerMain::start(const Args &args)
	{
		ConsoleA console;

		if (args.length() < 3) throw ErrorMessageException(THISLOCATION, "need: <path to index> <command> <params>. Try 'help' as command ");
		initPaths(FilePath(this->appPathname) / args[1] / nil);

		if (args[2] == L"file") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need pathname");
			indexFile(args[3]);
		}
		else if (args[2] == L"refresh") {
			refreshIndex();
			doCleanup();

		}
		else if (args[2] == L"cleanup") {
			doCleanup();
		}
		else if (args[2] == L"help") {
			doHelp();
		}
		else {
			throw ErrorMessageException(THISLOCATION, "Unknown command, try help as command");
		}
		return 0;
	}

	void IndexerMain::indexFile(ConstStrW file)
	{
		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		FileInfo finfo = FileInfo::getFileInfo(file);
		TokenizedDocument doc;
		SeqFileInput f(finfo.fullPath, 0);
		doc.tokenizePlainText(f);
		if (doc.binaryFile) throw ErrorMessageException(THISLOCATION, "Rejected - binary file");

		didx.init();

		if (!didx.isInIndex(doc.docId)) {
			widx.init();
			widx.addToIndex(doc);
		}
		DocIndex::DocumentMeta dmeta;
		didx.openDocument(doc.docId, dmeta);
		didx.addFile(dmeta, finfo);
		didx.updateDocument(dmeta);
	}

	void IndexerMain::doCleanup()
	{
		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		Map<DocID, bool> docCache;

		didx.init();
		widx.init();


		widx.enumIndex([&](OpenedWordIndexFile &index){


			
			bool needCleanup = false;
			bool anyRecord = false;

			index.enumDocuments([&](const DocID &docId, natural count, const WordMatch *wmatch) {

					const bool *cached = docCache.find(docId);
					bool exists;
					if (cached == 0) {
						exists = didx.isInIndex(docId);
						docCache.insert(docId, exists);
					}
					else {
						exists = *cached;
					}

					if (!exists) needCleanup = true; else anyRecord = true;
					

					return true;
			});

			if (!anyRecord) {
				//word has no record  - delete it
				index.deleteWord();
			} else if (needCleanup) {
				SeqFileOutput idx = index.openWordIndexForCleanup();
				SeqFileOutBuff<> bidx(idx);
				index.enumDocuments([&](const DocID &docId, Bin::natural16 count, const WordMatch *wmatch) {
					const bool *cached = docCache.find(docId);
					if (*cached) {
						index.copyDocRecord(bidx, docId, count, wmatch);
					}
					return true;
				});
				bidx.flush();
				index.close();
			}

			return true;			
		});
	}


	void IndexerMain::doHelp()
	{
	}


	void IndexerMain::initPaths(FilePath path)
	{
		wordIndexFld = path / "words" / nil;
		wordListFile = path / "wordlist.txt";
		docIndexFld = path / "docs" / nil;
		deletedDocsFile = path / "erased.dat";

	}

	void IndexerMain::refreshIndex()
	{
		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		didx.init();
		widx.init();

		didx.enumDocuments([&](const DocIndex::DocumentMeta &dmeta) {
			DocIndex::DocumentMeta newMeta;
			didx.initDocument(dmeta, newMeta);
			bool dirty = false;
			dmeta.enumFiles([&](const FileInfo &origFile){

				FileInfo nwFile = FileInfo::getFileInfo(origFile.fullPath);
				if (nwFile > origFile) {
					LS_LOG.debug("reindex %1") << nwFile.fullPath;
					TokenizedDocument doc;
					SeqFileInput f(nwFile.fullPath, OpenFlags::accessSeq | OpenFlags::shareRead);
					doc.tokenizePlainText(f);
					if (doc.binaryFile) {
						dirty = true;
						return true;
					}
					if (!didx.isInIndex(doc.docId)) {
						LS_LOG.debug("added to index %1") << nwFile.fullPath;
						widx.addToIndex(doc);
					}
					if (doc.docId == newMeta.docId) {
						LS_LOG.debug("same doc container %1 - %2") << nwFile.fullPath << newMeta.path;
						didx.addFile(newMeta, nwFile);
					} 
					else {
						dirty = true;

						DocIndex::DocumentMeta odmeta;
						didx.openDocument(doc.docId, odmeta);
						didx.addFile(odmeta, nwFile);
						didx.updateDocument(odmeta);
						LS_LOG.debug("new doc container %1 - %2") << nwFile.fullPath << odmeta.path;
					}
				}
				else {
					LS_LOG.debug("unchanged %1 - %2") << nwFile.fullPath;
					didx.addFile(newMeta, nwFile);
				}
				return true;
			});

			if (dirty)
				didx.updateDocument(newMeta);

			return true;
		});

	}

}
