#include "bf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bf_err_t interpret(bf_ops_t bf, int fd_in, int fd_out, uint8_t** data_array_ptr, size_t data_size)
{
    uint8_t* data_array = *data_array_ptr;

    size_t ip = 0;
    size_t dp = 0; // data pointer
    while (ip < bf.count) {
        bf_op_t op = bf.array[ip];
        ip++;
        switch (op.type) {
        case OP_INC:
            data_array[dp] += op.operand;
            break;
        case OP_DEC:
            data_array[dp] -= op.operand;
            break;
        case OP_LEFT:
            if (dp < op.operand)
                return BF_ERR_LEFT_OF_FIRST_CELL;
            dp -= op.operand;
            break;
        case OP_RIGHT:
            dp += op.operand;
            if (dp >= data_size) {
                size_t new_size = 2 * dp + 10;
                data_array = *data_array_ptr = realloc(data_array, new_size);
                if (data_array == NULL)
                    return BF_ERR_MEM;
                memset(&data_array[data_size], 0, new_size - data_size);
                data_size = new_size;
            }
            break;
        case OP_JZ:
            if (data_array[dp] == 0)
                ip = op.operand;
            break;
        case OP_JNZ:
            if (data_array[dp] != 0)
                ip = op.operand;
            break;
        case OP_INPUT: {
            int r = read(fd_in, &data_array[dp], sizeof(data_array[0]));
            if (r == 0) {
                // end of program input is OK
                return BF_OK;
            } else if (r == -1)
                return BF_ERR_READ;
        } break;
        case OP_OUTPUT: {
            char c = data_array[dp];
            for (size_t i = 0; i < op.operand; ++i) {
                int r = write(fd_out, &c, sizeof(c));
                if (r == -1)
                    return BF_ERR_WRITE;
            }
        } break;
        default:
            __builtin_unreachable();
        }
    }

    return BF_OK;
}
