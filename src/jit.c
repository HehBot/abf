#include "bf.h"
#include "dynarr.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

void (*jitc(bf_ops_t bf))(int, int)
{
    struct {
        uint8_t* array;
        size_t count;
        size_t capacity;
    } emitted_code = { 0 };

    // rsi -> data pointer

    dynarr_append(&emitted_code, 0xC3); // ret

    void* mmapped_region = mmap(NULL, emitted_code.count, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    memmove(mmapped_region, emitted_code.array, emitted_code.count);

    free(emitted_code.array);

    void (*f)(int, int);
    *(void**)(&f) = mmapped_region;
    return f;
}
