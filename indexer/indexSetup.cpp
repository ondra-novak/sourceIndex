/*
 * indexSetup.cpp
 *
 *  Created on: 18. 8. 2015
 *      Author: ondra
 */

#include "indexSetup.h"
#include "lightspeed/base/containers/autoArray.tcc"


namespace SourceIndex {
IndexSetup::IndexSetup() {
	// TODO Auto-generated constructor stub

}

IndexSetup::~IndexSetup() {
	// TODO Auto-generated destructor stub
}

void IndexSetup::loadSetup(SeqFileInput file) {
/*
	JSON::Value setup = JSON::fromStream(file);
	for (JSON::Iterator iter = setup->getFwIter(); iter.hasItems();) {
		const JSON::KeyValue &kv = iter.getNext();
		JSON::Value setupItem = kv.node;

	}
*/
}

void IndexSetup::saveSetup(SeqFileOutput file) const {
}

IndexSetup::FileList IndexSetup::generateFileList() const {
}

}
