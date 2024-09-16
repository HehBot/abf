#include "bf.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool read_file(char const* filepath, char** buf_ptr, size_t* size_ptr)
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

    return true;

f3:
    free(buf);
f2:
    close(fd);
f1:
    return false;
}

bf_ops_t parse_bf(char* buf, size_t size);
bool interpret(bf_ops_t bf, int fd_in, int fd_out);
void (*jitc(bf_ops_t bf))(int, int);

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <bf script>\n", argv[0]);
        return 1;
    }

    char const* filepath = argv[1];

    char* buf;
    size_t size;
    if (!read_file(filepath, &buf, &size)) {
        fprintf(stderr, "Error: Reading file %s: %s\n", filepath, strerror(errno));
        return 1;
    }

    bf_ops_t bf_ops = parse_bf(buf, size);
    free(buf);

    if (bf_ops.array == NULL)
        return 1;

    void (*f)(int, int) = jitc(bf_ops);
    if (f == NULL)
        return 1;
    f(fileno(stdin), fileno(stdout));

    interpret(bf_ops, fileno(stdin), fileno(stdout));

    free(bf_ops.array);
    return 0;
}
