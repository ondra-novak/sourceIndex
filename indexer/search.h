#pragma once
#include "docid.h"
#include "increment.h"
#include "lightspeed/base/containers/autoArray.h"
#include "wordIndex.h"
#include "lightspeed/base/containers/map.h"

namespace SourceIndex {

	using namespace LightSpeed;

	class Search
	{
	public:

		Search(WordIndex &wordIndex):wordIndex(wordIndex) {}

		struct ResultItem {
			DocID document;
			AutoArray<WordMatch> matches;
			natural score;
		};

		typedef AutoArray<ResultItem> Result;


		struct QueryItem {
			WordID word;
			///group separator - separated words don't need to be near
			bool newGroup;
			///search begin of the word - with wordEnd it searches whole word
			bool wordBegin;
			///search end of the word - with wordBegin it searches whole word
			bool wordEnd;
			///exclude this word
			bool exclusion;
			///inbetween search where this word has wordBegin and next word has wordEnd
			/**
			In this case, it is allowed to accept both words on the same hit. This is special
			case when begin*end word is searched: "sour*dex" can match "sourceIndex" or "source index" (with
			worse score)

			if used with exclusion, second word must have at least one hit below to first hit of the first word.
			otherwise exclusion fails
			*/
			bool inbetween;

		};

		typedef AutoArray<QueryItem> Query;
		typedef ConstStringT<QueryItem> CQuery;



		///Searches for query in word index
		/**
		@param windex word index
		@param query query
		*/
		static void search(const CQuery &query, Result &result);

		
	protected:

		typedef Map<WordID, OpenedWordIndexFile> OpenedWordsMap;

		WordIndex &wordIndex;
		OpenedWordsMap wordMap;

		class QueryCmp;
		


		typedef ConstStringT<WordMatch> WordMatchSet;
		typedef AutoArray<WordMatchSet, SmallAlloc<4> > MultiMatchSet;
		typedef const DocID *CPDoc;

		class CmpCPDoc {
		public: bool operator()(CPDoc a, CPDoc b) const;
		};

		typedef Map<CPDoc, MultiMatchSet, CmpCPDoc > DocMap;
		typedef AutoArray<OpenedWordIndexFile> WordIndexSet;

		void createDocMap(DocMap &map, const QueryItem &item, WordIndexSet &openedWords);
		static void mergeExclude(DocMap &docMap, const DocMap &andMap);
		static void mergeAnd(DocMap &docMap, const DocMap &andMap);
		static void buildResult(const DocMap &docMap, const Query & query, Result &result);
		static natural findBestHit(const MultiMatchSet &matches, AutoArray<WordMatch> &result, ConstStringT<bool> groups, natural startPos);
		static std::pair<WordMatch,natural> findNearestMatch(const WordMatchSet &matchSet, const WordMatch &relativeTo);

		class UnionMatches;
	};


	class QueryBuilder {
	public:

		Search::Query buildQuery(WordIndex &widx, ConstStrA textQuery, bool caseSensitive, bool wholeWords);

	};

}
