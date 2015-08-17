#include "lightspeed/base/containers/constStr.h"
#include "lightspeed/base/containers/carray.h"
#include "lightspeed/base/streams/fileio.h"

namespace SourceIndex {

	using namespace LightSpeed;
	typedef CArray<byte, 16> DocID;

	DocID getDocID(SeqFileInput document);


}
