#ifdef KERNEL
#include <types.h>
#endif
#include "network.h"

int netprot_createPackage(PackageHeader_t * header, int no, uint8_t type, uint8_t dest, void * data, size_t dataSize) {
	uint32_t * d = (uint32_t*)data;
	uint32_t sum = 0;
	uint32_t xor = 0;
	int i;

	if (dataSize % 4 != 0)
		return -1;

	for (i = 0 ; i < dataSize/4 ; i++) {
		xor ^= d[i];
		sum = ((sum << 15) + d[i]) % 1000000007;
	}

	header->h1 = 0x83126743;
	header->h2 = 0x97321864;
	header->type = type;
	header->no = no;
	header->dest = dest;
	header->sum = sum;
	header->xor = xor;
	header->size = dataSize;

	return 0;
}

int netprot_checkPackage(PackageHeader_t * header, void * data) {
	if (header->h1 != 0x83126743 || header->h2 != 0x97321864)
		return -1;

	int i;
	uint32_t dataSize = header->size;

	if (dataSize % 4 != 0)
		return -2;

	if (data == NULL)
		return 0;

	uint32_t * d = (uint32_t*)data;
	uint32_t sum = 0;
	uint32_t xor = 0;
	for (i = 0 ; i < dataSize/4 ; i++) {
		xor ^= d[i];
		sum = ((sum << 15) + d[i]) % 1000000007;
	}

	if (sum != header->sum || xor != header->xor)
		return -3;

	return 0;
}
