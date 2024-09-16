#include "bf.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

bool interpret(bf_ops_t bf, int fd_in, int fd_out)
{
    struct {
        uint8_t* array;
        size_t size;
    } data = {
        .array = calloc(1, 1),
        .size = 1,
    };

    size_t ip = 0;
    size_t dp = 0; // data pointer
    while (ip < bf.count) {
        bf_op_t op = bf.array[ip];
        ip++;
        switch (op.type) {
        case OP_INC:
            data.array[dp] += op.operand;
            break;
        case OP_DEC:
            data.array[dp] -= op.operand;
            break;
        case OP_LEFT:
            if (dp < op.operand) {
                fprintf(stderr, "Error: Ill-formed BF program: Cannot go left of first cell\n");
                goto f1;
            }
            dp -= op.operand;
            break;
        case OP_RIGHT:
            dp += op.operand;
            if (dp >= data.size) {
                size_t new_size = 2 * dp + 10;
                data.array = realloc(data.array, new_size * sizeof(data.array[0]));
                memset(&data.array[data.size], 0, new_size - data.size);
                data.size = new_size;
            }

            break;
        case OP_JZ:
            if (data.array[dp] == 0)
                ip = op.operand;
            break;
        case OP_JNZ:
            if (data.array[dp] != 0)
                ip = op.operand;
            break;
        case OP_INPUT: {
            char c;
            int r = read(fd_in, &c, sizeof(c));
            if (r == 0)
                goto f1;
            else if (r == -1) {
                fprintf(stderr, "Error: Reading from input\n");
                goto f1;
            }
            data.array[dp] = c;
        } break;
        case OP_OUTPUT: {
            char c = data.array[dp];
            for (size_t i = 0; i < op.operand; ++i) {
                int r = write(fd_out, &c, sizeof(c));
                if (r == -1) {
                    fprintf(stderr, "Error: Writing to output\n");
                    goto f1;
                }
            }
        } break;
        default:
            __builtin_unreachable();
        }
    }

    free(data.array);
    return true;

f1:
    free(data.array);
    return false;
}
