#include "docid.h"
#include "lightspeed\utils\md5iter.h"

namespace SourceIndex {


	SourceIndex::DocID getDocID(SeqFileInput document)
	{
		HashMD5<byte> md5;
		CArray<byte, 4096> buffer;
		md5.blockCopy(document, buffer);
		md5.finish();
		return DocID(md5.digest());

	}

}