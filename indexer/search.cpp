#include "search.h"
#include "lightspeed/base/containers/sort.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/containers/sort.tcc"

namespace SourceIndex {

	Search::Search()
	{
	}


	Search::~Search()
	{
	}

	void Search::search(WordIndex &windex, const Query &query, Result &result)
	{
		result.clear();
		if (!testQuerySorted(query)) {
			search2(windex, sortQuery(query),result);
		}
		else {
			search2(windex, query, result);
		}
	}

	bool Search::testQuerySorted(const Query & query)
	{
		natural p = 0;
		for (natural i = 0; i < query.length(); i++) {
			if (query[i].position > p) return false;
			p = query[i].position;
		}
		return true;
	}

	class Search::QueryCmp {
	public:
		bool operator()(const QueryItem &a, const QueryItem &b){ return a.position < b.position; }
	};


	Search::Query Search::sortQuery(Query query)
	{
		HeapSort<Query, QueryCmp> hs(query);
		hs.sort();
		return query;
	}

	natural Search::createDocMap(DocMap &map, const WordIndexSet &wis, const Query &query, natural p, natural group) {
		while (p < query.length() && query[p].position == group) {
			wis[p].enumDocuments([&map](const DocID &docId, Bin::natural16 count, const WordMatch *matches){
				map.insert(&docId, MultiMatchSet(1,WordMatchSet(matches, count)));
				return true;
			});
			p++;
		}
		return p;
	}

	void Search::search2(WordIndex & windex, const Query & query, Result &result)
	{
		if (query.empty() || query[0].position == naturalNull) return;

		AutoArray<OpenedWordIndexFile> wordIndexes;
		for (natural i = 0; i < query.length(); i++) wordIndexes.add(windex.openWordIndexForRead(query[i].word));

		DocMap docMap;
		DocMap andMap;

		natural p = createDocMap(docMap, wordIndexes, query, 0, query[0].position);

		if (docMap.empty()) return;

		while (p < query.length()) {
			andMap.clear();
			bool exclude = query[p].position == naturalNull;
			p = createDocMap(andMap, wordIndexes, query, p, query[p].position);
			if (exclude) {
				mergeExclude(docMap, andMap);
			}
			else {
				mergeAnd(docMap, andMap);
			}
			if (docMap.empty()) return;
		}

		buildResult(docMap, query,result);

	}


	void Search::mergeExclude(DocMap &docMap, const DocMap &andMap)
	{
		DocMap::Iterator leftIter = docMap.getFwIter();
		DocMap::Iterator rightIter = andMap.getFwIter();
		DocMap newDocMap;
		while (leftIter.hasItems() && rightIter.hasItems()) {
			const DocMap::Entity &e1 = leftIter.peek();
			const DocMap::Entity &e2 = rightIter.peek();
			if (*e1.key < *e2.key) {
				newDocMap.insert(e1.key, e1.value);
				leftIter.skip();
			}
			else if (*e1.key > *e2.key)  {
				rightIter.skip();
			}
			else {
				leftIter.skip();
				rightIter.skip();
			}
		}
		newDocMap.swap(docMap);
	}


	class Search::UnionMatches : public IteratorBase<WordMatchSet, UnionMatches>{
	public:
		UnionMatches(const MultiMatchSet &a1, const MultiMatchSet &a2) :a1(a1), a2(a2), pos(0) {}

		bool hasItems() const { return pos < a1.length() + a2.length(); }
		const WordMatchSet &getNext() {
			const WordMatchSet &ret = peek();
			pos++;
			return ret;
		}
		const WordMatchSet &peek() const {
			if (pos < a1.length()) return a1[pos];
			else return a2[pos - a1.length()];
		}

	protected:
		const MultiMatchSet &a1;
		const MultiMatchSet &a2;
		natural pos;
	};

	void Search::mergeAnd(DocMap &docMap, const DocMap &andMap)
	{
		DocMap::Iterator leftIter = docMap.getFwIter();
		DocMap::Iterator rightIter = andMap.getFwIter();
		DocMap newDocMap;
		while (leftIter.hasItems() && rightIter.hasItems()) {
			const DocMap::Entity &e1 = leftIter.peek();
			const DocMap::Entity &e2 = rightIter.peek();
			if (*e1.key < *e2.key) {
				leftIter.skip();
			} else if (*e1.key > *e2.key)  {
				rightIter.skip();
			}
			else {
				UnionMatches um(e1.value, e2.value);
				newDocMap.insert(e1.key, MultiMatchSet(um));
				leftIter.skip();
				rightIter.skip();
			}
		}
		newDocMap.swap(docMap);
	}


	struct CmpResultItem {
		bool operator()(const Search::ResultItem &a, const Search::ResultItem &b) {
			return a.relevance > b.relevance;
		}
	};

	void Search::buildResult(const DocMap &docMap, const Query & query, Result &result)
	{
		AutoArray<bool> groupSep;
		natural p = query[0].position; 
		groupSep.add(true);
		for (natural i = 1; i < query.length(); i++) {
			if (p != query[i].position) {
				p = query[i].position;
				if (p == naturalNull) break;
				groupSep.add(query[i].group);
			}		
		}

		HeapSort<Result, CmpResultItem> resheap(result);

		for (DocMap::Iterator iter = docMap.getFwIter(); iter.hasItems();) {
			const DocMap::Entity &e = iter.getNext();
			ResultItem ritem;
			ritem.relevance = findBestHit(e.value, ritem.matches, groupSep,0);
			ritem.document = *e.key;
			result.add(ritem);
			resheap.push();
		}
		
		resheap.sortHeap();		

	}


	float Search::findBestHit(const MultiMatchSet &matches, AutoArray<WordMatch> &result, ConstStringT<bool> groups, natural startPos)
	{
		float relevance = 1;

		//TODO implement findBestHit for group

		if (startPos < matches.length()) {
			return relevance * findBestHit(matches, result, groups, startPos);
		}
		else return relevance;
	}

	bool Search::CmpCPDoc::operator()(CPDoc a, CPDoc b) const
	{
		return *a < *b;
	}

}