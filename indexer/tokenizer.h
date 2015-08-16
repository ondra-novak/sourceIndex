#pragma once
#include "docid.h"
#include "increment.h"
#include "lightspeed\base\containers\stringpool.h"
#include "lightspeed\base\containers\set.h"


namespace SourceIndex {

	struct TokenizedDocument {
		DocID docId;
		DocIncrement docIncrement;
		StringPoolA wordListPool;
		Set<StringPoolA::Str> wordList;
		Set<StringPoolA::Str> classList;
		bool binaryFile;

		void tokenizePlainText(SeqFileInput input);

	private:
		template<typename K>
		void tokenizePlainTextStream(IIterator<char, K> &iter);

		void flushWord(ConstStrA word, natural line, natural col);
		void updateDocId();
	};

}