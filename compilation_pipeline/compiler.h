#ifndef compiler_h
#define compiler_h

#include "../datastructs/chunk.h"
#include "../memory.h"
#include "lexer.h"
#include "../datastructs/hash_map.h"

struct sLocal {
    Token name;
    int depth;
    int isCaptured;
};

struct sUpvalue {
    int index;
    int ownedAbove;
};

typedef struct sLocal Local;
typedef struct sUpvalue Upvalue;

struct sScope {
    struct sScope* enclosing;
    int depth;
    Local locals[700]; // todo: change constant
    int count;
    ObjFunction* function;
    Upvalue upvalues[700];
};

typedef struct sScope Scope;

typedef struct {
    Lexer lexer;
    Token current;
    Token previous;
    Collector* collector;
    int hadError;
    int panic;
    Scope *scope;
} Compiler;

void initCompiler(Compiler* compiler);
ObjFunction* compile(Compiler* compiler, Collector* collector, char* source);
void freeCompiler(Compiler* compiler);

#endif
