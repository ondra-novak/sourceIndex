#include "lightspeed/base/types.h"
#include "lightspeed/base/containers/constStr.h"

namespace SourceIndex {


	using namespace LightSpeed;
	typedef Bin::natural32 WordID;

	WordID getWordId(ConstStrA word);
	WordID getLocaseWordId(ConstStrA word);

}