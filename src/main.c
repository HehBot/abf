#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define dynarr_append(dynarr, obj)                                                                       \
    do {                                                                                                 \
        if ((dynarr)->count >= (dynarr)->capacity) {                                                     \
            (dynarr)->capacity = 2 * (dynarr)->capacity + 20;                                            \
            (dynarr)->array = realloc((dynarr)->array, (dynarr)->capacity * sizeof((dynarr)->array[0])); \
        }                                                                                                \
        (dynarr)->array[(dynarr)->count] = (obj);                                                        \
        (dynarr)->count++;                                                                               \
    } while (0);

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

typedef enum {
    OP_INC = '+',
    OP_DEC = '-',
    OP_LEFT = '<',
    OP_RIGHT = '>',
    OP_JZ = '[',
    OP_JNZ = ']'
} bf_op_type_t;

bool is_bf_op(char c)
{
    return strchr("+-<>[]", c) != NULL;
}

typedef struct {
    bf_op_type_t op_type;
    size_t operand;
} bf_op_t;

typedef struct {
    bf_op_t* array;
    size_t count;
    size_t capacity;
} bf_ops_t;

typedef struct {
    char* buf;
    size_t size;
    size_t head;
} lex_t;

typedef struct {
    size_t* array;
    size_t count;
    size_t capacity;
} offsets_t;

char lex_next(lex_t* l)
{
    char c;
    while (l->head != l->size && !is_bf_op(c = l->buf[l->head]))
        l->head++;
    if (l->head == l->size)
        return 0;
    l->head++;
    return c;
}

bf_ops_t parse_bf(char* buf, size_t size)
{
    bf_ops_t bf_ops = { 0 };
    lex_t l = {
        .buf = buf,
        .size = size,
        .head = 0,
    };
    offsets_t stack = { 0 };

    char c = lex_next(&l);
    while (c != 0) {
        bf_op_t op = {
            .op_type = c,
            .operand = l.head - 1, // saving byte offset
        };
        switch (c) {
        case OP_INC:
        case OP_DEC:
        case OP_LEFT:
        case OP_RIGHT: {
            char s;
            size_t count = 1;
            while ((s = lex_next(&l)) != 0 && s == c)
                count++;
            op.operand = count;
            c = s;
        } break;
        case OP_JZ: {
            dynarr_append(&stack, bf_ops.count);
            c = lex_next(&l);
        } break;
        case OP_JNZ: {
            if (stack.count == 0) {
                fprintf(stderr, "Error: Ill-formed BF program: No matching '[' for ']' at %zu\n", op.operand);
                goto f1;
            }
            size_t match = stack.array[stack.count - 1];
            stack.count--;
            op.operand = match + 1;
            bf_ops.array[match].operand = bf_ops.count + 1;
            c = lex_next(&l);
        } break;
        default:
            __builtin_unreachable();
        }
        dynarr_append(&bf_ops, op);
    }
    if (stack.count != 0) {
        fprintf(stderr, "Error: Ill-formed BF program: No matching ']' for '[' at %zu\n", bf_ops.array[stack.array[stack.count - 1]].operand);
        goto f1;
    }

    free(stack.array);
    return bf_ops;

f1:
    free(stack.array);
    free(bf_ops.array);
    return (bf_ops_t) { 0 };
}

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
        fprintf(stderr, "Error reading file %s: %s\n", filepath, strerror(errno));
        return 1;
    }

    bf_ops_t bf_ops = parse_bf(buf, size);
    free(buf);

    if (bf_ops.array == NULL)
        return 1;

    for (size_t i = 0; i < bf_ops.count; i++)
        printf("%3zu %c %zu\n", i, bf_ops.array[i].op_type, bf_ops.array[i].operand);

    free(bf_ops.array);

    return 0;
}
