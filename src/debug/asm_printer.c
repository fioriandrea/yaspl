#include <stdio.h>

#include "asm_printer.h"
#include "../util.h"
#include "../commontypes.h"
#include "../datastructs/value.h"
#include "../datastructs/object.h"

static int printSimpleInstruction(char* instname, int offset) {
    printf("%s\n", instname);
    return offset + 1;
}

static int printAddressedInstruction(char* instname, Chunk* chunk, int offset) {
    uint8_t address = chunk->code[offset + 1];
    Value* val = &chunk->constants.values[address];
    printf("%s [%d] '", instname, address);
    printValue(*val);
    printf("'\n");
    return offset + 2;
}

static int printAddressedLongInstruction(char* instname, Chunk* chunk, int offset) {
    uint16_t address = join_bytes(chunk->code[offset + 1], chunk->code[offset + 2]);
    Value* val = &chunk->constants.values[address];
    printf("%s [%d] '", instname, address);
    printValue(*val);
    printf("'\n");
    return offset + 3;
}

static int printArgumentedInstruction(char* instname, Chunk* chunk, int offset) {
    uint8_t arg = chunk->code[offset + 1];
    printf("%s arg:[%d]\n", instname, arg);
    return offset + 2;
}

static int printArgumentedLongInstruction(char* instname, Chunk* chunk, int offset) {
    uint16_t arg = join_bytes(chunk->code[offset + 1], chunk->code[offset + 2]);
    printf("%s arg:[%d]\n", instname, arg);
    return offset + 3;
}

void printChunk(Chunk* chunk, char* name) {
    printf("chunk => %s\n", name);
    for (int i = 0; i < chunk->count; ) {
        i = printInstruction(chunk, chunk->code[i], i);
    }
}

int printInstruction(Chunk* chunk, OpCode code, int offset) {
#define print_simple_instruction(op) case op: return printSimpleInstruction(#op, offset);
#define print_addressed_instruction(op) case op: return printAddressedInstruction(#op, chunk, offset);
#define print_addressed_long_instruction(op) case op: return printAddressedLongInstruction(#op, chunk, offset);
#define print_argumented_instruction(op) case op: return printArgumentedInstruction(#op, chunk, offset);
#define print_argumented_long_instruction(op) case op: return printArgumentedLongInstruction(#op, chunk, offset);
#define print_closure(op, l) \
    case op: \
             { \
                 offset++; \
                 uint16_t address = l ? join_bytes(chunk->code[offset++], chunk->code[offset++]) : chunk->code[offset++]; \
                 Value funVal = chunk->constants.values[address]; \
                 printf("%s [%d] '", #op, address); \
                 printValue(funVal); \
                 printf("'\n"); \
                 ObjFunction* function = as_function(funVal); \
                 for (int i = 0; i < function->upvalueCount; i++) { \
                     int ownedAbove = chunk->code[offset++]; \
                     int index = chunk->code[offset++]; \
                     printf("index -> %d [%s]\n", index, ownedAbove ? "ownedAbove" : "ownedHere"); \
                 } \
                 return offset; \
             } 

    printf("line = %d: ", lineArrayGet(&chunk->lines, offset));
    switch (code) {
        print_closure(OP_CLOSURE, 0)
            print_closure(OP_CLOSURE_LONG, 1)
            print_addressed_instruction(OP_CONST)
            print_addressed_long_instruction(OP_CONST_LONG)
            print_addressed_instruction(OP_GLOBAL_DECL)
            print_addressed_long_instruction(OP_GLOBAL_DECL_LONG)
            print_addressed_instruction(OP_GLOBAL_GET)
            print_addressed_long_instruction(OP_GLOBAL_GET_LONG)
            print_addressed_instruction(OP_GLOBAL_SET)
            print_addressed_long_instruction(OP_GLOBAL_SET_LONG)
            print_argumented_instruction(OP_LOCAL_GET)
            print_argumented_long_instruction(OP_LOCAL_GET_LONG)
            print_argumented_instruction(OP_LOCAL_SET)
            print_argumented_long_instruction(OP_LOCAL_SET_LONG)
            print_argumented_instruction(OP_UPVALUE_GET)
            print_argumented_long_instruction(OP_UPVALUE_GET_LONG)
            print_argumented_instruction(OP_UPVALUE_SET)
            print_argumented_long_instruction(OP_UPVALUE_SET_LONG)
            print_argumented_long_instruction(OP_JUMP_IF_FALSE)
            print_argumented_long_instruction(OP_JUMP_IF_TRUE)
            print_argumented_long_instruction(OP_JUMP)
            print_argumented_long_instruction(OP_JUMP_BACK)
            print_argumented_instruction(OP_CALL)
            print_argumented_instruction(OP_ARRAY)
            print_argumented_long_instruction(OP_ARRAY_LONG)
            print_argumented_instruction(OP_DICT)
            print_argumented_long_instruction(OP_DICT_LONG)
            print_simple_instruction(OP_RET)
            print_simple_instruction(OP_CLOSE_UPVALUE)
            print_simple_instruction(OP_INDEXING_GET)
            print_simple_instruction(OP_INDEXING_SET)
            print_simple_instruction(OP_XOR)
            print_simple_instruction(OP_NEGATE)
            print_simple_instruction(OP_ADD)
            print_simple_instruction(OP_SUB)
            print_simple_instruction(OP_MUL)
            print_simple_instruction(OP_DIV)
            print_simple_instruction(OP_MOD)
            print_simple_instruction(OP_POW)
            print_simple_instruction(OP_CONST_TRUE)
            print_simple_instruction(OP_CONST_FALSE)
            print_simple_instruction(OP_CONST_NIHL)
            print_simple_instruction(OP_NOT)
            print_simple_instruction(OP_POP)
            print_simple_instruction(OP_LESS)
            print_simple_instruction(OP_LESS_EQUAL)
            print_simple_instruction(OP_GREATER)
            print_simple_instruction(OP_GREATER_EQUAL)
            print_simple_instruction(OP_EQUAL)
            print_simple_instruction(OP_CONCAT)
            print_simple_instruction(OP_PRINT)
        default:
            printf("Undefined instruction: [opcode = %d]\n", code);
            return offset + 1;
    }
#undef print_addressed_instruction
#undef print_addressed_long_instruction
#undef print_simple_instruction
#undef print_argumented_instruction
#undef print_argumented_long_instruction
#undef print_closure
}