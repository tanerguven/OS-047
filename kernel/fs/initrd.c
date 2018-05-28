#include <kernel/kernel.h>
#include <string.h>

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
};

static unsigned int tar_getsize(const char *in) {
    unsigned int size = 0;
    unsigned int j;
    unsigned int count = 1;

    for (j = 11; j > 0; j--, count *= 8)
        size += ((in[j - 1] - '0') * count);

    return size;
}

#define MAX_INITRD_FILE_COUNT 16
static struct {
	char path[100];
	addr_t addr;
	size_t size;
} fileList[MAX_INITRD_FILE_COUNT];

static int fileCount = 0;

void initrd_init(addr_t initrdStart, size_t initrdSize) {
	print_info("[initrd_init]\n");
	int i;

	void * initrd = (void*)initrdStart;
	addr_t address = (addr_t)initrd;
	struct tar_header * header = (struct tar_header *)address;

	for (i = 0 ; header->name[0] != '\0' ; i++) {
		unsigned long size = tar_getsize(header->size);

		ASSERT(strncmp(header->name, "initrd/", 7) == 0);

		switch (header->typeflag[0]) {
		case '0':
			strcpy(fileList[fileCount].path, header->name + 7);
			fileList[fileCount].addr = address+512;
			fileList[fileCount].size = size;
			// print_info("file: %s (%d byte)\n", fileList[fileCount].path, fileList[fileCount].size);
			fileCount++;
			ASSERT3(fileCount, <, MAX_INITRD_FILE_COUNT);

			break;
		case '5':
			// print_info("dir: %s\n", header->name);
			break;
		default:
			print_info("unknown typeflag: %c\n", header->typeflag[0]);
			PANIC("-");
		}
		// printf("  mode:%s size:%s\n", header->mode, header->size);

		address += ((size / 512) + 1) * 512;
		if (size % 512)
			address += 512;

		header = (struct tar_header *)address;
	}
}

int initrd_getFileAddr(const char * path, addr_t * addr, size_t * size) {
	int i;
	for (i = 0 ; i < MAX_INITRD_FILE_COUNT ; i++) {
		if (strcmp(path, fileList[i].path) == 0) {
			*addr = fileList[i].addr;
			*size = fileList[i].size;
			return 0;
		}
	}

	return -1;
}

/*
void initrd_test() {
	addr_t addr;
	size_t size;
	int r;
	r = initrd_getFileAddr("test.txt", &addr, &size);
	ASSERT(r == 0);

	char buf[256];
	strncpy(buf, (char*)addr, size);
	print_info("test.txt: '%s'\n", buf);
}
*/
