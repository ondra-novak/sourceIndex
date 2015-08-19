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
		Search();
		~Search();

		struct ResultItem {
			DocID document;
			AutoArray<WordMatch> matches;
			float relevance;
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
		};

		typedef AutoArray<QueryItem> Query;


		///Searches for query in word index
		/**
		@param windex word index
		@param query query
		*/
		void search(WordIndex &windex, const Query &query, Result &result);

		
	protected:
		bool testQuerySorted(const Query & query);
		void search2(WordIndex & windex, const Query & query, Result &result);
		Query sortQuery(Query query);

		class QueryCmp;
		

		typedef ConstStringT<WordMatch> WordMatchSet;
		typedef AutoArray<WordMatchSet, SmallAlloc<4> > MultiMatchSet;
		typedef const DocID *CPDoc;

		class CmpCPDoc {
		public: bool operator()(CPDoc a, CPDoc b) const;
		};


		typedef Map<CPDoc, MultiMatchSet, CmpCPDoc > DocMap;
		typedef AutoArray<OpenedWordIndexFile> WordIndexSet;

		natural createDocMap(DocMap &map, const WordIndexSet &wis, const Query &query, natural p, natural group);
		void mergeExclude(DocMap &docMap, const DocMap &andMap);
		void mergeAnd(DocMap &docMap, const DocMap &andMap);
		void buildResult(const DocMap &docMap, const Query & query, Result &result);
		float findBestHit(const MultiMatchSet &matches, AutoArray<WordMatch> &result, ConstStringT<bool> groups, natural startPos);
		class UnionMatches;
	};


}