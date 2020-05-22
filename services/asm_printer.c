#include <stdio.h>

#include "asm_printer.h"
#include "../standardtypes.h"
#include "../datastructs/value.h"
#include "../datastructs/object.h"

static int printSimpleInstruction(char* instname, int offset) {
    printf("%s\n", instname);
    return offset + 1;
}

void printObj(Value val) {
    Obj* obj = as_obj(val);
    switch (obj->type) {
        case OBJ_STRING:
            printf("%s", as_cstring(val));
            break;
    }
}

void printValue(Value val) {
    switch (val.type) {
        case VALUE_BOOL:
            printf("%s", as_cbool(val) ? "true" : "false");
            break;
        case VALUE_NUMBER:
            printf("%g", as_cnumber(val));
            break;
        case VALUE_NIHL:
            printf("nihl");
            break;
        case VALUE_OBJ:
            printObj(val);
    }
}

static int printConstantInstruction(char* instname, Chunk* chunk, int offset) {
    uint8_t address = chunk->code[offset + 1];
    Value* val = &chunk->constants.values[address];
    printf("%s [%d] '", instname, address);
    printValue(*val);
    printf("'\n");
    return offset + 2;
}

void printChunk(Chunk* chunk, char* name) {
    printf("chunk => %s\n", name);
    for (int i = 0; i < chunk->count; ) {
        i = printInstruction(chunk, chunk->code[i], i);
    }
}

int printInstruction(Chunk* chunk, OpCode code, int offset) {
    printf("line = %d: ", lineArrayGet(&chunk->lines, offset));
    switch (code) {
        case OP_RET:
            return printSimpleInstruction("OP_RET", offset);
        case OP_CONST:
            return printConstantInstruction("OP_CONST", chunk, offset);
        case OP_NEGATE:
            return printSimpleInstruction("OP_NEGATE", offset);
        case OP_ADD:
            return printSimpleInstruction("OP_ADD", offset);
        case OP_SUB:
            return printSimpleInstruction("OP_SUB", offset);
        case OP_MUL:
            return printSimpleInstruction("OP_MUL", offset);
        case OP_DIV:
            return printSimpleInstruction("OP_DIV", offset);
        case OP_MOD:
            return printSimpleInstruction("OP_MOD", offset);
        case OP_POW:
            return printSimpleInstruction("OP_POW", offset);
        case OP_CONST_TRUE:
            return printSimpleInstruction("OP_CONST_TRUE", offset);
        case OP_CONST_FALSE:
            return printSimpleInstruction("OP_CONST_FALSE", offset);
        case OP_CONST_NIHL:
            return printSimpleInstruction("OP_CONST_NIHL", offset);
        case OP_NOT:
            return printSimpleInstruction("OP_NOT", offset);
        case OP_POP:
            return printSimpleInstruction("OP_POP", offset);
        case OP_LESS:
            return printSimpleInstruction("OP_LESS", offset);
        case OP_LESS_EQUAL:
            return printSimpleInstruction("OP_LESS_EQUAL", offset);
        case OP_GREATER:
            return printSimpleInstruction("OP_GREATER", offset);
        case OP_GREATER_EQUAL:
            return printSimpleInstruction("OP_GREATER_EQUAL", offset);
        case OP_EQUAL:
            return printSimpleInstruction("OP_EQUAL", offset);
        case OP_CONCAT:
            return printSimpleInstruction("OP_CONCAT", offset);
        default:
            printf("Undefined instruction: [opcode = %d]\n", code);
            return offset + 1;
    }
}
