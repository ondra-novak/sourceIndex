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

		struct ResultItem {
			DocID document;
			AutoArray<WordMatch> matches;
			natural score;
		};

		typedef AutoArray<ResultItem> Result;


		struct QueryItem {
			///position in query - words with same position are considered as OR. 
			///naturalNull means exclusion. Document with these words will be removed
			natural position;
			///searched word
			WordID word;
			///specifies starting of new group, which has no connection with previous group
			/**This means, that there will no relevance penalty, if next words will be located on other place in the document */
			bool group;
			///Exact words has baseScore equal 0, case not-matching words = 1, begin of word = 2, end of word = 3, any substring = 4
			natural baseScore;
		};

		typedef AutoArray<QueryItem> Query;


		///Searches for query in word index
		/**
		@param windex word index
		@param query query
		*/
		static void search(WordIndex &windex, const Query &query, Result &result);

		
	protected:
		static bool testQuerySorted(const Query & query);
		static void search2(WordIndex & windex, const Query & query, Result &result);
		static Query sortQuery(Query query);

		class QueryCmp;
		

		typedef ConstStringT<WordMatch> WordMatchSet;
		typedef AutoArray<WordMatchSet, SmallAlloc<4> > MultiMatchSet;
		typedef const DocID *CPDoc;

		class CmpCPDoc {
		public: bool operator()(CPDoc a, CPDoc b) const;
		};


		typedef Map<CPDoc, MultiMatchSet, CmpCPDoc > DocMap;
		typedef AutoArray<OpenedWordIndexFile> WordIndexSet;

		static natural createDocMap(DocMap &map, const WordIndexSet &wis, const Query &query, natural p, natural group);
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
