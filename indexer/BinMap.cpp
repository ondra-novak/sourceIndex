#include "BinMap.h"
#include "lightspeed/base/containers/autoArray.tcc"

namespace SourceIndex {


	template class BinMap < Bin::natural32, Bin::natural32, std::less<Bin::natural32> >;

}
