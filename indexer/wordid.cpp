#include "wordid.h"
#include "lightspeed\utils\crc32.h"

namespace SourceIndex {

	WordID getWordId(ConstStrA word)
	{
		Crc32 crc32;
		crc32.blockWrite(reinterpret_cast<const byte *>(word.data()), word.length());
		return crc32.getCrc32();
	}

}