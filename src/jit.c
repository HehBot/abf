#include "bf.h"
#include "dynarr.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define splay64(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff, ((x) >> 32) & 0xff, ((x) >> 40) & 0xff, ((x) >> 48) & 0xff, ((x) >> 56) & 0xff
#define splay32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

buf_t jitc(bf_ops_t bf)
{
    // assuming all code fits in size UINT32_MAX
    struct {
        uint8_t* array;
        size_t count;
        size_t capacity;
    } code = { 0 };

    // JIT ****************************************************************
    // callee-saved regs are rbx, rbp, r12-r15

    //      [input]
    //      rdi = fd_in
    //      rsi = fd_out
    //      rdx = data_array_ptr
    //      rcx = data_size

    // push rbx; push r12; push r13; push r14; push r15
    // mov r12, rdi
    // mov r13, rsi
    // mov rdi, qword [rdx]
    // mov rbx, rdx
    // xor rdx, rdx
    // mov r14, rcx
    dynarr_append(&code, 0x53, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57);
    dynarr_append(&code, 0x49, 0x89, 0xfc);
    dynarr_append(&code, 0x49, 0x89, 0xf5);
    dynarr_append(&code, 0x48, 0x8b, 0x3a);
    dynarr_append(&code, 0x48, 0x89, 0xd3);
    dynarr_append(&code, 0x48, 0x31, 0xd2);
    dynarr_append(&code, 0x49, 0x89, 0xce);

    //      [state]
    //      r12 = fd_in
    //      r13 = fd_out
    //      rbx = data_array_ptr
    //      r14 = data_size
    //      rdi = data_array
    //      rdx = dp

    // jmp after_bad
    dynarr_append(&code, 0xeb, 0x00);
    int8_t* jmp_after_bad_amt = (void*)&code.array[code.count - 1];

    // bad:
    uint32_t bad_offset = code.count;

    // pop r15, pop r14; pop r13; pop r12; pop rbx
    // mov rdx, <&errno as u64>
    // neg eax
    // mov dword [rdx], eax
    // movzx eax, cl
    // ret
    dynarr_append(&code, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5b);
    dynarr_append(&code, 0x48, 0xba, splay64((uintptr_t)&errno));
    dynarr_append(&code, 0xf7, 0xd8);
    dynarr_append(&code, 0x89, 0x02);
    dynarr_append(&code, 0x0f, 0xb6, 0xc1);
    dynarr_append(&code, 0xc3);
    // after_bad:

    uint32_t after_bad_offset = code.count;
    *jmp_after_bad_amt = after_bad_offset - bad_offset;

    struct {
        uint32_t* array;
        size_t count;
        size_t capacity;
    } jmp_cond_backpatch_list = { 0 }, jmp_end_backpatch_list = { 0 };

    for (size_t i = 0; i < bf.count; ++i) {
        bf_op_t op = bf.array[i];
        switch (op.type) {
        case OP_INC:
            // add byte [rdi + rdx], <op.operand as u8>
            dynarr_append(&code, 0x80, 0x04, 0x17, (uint8_t)op.operand);
            break;
        case OP_DEC:
            // sub byte [rdi + rdx], <op.operand as u8>
            dynarr_append(&code, 0x80, 0x2c, 0x17, (uint8_t)op.operand);
            break;
        case OP_LEFT:
            // mov cl, <BF_ERR_LEFT_OF_FIRST_CELL as u8>
            dynarr_append(&code, 0xb1, BF_ERR_LEFT_OF_FIRST_CELL);
            // mov rax, <op.operand as u64/32>
            if ((op.operand >> 32) == 0) {
                dynarr_append(&code, 0xb8, splay32(op.operand));
            } else {
                dynarr_append(&code, 0x48, 0xb8, splay64(op.operand));
            }
            // sub rdx, rax
            // jl bad
            dynarr_append(&code, 0x48, 0x29, 0xc2);
            dynarr_append(&code, 0x0f, 0x8c, 0x00, 0x00, 0x00, 0x00);
            *(int32_t*)&code.array[code.count - 4] = (int32_t)code.count - (int32_t)bad_offset;
            break;
        case OP_RIGHT:
            // mov rax, <op.operand as u64>
            if ((op.operand >> 32) == 0) {
                dynarr_append(&code, 0xb8, splay32(op.operand));
            } else {
                dynarr_append(&code, 0x48, 0xb8, splay64(op.operand));
            }
            // add rdx, rax
            dynarr_append(&code, 0x48, 0x01, 0xc2);
            break;
        case OP_JZ:
            // cmp byte [rdi + rdx], 0x0
            // jz <to be backpatched>
            dynarr_append(&code, 0x80, 0x3c, 0x17, 0x00);
            dynarr_append(&code, 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00);
            dynarr_append(&jmp_cond_backpatch_list, code.count);
            break;
        case OP_JNZ:
            // cmp byte [rdi + rdx], 0x0
            // jnz <to be backpatched>
            dynarr_append(&code, 0x80, 0x3c, 0x17, 0x00);
            dynarr_append(&code, 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00);
            {
                uint32_t curr_offset = code.count;
                uint32_t target_offset = jmp_cond_backpatch_list.array[jmp_cond_backpatch_list.count - 1];
                int32_t jump = (int32_t)target_offset - (int32_t)curr_offset;
                *(int32_t*)&code.array[curr_offset - 4] = jump;
                *(int32_t*)&code.array[target_offset - 4] = -jump;
                jmp_cond_backpatch_list.count--;
            }
            break;
        case OP_OUTPUT:
            // mov r15, rdx
            // lea rsi, byte [rdi + rdx]
            // mov rdi, r13
            // mov rdx, 1
            // mov eax, 1
            // mov cl, <BF_ERR_WRITE as u8>
            //
            // mov r8, <op.operand as u64/u32>
            // loop:
            // syscall                      // hacky as we know linux syscall does not clobber registers
            // cmp eax, 1
            // jne bad
            // dec r8
            // jnz loop
            //
            // mov rdx, r15
            // mov rdi, qword [rbx]
            dynarr_append(&code, 0x49, 0x89, 0xd7);
            dynarr_append(&code, 0x48, 0x8d, 0x34, 0x17);
            dynarr_append(&code, 0x4c, 0x89, 0xef);
            dynarr_append(&code, 0xba, 0x01, 0x00, 0x00, 0x00);
            dynarr_append(&code, 0xb8, 0x01, 0x00, 0x00, 0x00);
            dynarr_append(&code, 0xb1, BF_ERR_WRITE);

            if (op.operand > 1) {
                if ((op.operand >> 32) == 0) {
                    dynarr_append(&code, 0x41, 0xb8, splay32(op.operand));
                } else {
                    dynarr_append(&code, 0x49, 0xb8, splay64(op.operand));
                }
            }
            uint32_t loop_offset = code.count;
            dynarr_append(&code, 0x0f, 0x05);
            dynarr_append(&code, 0x83, 0xf8, 0x01);
            dynarr_append(&code, 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00);
            *(int32_t*)&code.array[code.count - 4] = (int32_t)bad_offset - (int32_t)code.count;
            if (op.operand > 1) {
                dynarr_append(&code, 0x49, 0xff, 0xc8);
                dynarr_append(&code, 0x0f, 0x85, 0x00, 0x00, 0x00, 0x00);
                *(int32_t*)&code.array[code.count - 4] = (int32_t)loop_offset - (int32_t)code.count;
            }

            dynarr_append(&code, 0x4c, 0x89, 0xfa);
            dynarr_append(&code, 0x48, 0x8b, 0x3b);
            break;
        case OP_INPUT:
            // mov r15, rdx
            // lea rsi, byte [rdi + rdx]
            // mov rdi, r12
            // mov rdx, 1
            // xor eax, eax
            // syscall
            //
            // mov cl, <BF_ERR_READ as u8>
            // cmp eax, 0
            // jl bad
            // je postamble
            //
            // mov rdx, r15
            // mov rdi, qword [rbx]
            dynarr_append(&code, 0x49, 0x89, 0xd7);
            dynarr_append(&code, 0x48, 0x8d, 0x34, 0x17);
            dynarr_append(&code, 0x4c, 0x89, 0xe7);
            dynarr_append(&code, 0xba, 0x01, 0x00, 0x00, 0x00);
            dynarr_append(&code, 0x31, 0xc0);
            dynarr_append(&code, 0x0f, 0x05);

            dynarr_append(&code, 0xb1, BF_ERR_READ);
            dynarr_append(&code, 0x83, 0xf8, 0x00);
            dynarr_append(&code, 0x0f, 0x8c, 0x00, 0x00, 0x00, 0x00);
            *(int32_t*)&code.array[code.count - 4] = (int32_t)bad_offset - (int32_t)code.count;
            dynarr_append(&code, 0x0f, 0x84, 0x00, 0x00, 0x00, 0x00);
            dynarr_append(&jmp_end_backpatch_list, code.count);

            dynarr_append(&code, 0x4c, 0x89, 0xfa);
            dynarr_append(&code, 0x48, 0x8b, 0x3b);
            break;
        default:
            __builtin_unreachable();
        }
    }

    // end:
    uint32_t postamble_offset = code.count;

    for (size_t i = 0; i < jmp_end_backpatch_list.count; ++i) {
        uint32_t offset = jmp_end_backpatch_list.array[i];
        *(int32_t*)&code.array[offset - 4] = (int32_t)postamble_offset - (int32_t)offset;
    }
    free(jmp_end_backpatch_list.array);

    // pop r15; pop r14; pop r13; pop r12; pop rbx
    // xor eax, eax
    // mov al, <BF_OK as u8> // emitted if BF_OK != 0
    // ret
    dynarr_append(&code, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5b);
    dynarr_append(&code, 0x31, 0xc0);
    if (BF_OK != 0)
        dynarr_append(&code, 0xb0, BF_OK);
    dynarr_append(&code, 0xc3);

    // END - JIT **********************************************************

    if (code.count > UINT32_MAX)
        goto bad;

    free(jmp_cond_backpatch_list.array);
    return (buf_t) { .b = code.array, .sz = code.count };

bad:
    free(jmp_cond_backpatch_list.array);
    free(code.array);
    return (buf_t) { .b = NULL, .sz = 0 };
}
