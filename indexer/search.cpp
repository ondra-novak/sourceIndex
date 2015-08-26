#include "search.h"
#include "lightspeed/base/containers/sort.h"
#include "lightspeed/base/containers/autoArray.tcc"
#include "lightspeed/base/containers/sort.tcc"

#include "tokenizer.h"
namespace SourceIndex {


	void Search::createDocMap(DocMap &map, const QueryItem &item, WordIndexSet &openedWords) {

		const OpenedWordIndexFile *ofile = wordMap.find(item.word);
		if (ofile == 0) {
			OpenedWordsMap::Iterator iter = wordMap.insert(item.word, wordIndex.openWordIndexForRead(item.word));
			ofile = &iter.getNext().value;
		}
		ofile->enumDocuments([&](const DocID &docId, Bin::natural16 count, const WordMatch *matches) {
			map.insert(&docId,MultiMatchSet(WordMatchSet(matches,count)));
			return true;
		});


	}


	void Search::search(const CQuery &query, Result &result)
	{
		result.clear();
		if (query.empty()) return;


		DocMap docMap;
		DocMap andMap;

		natural p = createDocMap(docMap, query, 0, query[0].position);

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
			return a.score < b.score;
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
			ritem.score = findBestHit(e.value, ritem.matches, groupSep,0);
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
			
			TokenizerCB(bool caseNormalize) :nextIsEnd(false), nextIsGroup(false),caseNormalize(caseNormalize) {}

			struct BaseWord {
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


			virtual void flushWord(ConstStrA word, natural page, natural line, natural col) override
			{
				BaseWord bw;
				if (caseNormalize) bw.word = getLocaseWordId(word);
				else bw.word = getWordId(word);
				bw.wordEnd = nextIsEnd; nextIsEnd = false;
				bw.newGroup = nextIsGroup; nextIsGroup = false;
				bw.exclusion = nextIsExcluded; nextIsExcluded = false;
				bw.wordBegin = false;
				words.add(bw);

			}

			virtual void flushSymbol(ConstStrA word, natural page, natural line, natural col) override
			{
				for (natural i = 0; i < word.length(); i++) {
					switch (word[i]) {
					case '-': nextIsExcluded = true;
						break;
					case ',': nextIsGroup = true; nextIsExcluded = false; nextIsEnd = false;
						break;
					case '^': nextIsEnd = true;
						break;
					case '$': if (!nextIsGroup && !words.empty()) words(words.length() - 1).wordBegin = true;
						break;
					case '*':
					case '%': nextIsEnd = true;
						if (!nextIsGroup && !words.empty()) {
							words(words.length() - 1).wordBegin = true;
							words(words.length() - 1).inbetween = true;
							break;
						}
					}
				}
			}

			virtual void fileIsBinary() override
			{

			}



			AutoArray<BaseWord> words;
			bool nextIsEnd;
			bool nextIsGroup;
			bool caseNormalize;
			bool nextIsExcluded;

		};

		TokenizerCB tcb(widx,query,caseSensitive,wholeWords);
		ConstStrA::Iterator iter = textQuery.getFwIter();
		tcb.tokenizePlainTextStream(iter);
		return query;

	}

}

