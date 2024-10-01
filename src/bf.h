#ifndef BF_H
#define BF_H

#include <stddef.h>
#include <stdint.h>
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

static inline int is_bf_op(char c)
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

typedef enum {
    BF_OK = 0,
    BF_ERR_READ = 1,
    BF_ERR_WRITE = 2,
    BF_ERR_MEM = 3,
    BF_ERR_LEFT_OF_FIRST_CELL = 4,
} bf_err_t;
bf_err_t interpret(bf_ops_t bf, int fd_in, int fd_out, uint8_t** data_array_ptr, size_t data_size);
typedef struct {
    void* b;
    size_t sz;
} buf_t;
buf_t jitc(bf_ops_t bf);

#endif // BF_H
