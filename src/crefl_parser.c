#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <public/types.h>
#include <dot_crefl.h>


static void print_header(void* mem, size_t sz) {
    DEREF(CREFL_Header, header, mem, 0);
    if (header->endianness) {
        printf("This refl version doesn't support big-endian.\n");
        munmap(mem, sz);
        exit(EXIT_FAILURE);
    }

    printf("Header information\n");
    printf("MAGIC: %.*s\n", 8, (char*)&header->magic);
    printf("ENDIANNESS: %hhu\n", header->endianness);
    printf("VERSION: %hu\n", header->version);
    printf("FLAGS: %hu\n", header->flags);
    printf("CHECK_SUM: 0x%lx%lx\n", *(uint64_t*)header->checksum + 8, *(uint64_t*)header->checksum);
    printf("N_SECTIONS: %u\n", header->n_sections);
}



void refl_engine_load(const char* file_path) {

    int fd = open(file_path, O_RDONLY, 0);
    if (fd < 0) {
        printf("Refl engine: Could not open file '%s': %s\n", file_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;


    void* mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    print_header(mapped, size);
    munmap(mapped, size);
}