#ifndef BF_H
#define BF_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef enum {
    OP_INC = '+',
    OP_DEC = '-',
    OP_LEFT = '<',
    OP_RIGHT = '>',
    OP_JZ = '[',
    OP_JNZ = ']',
    OP_INPUT = ',',
    OP_OUTPUT = '.',
} bf_op_type_t;

static inline bool is_bf_op(char c)
{
    return strchr("+-<>[],.", c) != NULL;
}

typedef struct {
    bf_op_type_t type;
    size_t operand;
} bf_op_t;

typedef struct {
    bf_op_t* array;
    size_t count;
    size_t capacity;
} bf_ops_t;

#endif // BF_H
