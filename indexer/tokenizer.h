#pragma once
#include "docid.h"
#include "increment.h"
#include "lightspeed/base/containers/stringpool.h"
#include "lightspeed/base/containers/set.h"


namespace SourceIndex {

	struct TokenizedDocument {
		DocID docId;
		DocIncrement docIncrement;
		StringPoolA wordListPool;
		Set<StringPoolA::Str> wordList;
		Set<StringPoolA::Str> classList;
		bool binaryFile;

		void tokenizePlainText(SeqFileInput input);
		void addWord(ConstStrA word, natural page, natural line, natural col);

	private:
		template<typename K>
		void tokenizePlainTextStream(IIterator<char, K> &iter);

		void updateDocId();
	};


	class AbstractPlainTextTokenizer {
	public:

		void tokenize(SeqFileInput input);

		template<typename K>
		void tokenizePlainTextStream(IIterator<char, K> &iter);

		virtual void flushWord(ConstStrA word, natural page, natural line, natural col) = 0;
		virtual void flushSymbol(ConstStrA word, natural page, natural line, natural col) = 0;
		virtual void fileIsBinary() = 0;

	};

}
