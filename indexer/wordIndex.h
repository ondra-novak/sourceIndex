#pragma once
#include "lightspeed/utils/FilePath.h"
#include "lightspeed/base/streams/fileio_ifc.h"
#include "lightspeed/base/containers/stringpool.h"

#include "lightspeed/base/containers/map.h"
#include "tokenizer.h"
#include "BinMap.h"
#include <xstddef>
namespace SourceIndex {

	using namespace LightSpeed;


	class OpenedWordIndexFile;

	class WordIndex
	{
	public:
		typedef Bin::natural32 Flags;
		//this word is begin part of target word
		const Flags flgWordBegin = 1;
		//this word is end part of target word
		const Flags flgWordEnd = 2;
		//this word is lowercase version of target word
		const Flags flgLowerCase = 4;

		WordIndex(const FilePath &path, const FilePath &wordIndexMap);
		void checkPath();
		void init();

		void commit();

		void addToIndex(const TokenizedDocument &doc);
		static void writeDocHeader(SeqFileOutput wordIndex, DocID docId, Bin::natural16 count);
		SeqFileOutput openWordIndexForWrite(WordID wid);

		OpenedWordIndexFile openWordIndexForRead(WordID wid) const;

		FilePath getWordIndexPathname(WordID wid) const;
		static void writeMatchHit(SeqFileOutput &wordIndex, const WordMatch &docIncrement);

		class IFindWordCB {
		public:
			virtual void foundWord(WordID word, Flags flags) = 0;
		};

		void findWords(WordID word, Flags flags, IFindWordCB *cb);


		template<typename Fn>
		bool enumIndex(Fn fn);


	protected:

		/*
		000 = word is substring of the target word
		001 = word is begin of the target word
		010 = word is end of the target word
		011 = word is the target word (never appear)
		100 = word is lowercase substring of the target word (caseInsensitive substring search)
		101 = word is lowercase begin of the target word
		110 = word is lowercase end of the target word
		111 = word is lowercase version of the target word

		note - if lowercase version of the word is equal to non-lowercase version, lowercase version is not stored.
				In case of caseInsensitive search query must be filled with both non-lowercase and lowercase versions
				targets.

		case insensitive search for substring - include results for all flags
		case insensitive search for begin - search 001 and 101
		case insensitive search for end - search 010 and 110
		case insensitive search for whole word - search 111
		case sensitive search for substring - search 000, 001, 010 and theoretically 011
		case sensitive search for begin - search for 001
		case sensitive search for end - search for 010
		case sensitive search for whole word - nothing, no need to search word map

		
		*/

		FilePath path;
		FilePath wordIndexFile;



		struct WordIndexKey: public ComparableLess<WordIndexKey> {
			WordID word;
			Flags flags;
			WordIndexKey(WordID word, Flags flags) :word(word), flags(flags) {}

			bool lessThan(const WordIndexKey &other) const {
				if (word == other.word) return flags < other.flags;
				else return word < other.word;
			}
		};


		IFileIOServices &svc;

		
		typedef BinMap<WordIndexKey, WordID, std::less<WordIndexKey> > WordIndexMap;
		typedef WordIndexMap::KeyValue KeyValue;
		typedef AutoArray<KeyValue> NewWordIndex;
		typedef Set<WordID> NewWordConstrain;

		WordIndexMap wordIndex;
		NewWordIndex newWordIndex;
		NewWordConstrain newWordConstrain;


		class IEnumIndexCb {
		public:
			virtual bool operator()(OpenedWordIndexFile &windex) const = 0;
		};


		bool enumIndexVt(const IEnumIndexCb &cb);
		template<typename PFI>
		bool enumIndexVtSt(PFI iter, const IEnumIndexCb &cb);
		void indexNewWord(ConstStrA word, WordID wordId);
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
		bool enumDocuments(Fn fn) const;

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

		bool enumDocumentsVt(const IEnumDocsCallback &cb) const;

	};

	template<typename Fn>
	bool SourceIndex::OpenedWordIndexFile::enumDocuments(Fn fn) const
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
