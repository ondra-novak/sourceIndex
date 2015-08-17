#pragma once
#include "lightspeed/utils/FilePath.h"
#include "lightspeed/base/streams/fileio_ifc.h"
#include "lightspeed/base/containers/stringpool.h"
#include "tokenizer.h"
namespace SourceIndex {

	using namespace LightSpeed;


	class OpenedWordIndexFile;

	class WordIndex
	{
	public:
		WordIndex(const FilePath &path, const FilePath &wordList);
		void checkPath();
		void init();

		void addToIndex(const TokenizedDocument &doc);
		static void writeDocHeader(SeqFileOutput wordIndex, DocID docId, Bin::natural16 count);
		SeqFileOutput openWordIndexForWrite(WordID wid);

		OpenedWordIndexFile openWordIndexForRead(WordID wid);

		FilePath getWordIndexPathname(WordID wid);
		static void writeMatchHit(SeqFileOutput &wordIndex, const WordMatch &docIncrement);



		template<typename Fn>
		bool enumIndex(Fn fn);
	protected:
		FilePath path;
		FilePath wordList;
		IFileIOServices &svc;
		StringPoolA wordListPool;
		Set<StringPoolA::Str> wordListSet;

		class IEnumIndexCb {
		public:
			virtual bool operator()(OpenedWordIndexFile &windex) const = 0;
		};

		bool enumIndexVt(const IEnumIndexCb &cb);
		template<typename PFI>
		bool enumIndexVtSt(PFI iter, const IEnumIndexCb &cb);
	};

	template<typename Fn>
	bool SourceIndex::WordIndex::enumIndex(Fn fn)
	{
		class Cb : public IEnumIndexCb {
		public:
			Cb(Fn fn) :fn(fn) {}
			virtual bool operator()(OpenedWordIndexFile &windex) const {
				return fn(windex);
			}
			Fn fn;
		};
		return enumIndexVt(Cb(fn));
	}

	class OpenedWordIndexFile {
	public:

		OpenedWordIndexFile(const FilePath &wordIdxFile, IFileIOServices &svc);


		template<typename Fn>
		bool enumDocuments(Fn fn);

		void deleteWord();
		void close();
		SeqFileOutput openWordIndexForCleanup();
		static void copyDocRecord(SeqFileOutput &wordIndex, DocID docId, Bin::natural16 count, const WordMatch *matches);

	protected:
		PMappedFile mappedFile;
		natural fsize;
		FilePath wordIdxFile;
		IFileIOServices &svc;

		class IEnumDocsCallback {
		public:
			virtual bool operator()(const DocID &docId, Bin::natural16 count, const WordMatch *matches)const = 0;
		};

		bool enumDocumentsVt(const IEnumDocsCallback &cb);

	};

	template<typename Fn>
	bool SourceIndex::OpenedWordIndexFile::enumDocuments(Fn fn)
	{
		class Cb : public IEnumDocsCallback {
		public:
			Cb(Fn fn) :fn(fn) {}
			virtual bool operator()(const DocID &docId, Bin::natural16 count, const WordMatch *matches) const {
				return fn(docId, count, matches);
			}
			Fn fn;
		};
		return enumDocumentsVt(Cb(fn));
	}


}