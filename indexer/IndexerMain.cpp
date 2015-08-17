#include "IndexerMain.h"
#include "lightspeed/base/exceptions/errorMessageException.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "tokenizer.h"
#include "wordIndex.h"
#include "docIndex.h"
#include "lightspeed/base/streams/standardIO.tcc"
#include "lightspeed/base/debug/dbglog.h"

#include "lightspeed/utils/base64.tcc"
#include "lightspeed/base/containers/convertString.h"

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
		else if (args[2] == L"merge") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need file with files to merge");
			indexFiles(args[3],rmMerge);
		} else if (args[2] == L"add") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need file with files to add");
			indexFiles(args[3],rmAdd);
		} else if (args[2] == L"reindex") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need file with files to reindex");
			indexFiles(args[3],rmReindex);
		} else if (args[2] == L"filter") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need file with files to filter");
			indexFiles(args[3],rmFilter);
		} else if (args[2] == L"erase") {
			if (args.length() < 4) throw ErrorMessageException(THISLOCATION, "need file with files to erase");
			indexFiles(args[3],rmErase);
		}else if (args[2] == L"refresh") {
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
		LS_LOGOBJ(lg);

		lg.progress("Running cleanup");
		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		Map<DocID, bool> docCache;

		didx.init();
		widx.init();


		widx.enumIndex([&](OpenedWordIndexFile &index){


			
			bool needCleanup = false;
			bool anyRecord = false;

			index.enumDocuments([&](const DocID &docId, natural count, const WordMatch *wmatch) {

				LS_LOGOBJ(lg);

					const bool *cached = docCache.find(docId);
					bool exists;
					if (cached == 0) {
						exists = didx.isInIndex(docId);
						docCache.insert(docId, exists);
						if (!exists) lg.progress("Deleted document: %1") << convertString(Base64Encoder(true,true),ConstBin(docId));
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

	bool IndexerMain::addFileToIndex(DocIndex &didx, WordIndex &widx, ConstStrW fname) {
		TokenizedDocument doc;
		FileInfo finfo = FileInfo::getFileInfo(fname);
		SeqFileInput f(finfo.fullPath, 0);
		doc.tokenizePlainText(f);
		if (!doc.binaryFile) {

			if (!didx.isInIndex(doc.docId)) widx.addToIndex(doc);

			DocIndex::DocumentMeta dmeta;
			didx.openDocument(doc.docId, dmeta);
			didx.addFile(dmeta, finfo);
			didx.updateDocument(dmeta);
			return true;
		} else{
			return false;
		}


	}

	void IndexerMain::indexFiles(ConstStrW fileList, ReindexMode mode) {

		LS_LOGOBJ(lg);

		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		didx.init();
		widx.init();

		StringPoolW pool;
		AutoArray<StringPoolW::Str> filearray;

		SeqFileInBuff<> input(fileList,0);
		ScanTextW scan(input);
		while (scan.readLine()) {
			ConstStrW line = scan[1].str();
			if (!line.empty()) {
				FileInfo finfo = FileInfo::getFileInfo(line);
				filearray.add(pool.add(finfo.fullPath));
			}
		}

		Set<ConstStrW> fileset;
		for (natural i = 0; i < filearray.length(); i++)
			fileset.insert(filearray[i]);

		bool runCleanup = false;

		if (mode != rmAdd) {
			runCleanup |= refreshIndex(fileset,mode);
		}

		if (mode != rmErase) {
			for (Set<ConstStrW>::Iterator iter = fileset.getFwIter(); iter.hasItems();) {
				ConstStrW fname = iter.getNext();
				try {
					if (addFileToIndex(didx,widx,fname))
						lg.progress("Processed: %1") << fname;
					else
						lg.warning("Binary file skipped: %1") << fname;
				} catch (std::exception &e) {
					lg.warning("Skipping: %1 because %2") << fname << e.what();
				}
			}
		}

		if (runCleanup) {
			doCleanup();
		}



	}

	bool IndexerMain::refreshIndex() {
		Set<ConstStrW> fileset;
		return refreshIndex(fileset,rmMerge);

	}


	bool IndexerMain::refreshIndex(Set<ConstStrW> &fileset, ReindexMode mode)
	{
		bool didErase = false;
		DocIndex didx(docIndexFld);
		WordIndex widx(wordIndexFld, wordListFile);

		didx.init();
		widx.init();

		didx.enumDocuments([&](const DocIndex::DocumentMeta &dmeta) {
			DocIndex::DocumentMeta newMeta;
			didx.initDocument(dmeta, newMeta);
			bool dirty = false;
			dmeta.enumFiles([&](const FileInfo &origFile){

				bool reindexThis = false;

				switch (mode) {
					case rmFilter: if (fileset.find(origFile.fullPath) == 0) return true;
								break;
					case rmErase:  if (fileset.find(origFile.fullPath) != 0) return true;
								break;
					case rmReindex: reindexThis =  (fileset.find(origFile.fullPath) != 0);break;
					default: break;
				}

				fileset.erase(origFile.fullPath);

				FileInfo nwFile = FileInfo::getFileInfo(origFile.fullPath);
				if (nwFile > origFile || reindexThis) {
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
				didErase |= !didx.updateDocument(newMeta);

			return true;
		});

		return didErase;
	}

}
