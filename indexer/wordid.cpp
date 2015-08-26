#include "wordid.h"
#include "lightspeed/utils/crc32.h"
#include "lightspeed/base/streams/utf.h"
#include "lightspeed/base/iter/typeconv.h"
#include <cwctype>

namespace SourceIndex {

	WordID getWordId(ConstStrA word)
	{
		Crc32 crc32;
		crc32.blockWrite(reinterpret_cast<const byte *>(word.data()), word.length());
		return crc32.getCrc32();
	}

	SourceIndex::WordID getLocaseWordId(ConstStrA word)
	{
		Crc32 crc32;
		TypeConvWriter<Crc32 &, char> crc32chr(crc32);
		WideToUtf8Writer<TypeConvWriter<Crc32 &, char> &> crc32wchr(crc32chr);

		Utf8ToWideReader<ConstStrA::Iterator> wideChars(word.getFwIter());

		while (wideChars.hasItems()) crc32wchr.write(towupper(wideChars.getNext()));

		return crc32.getCrc32();
	}

}
