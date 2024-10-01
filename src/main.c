#include "bf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int read_file(char const* filepath, char** buf_ptr, size_t* size_ptr)
{
    int fd = open(filepath, O_RDONLY);
    if (fd == -1)
        goto f1;

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == -1)
        goto f2;
    if (lseek(fd, 0, SEEK_SET) == -1)
        goto f2;

    char* buf = malloc(size);
    if (buf == NULL)
        goto f2;

    size_t bytes_read = 0;
    while (bytes_read != size) {
        int next = read(fd, buf + bytes_read, size - bytes_read);
        if (next == -1)
            goto f3;
        bytes_read += next;
    }
    *buf_ptr = buf;
    *size_ptr = bytes_read;

    return 0;

f3:
    free(buf);
f2:
    close(fd);
f1:
    return 1;
}

bf_ops_t parse_bf(char* buf, size_t size);

int main(int argc, char** argv)
{
    int is_jit;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [interpret|jit] <bf script>\n", argv[0]);
        return 1;
    } else {
        if (!strcmp(argv[1], "jit"))
            is_jit = 1;
        else if (!strcmp(argv[1], "interpret"))
            is_jit = 0;
        else {
            fprintf(stderr, "Usage: %s [interpret|jit] <bf script>\n", argv[0]);
            return 1;
        }
    }

    char const* filepath = argv[2];

    char* buf;
    size_t size;
    if (read_file(filepath, &buf, &size) != 0) {
        fprintf(stderr, "Error: Reading file %s: %s\n", filepath, strerror(errno));
        return 1;
    }

    bf_ops_t bf_ops = parse_bf(buf, size);
    free(buf);

    if (bf_ops.array == NULL)
        return EXIT_FAILURE;

    size_t data_size = 1000;
    uint8_t* data_array = calloc(data_size, 1);
    int err;

    if (is_jit) {
        buf_t buf = jitc(bf_ops);
        if (buf.b == NULL) {
            fprintf(stderr, "JIT failed\n");
            return EXIT_FAILURE;
        }

        void* tmp = valloc(buf.sz);
        memmove(tmp, buf.b, buf.sz);
        free(buf.b);

        mprotect(tmp, buf.sz, PROT_EXEC | PROT_READ | PROT_WRITE);
        bf_err_t (*f)(int, int, uint8_t**, size_t);
        *(void**)&f = tmp;
        err = f(fileno(stdin), fileno(stdout), &data_array, data_size);
        mprotect(tmp, buf.sz, PROT_READ | PROT_WRITE);

        free(tmp);
    } else
        err = interpret(bf_ops, fileno(stdin), fileno(stdout), &data_array, data_size);

    switch (err) {
    case BF_OK:
        break;
    case BF_ERR_READ:
        fprintf(stderr, "Runtime error: Reading input: %s\n", strerror(errno));
        break;
    case BF_ERR_WRITE:
        fprintf(stderr, "Runtime error: Writing output: %s\n", strerror(errno));
        break;
    case BF_ERR_MEM:
        fprintf(stderr, "Runtime error: Memory allocation: %s\n", strerror(errno));
        break;
    case BF_ERR_LEFT_OF_FIRST_CELL:
        fprintf(stderr, "Runtime error: Ill-formed BF program: Cannot go left of first cell\n");
        break;
    default:
        __builtin_unreachable();
    }

    free(data_array);
    free(bf_ops.array);
    return EXIT_SUCCESS;
}
