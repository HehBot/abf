#include "bf.h"
#include "dynarr.h"

#include <stdio.h>
#include <stdlib.h>

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

static char lex_next(lex_t* l)
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
            .type = c,
            .operand = l.head - 1, // saving byte offset
        };
        switch (c) {
        case OP_INC:
        case OP_DEC:
        case OP_LEFT:
        case OP_RIGHT:
        case OP_INPUT:
        case OP_OUTPUT: {
            char s;
            size_t count = 1;
            while ((s = lex_next(&l)) != 0 && s == c)
                count++;
            if (c == OP_INC || c == OP_DEC)
                count = count & 0xFF;
            else if (c == OP_INPUT)
                count = 1;
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
