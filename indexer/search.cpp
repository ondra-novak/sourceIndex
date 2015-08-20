#include "search.h"
#include "lightspeed/base/containers/sort.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/containers/sort.tcc"

#include "tokenizer.h"
namespace SourceIndex {


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

		while (leftIter.hasItems()) {
			const DocMap::Entity &e1 = leftIter.getNext();
			newDocMap.insert(e1.key, e1.value);
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

	static natural subAbs(natural a, natural b) {
		if (a < b) return b - a;
		else return a - b;
	}

	std::pair<WordMatch,natural> Search::findNearestMatch(const WordMatchSet &matchSet, const WordMatch &relativeTo) {
		if (matchSet.empty()) return std::make_pair(relativeTo,natural(0));

		natural bestScore = naturalNull;
		WordMatch bestMatch;

		for (natural i = 0; i < matchSet.length(); i++) {
			natural score;
			const WordMatch &cur = matchSet[i];
			if (relativeTo.getPage() != cur.getPage()) {
				score = subAbs(relativeTo.getPage(),cur.getPage()) * 1000;
			} else if (relativeTo.getLine() != cur.getLine()) {
				score = subAbs(relativeTo.getLine(),cur.getLine()) * 100;
			} else {
				if (cur.getCol() < relativeTo.getCol()) {
					score = (relativeTo.getCol() - cur.getCol()) * 2;
				} else {
					score = (cur.getCol() - relativeTo.getCol());
				}
			}
			if (score < bestScore) {
				bestMatch = cur;
				bestScore = score;
			}
		}

		return std::make_pair(bestMatch,bestScore);

	}


	natural Search::findBestHit(const MultiMatchSet &matches, AutoArray<WordMatch> &result, ConstStringT<bool> groups, natural startPos)
	{
		natural score = 0;
		natural groupEnd = groups.find(true,startPos+1);
		if (groupEnd == naturalNull) groupEnd = groups.length();
		if (startPos == groupEnd) return 0;

		if (startPos + 1 == groupEnd) {
			if (matches[startPos].empty()) result.add(WordMatch());
			else result.add(matches[startPos][0]);
			return score + findBestHit(matches,result,groups,startPos+1);
		} else {
			const WordMatchSet &first = matches[startPos];
			const WordMatchSet &second = matches[startPos+1];
			WordMatch bestFirst;
			WordMatch bestSecond;
			natural bestFirstScore = naturalNull;
			for (natural i = 0; i < first.length(); i++) {
				std::pair<WordMatch, natural> res = findNearestMatch(second,first[i]);
				if (res.second < bestFirstScore) {
					bestFirst = first[i];
					bestSecond = res.first;
					bestFirstScore = res.second;
				}
			}
			result.add(bestFirst);
			result.add(bestSecond);
			WordMatch relTo = bestSecond;
			score = bestFirstScore;
			for (natural i = startPos+2; i < groupEnd; i++) {
				std::pair<WordMatch, natural> res = findNearestMatch(matches[i], relTo);
				score += res.second;
				result.add(res.first);
				relTo = res.first;
			}
		}

		if (groupEnd < groups.length()) {
			return score+ findBestHit(matches,result,groups,groupEnd);
		} else {
			return score;
		}

	}

	bool Search::CmpCPDoc::operator()(CPDoc a, CPDoc b) const
	{
		return *a < *b;
	}

	Search::Query SourceIndex::QueryBuilder::buildQuery(WordIndex& widx, ConstStrA textQuery, bool caseSensitive, bool wholeWords) {

		Search::Query query;

		class TokenizerCB: public AbstractPlainTextTokenizer {
		public:
			Search::Query &query;
			WordIndex& widx;
			bool caseSensitive;
			bool wholeWords;
			natural n;
			bool nextGroup;
			bool nextIsExclusion;


			TokenizerCB(WordIndex& widx, Search::Query &query, bool caseSensitive, bool wholeWords)
				:query(query),widx(widx),caseSensitive(caseSensitive),wholeWords(wholeWords),n(0),nextGroup(true),nextIsExclusion(false) {}

			virtual void flushWord(ConstStrA word, natural page, natural line, natural col) {
				Search::QueryItem qi;
				qi.group = nextGroup;
				qi.position = nextIsExclusion?naturalNull:n;
				qi.word = getWordId(word);
				query.add(qi);
				if (!caseSensitive || !wholeWords) {
					widx.findWords(word, caseSensitive, wholeWords, this);
				}
				nextGroup = false;
				n++;
				nextIsExclusion = false;
			}

			virtual void foundWord(ConstStrA word) {
				Search::QueryItem qi;
				qi.group = false;
				qi.position = nextIsExclusion?naturalNull:n;
				qi.word = getWordId(word);
				query.add(qi);
			}

			virtual void flushSymbol(ConstStrA word,natural page, natural line, natural col) {
				if (word == ConstStrA(',')) nextGroup = true;
				else if (word == ConstStrA('-')) nextIsExclusion = true;
			}


			virtual void fileIsBinary() {

			}

		};

		TokenizerCB tcb(widx,query,caseSensitive,wholeWords);
		ConstStrA::Iterator iter = textQuery.getFwIter();
		tcb.tokenizePlainTextStream(iter);
		return query;

	}

}

