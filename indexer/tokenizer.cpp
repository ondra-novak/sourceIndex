#include "tokenizer.h"
#include "lightspeed/base/streams/bufferedio.h"
#include "lightspeed/base/streams/fileiobuff.h"
#include "lightspeed/base/text/textstream.tcc"
#include "lightspeed/base/iter/iteratorFilter.tcc"
#include "lightspeed/utils/md5iter.h"

#include "lightspeed/base/debug/dbglog.h"
namespace SourceIndex {

	template<byte bytes, bool le>
	class UTFFilter : public IteratorFilterBase<byte, wchar_t, UTFFilter<bytes,le> > {
	public:
		UTFFilter() :sum(0), counter(0) {}
		bool needItems() const { return counter < bytes; }
		void input(const byte &x) {
			if (le) sum = sum | ((wchar_t)x << (8 * counter)); else	sum = sum * 256 + x; 
			counter ++; 
		}
		bool hasItems() const { return counter == bytes; }
		wchar_t output() { wchar_t out = sum;  counter = 0; sum = 0; return out; }
		natural calcToWrite(natural srcCount) const { return srcCount/2; }
		natural calcToRead(natural trgCount) const { return trgCount*2; }
	protected:
		wchar_t sum;
		byte counter;
	};


	void TokenizedDocument::tokenizePlainText(SeqFileInput input)
	{
		docIncrement.clear();
		wordList.clear();
		classList.clear();
		wordListPool.clear();
		binaryFile = false;

		SeqFileInBuff<> buffInput(input);
		byte bomtest[4];
		IOBuffer<> &buffer = buffInput.getBuffer();
		natural cnt = buffer.peek(bomtest, 4);
		if (cnt != 4) {
			SeqTextInA stream(buffInput);
			tokenizePlainTextStream(stream);
		} else if (bomtest[0] == 0xEF && bomtest[1] == 0xBB && bomtest[2] == 0xBF) {
			buffer.read(bomtest, 3); //skip BOM
			SeqTextInA stream(buffInput); //default input is UTF-8
			tokenizePlainTextStream(stream); //so index it
		} else if (bomtest[0] == 0xFE && bomtest[1] == 0xFF) {
			buffer.read(bomtest, 2); //skip BOM
			WideToUtf8Reader<Filter<UTFFilter<2, false> >::Read<SeqFileInput> > stream(buffInput);
			tokenizePlainTextStream(stream); //so index it
		} else if (bomtest[0] == 0xFF && bomtest[1] == 0xFE && bomtest[2] == 0x00 && bomtest[3] == 0x00) {
			buffer.read(bomtest, 4); //skip BOM
			WideToUtf8Reader<Filter<UTFFilter<4, true> >::Read<SeqFileInput> > stream(buffInput);
			tokenizePlainTextStream(stream); //so index it
		} else if (bomtest[0] == 0x00 && bomtest[1] == 0x00 && bomtest[2] == 0xFE && bomtest[3] == 0xFF) {
			buffer.read(bomtest, 4); //skip BOM
			WideToUtf8Reader<Filter<UTFFilter<4, false> >::Read<SeqFileInput> > stream(buffInput);
			tokenizePlainTextStream(stream); //so index it
		} else if (bomtest[0] == 0xFF && bomtest[1] == 0xFE) {
			buffer.read(bomtest, 2); //skip BOM
			WideToUtf8Reader<Filter<UTFFilter<2, true> >::Read<SeqFileInput> > stream(buffInput);
			tokenizePlainTextStream(stream); //so index it
		}
		else {
			byte testBuffer[1024];
			natural cnt = buffer.peek(testBuffer, 1024);
			natural zr1 = 0;
			natural zr2 = 0;
			for (natural i = 0; i+1 < cnt; i += 2) {
				if (testBuffer[i] == 0) zr1++;
				if (testBuffer[i + 1] == 0) zr2++;
			}
			if (zr1 * 2 < zr2 && zr2 > cnt / 2) {
				//probably UCS2 LE
				WideToUtf8Reader<Filter<UTFFilter<2, true> >::Read<SeqFileInput> > stream(buffInput);
				tokenizePlainTextStream(stream); //so index it
			}
			else if (zr2 * 2 < zr1 && zr1 > cnt / 2) {
				//probably UCS2 BE
				WideToUtf8Reader<Filter<UTFFilter<2, false> >::Read<SeqFileInput> > stream(buffInput);
				tokenizePlainTextStream(stream); //so index it
			}
			else {
				SeqTextInA stream(buffInput); //default input is UTF-8
				tokenizePlainTextStream(stream); //so index it
			}
		}
		updateDocId();
	}


	static inline bool iswordchar(char c) {

		return (c < 0 || isalpha(c) || c == '_' );
	}

	template<typename K>
	void SourceIndex::TokenizedDocument::tokenizePlainTextStream(IIterator<char, K> &iter)
	{
		natural line = 1;
		natural col = 1;
		bool cr = false;
		bool lf = false;
		AutoArrayStream<char, StaticAlloc<64> > wordBuffer;
		natural binChar = 0;
		while (iter.hasItems()) {
			char c = iter.getNext();
			if (c == '\r') {
				if (cr) { line++; col = 1; }
				else cr = true;
			}
			else if (c == '\n') {
				if (lf) { line++; col = 1; }
				else lf = true;
			}
			else if (c != '\t' && c >= 0 && c < 32) {
				//count bin characters
				binChar++;
				//if more bin characters found, mark file binary and exit
				if (binChar > 5) {
					binaryFile = true;
					return;
				}
			}
			else {
				if (cr || lf) {
					line++;
					col = 1;
					cr = lf = false;
				}

				if (c > 0 && isdigit(c)) {
					wordBuffer.write(c);
					while (iter.hasItems() && isdigit(iter.peek()) && wordBuffer.hasItems())
						wordBuffer.write(iter.getNext());
					flushWord(wordBuffer.getArray(),line,col);
					col += wordBuffer.length();
					wordBuffer.clear();
				}
				else if (iswordchar(c)) {
					wordBuffer.write(c);
					while (iter.hasItems() && iswordchar(iter.peek()) && wordBuffer.hasItems())
						wordBuffer.write(iter.getNext());
					flushWord(wordBuffer.getArray(), line, col);
					col += wordBuffer.length();
					wordBuffer.clear();
				}
				else {
					col++;
				}
			}
		}
	}

	void TokenizedDocument::flushWord(ConstStrA word, natural line, natural col)
	{
		const StringPoolA::Str *w = wordList.find(word);
		if (w == 0) {
			wordList.insert(wordListPool.add(word));
			flushWord(word,line,col);
		}
		else {
			WordIncrement wi;
			wi.wordID = getWordId(word);
			if (col > 0xFFFF) col = 0xFFFF;
			wi.match = WordMatch(0, Bin::natural32(line), Bin::natural16(col));
			docIncrement.add(wi);
		}

	}

	void TokenizedDocument::updateDocId()
	{
		HashMD5<byte> hash;
		hash.blockWrite(ConstBin(reinterpret_cast<const byte *>(docIncrement.data()), docIncrement.length()*sizeof(*docIncrement.data())),true);
		hash.finish();
		docId = hash.digest();
	}

}
