

#include <stddef.h>
#include <stdint.h>
#include "asset.hpp"
#include "cart.hpp"
#include "cartdb.hpp"
#include "util.hpp"

namespace cartdb {

bool CartDB::init(void) {
	if (!data.ptr)
		return false;

	auto &firstEntry = *reinterpret_cast<const Entry *>(data.ptr);

	if (firstEntry.version != ENTRY_VERSION) {
		LOG("unsupported DB version %d", firstEntry.version);
		return false;
	}

	_chipType    = firstEntry.chipType;
	_entryLength = sizeof(Entry) + cart::getDataLength(_chipType);
	numEntries   = data.length / _entryLength;

	return true;
}

IdentifyResult CartDB::identifyCart(cart::Cart &cart) const {
	// TODO: implement this

	LOG("no matching game found");
	return UNIDENTIFIED;
}

}
