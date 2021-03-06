#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include "vm.h"
#include "memory.h"
#include "util.h"
#include "./datastructs/value.h"
#include "./compilation_pipeline/compiler.h"
#include "./debug/debug_switches.h"
#include "./datastructs/value_operations.h"
#include "./natives/natives_export.h"

#define RUNTIME_ERROR 0
#define RUNTIME_OK 1

static void resetStack(struct sVM* vm) {
    vm->sp = vm->stack;
}

void initVM(struct sVM* vm) {
    vm->fp = 0;
    resetStack(vm);
    initMap(&vm->globals);
    vm->openUpvalues = NULL;
    vm->collector = NULL;
}

void vmDeclareNative(struct sVM* vm, int arity, char* name, CNativeFunction cfunction) {
    ObjNativeFunction* native = newNativeFunction(vm->collector, arity, name, cfunction);
    pushSafeObj(vm->collector, native);
    mapPut(vm->collector, &vm->globals, to_vobj(native->name), to_vobj(native));
    popSafe(vm->collector);
}

void vmPush(struct sVM* vm, Value val) {
    *vm->sp = val;
    vm->sp++;
}

Value vmPop(struct sVM* vm) {
    vm->sp--;
    return *vm->sp;
}

static void runtimeError(struct sVM* vm, char* format, ...) {
    int instruction = vm->frames[vm->fp - 1].pc - vm->frames[vm->fp - 1].closure->function->bytecode->code - 1; 
    int line = lineArrayGet(&vm->frames[vm->fp - 1].closure->function->bytecode->lines, instruction);         

    va_list args;                                    
    va_start(args, format);                          
    fprintf(stderr, "runtime error [line %d] in program: ", line);  
    vfprintf(stderr, format, args);                  
    va_end(args);                                    
    fputs("\n", stderr);
    resetStack(vm);                                    
}    

static Value vmPeek(struct sVM* vm, int depth) {
    return vm->sp[-(depth + 1)];
}

static void closeOnStackUpvalue(struct sVM* vm, Value* value) {
    // todo: this can be optimized
    ObjUpvalue* prev = NULL;
    ObjUpvalue* current = vm->openUpvalues;
    if (current == NULL)
        return;
    if (current->value == value) {
        vm->openUpvalues = current->next;
        closeUpvalue(current);
    } else {
        prev = current;
        current = current->next;
        while (current != NULL) {
            if (current->value == value) {
                prev->next = current->next;
                closeUpvalue(current);
                break;
            }
            prev = prev->next;
            current = current->next;
        }
    }
}

static int callObject(struct sVM* vm, Obj* called, int argCount) {
    switch (called->type) {
        case OBJ_CLOSURE:
            {
                ObjClosure* closure = (ObjClosure*) called;
                ObjFunction* function = closure->function;
                if (argCount != function->arity) {
                    runtimeError(vm, "expected %d arguments, got %d", function->arity, argCount);
                    return 0;
                }
                if (vm->fp + 1 >= MAX_FRAMES) {             
                    runtimeError(vm, "stack overflow");             
                    return 0;                                
                } 
                CallFrame* currentFrame = &vm->frames[vm->fp++];
                currentFrame->closure = closure;
                currentFrame->pc = currentFrame->closure->function->bytecode->code;
                currentFrame->localStack = vm->sp - argCount;
                return 1;
            }
        case OBJ_NATIVE_FUNCTION:
            {
                ObjNativeFunction* native = (ObjNativeFunction*) called;
                if (argCount != native->arity) {
                    runtimeError(vm, "expected %d arguments, got %d", native->arity, argCount);
                    return 0;
                }
                Value result = native->cfunction(vm, vm->sp - argCount);
                vm->sp = vm->sp - argCount - 1; // -1 to pop off native
                vmPush(vm, result);
                return 1;
            }
    }
}

static int vmRun(struct sVM* vm) {
    vm->fp = 1;
    CallFrame* currentFrame = &vm->frames[0];
    OpCode caseCode;
#define read_byte() (*(currentFrame->pc++))
#define read_long() join_bytes(read_byte(), read_byte())
#define read_constant() (currentFrame->closure->function->bytecode->constants.values[read_byte()])
#define read_constant_long() (currentFrame->closure->function->bytecode->constants.values[read_long()])
#define read_long_if(oplong) (caseCode == (oplong) ? read_long() : read_byte())
#define read_constant_long_if(oplong) (caseCode == (oplong) ? read_constant_long() : read_constant())
#define binary_op(operator, destination) \
    do { \
        if (!valuesNumbers(vmPeek(vm, 0), vmPeek(vm, 1))) { \
            runtimeError(vm, "operand must be numbers"); \
            return RUNTIME_ERROR; \
        } \
        double b = as_cnumber(vmPop(vm)); \
        double a = as_cnumber(vmPop(vm)); \
        vmPush(vm, destination(a operator b)); \
    } while (0)

#ifdef TRACE_EXEC
    printf("VM EXECUTION TRACE:\n");
#endif

    for (;;) {
#ifdef TRACE_EXEC
        printf("\n");
        printInstruction(currentFrame->closure->function->bytecode, *currentFrame->pc, (int) (currentFrame->pc - currentFrame->closure->function->bytecode->code));
        printf("stack: [");
        for (Value* start = vm->stack; start < vm->sp; start++) {
            dumpValue(*start);
            printf(" | ");
        }
        printf("]\n");
        printf("\n");
#endif
#ifdef TRACE_OPEN_UPVALUES
        printf("OPEN UPVALUES\n");
        for (ObjUpvalue* uv = vm->openUpvalues; uv != NULL; uv = uv->next) {
            dumpValue(to_vobj(uv));
            printf("\n");
        }
        printf("END OPEN UPVALUES\n");
#endif
        switch ((caseCode = read_byte())) {
            case OP_RET: 
                {
                    Value retVal = vmPop(vm);
                    vm->fp--;
                    if (vm->fp == 0)
                        return RUNTIME_OK;
                    // close local variables still on the stack
                    while (vm->sp > currentFrame->localStack) { 
                        // todo this can be done more efficiently
                        closeOnStackUpvalue(vm, vm->sp - 1);
                        vm->sp--;
                    }
                    vmPop(vm); // pop returning function
                    currentFrame = &vm->frames[vm->fp - 1];
                    vmPush(vm, retVal);
                    break;
                }
            case OP_CALL:
                {
                    uint8_t argCount = read_byte();
                    if (argCount > (vm->sp - vm->stack)) {
                        runtimeError(vm, "too many function arguments");
                        return RUNTIME_ERROR;
                    }
                    Value called = vmPeek(vm, argCount);
                    if (!isCallable(called)) {
                        runtimeError(vm, "value is not callable");
                        return RUNTIME_ERROR;
                    }
                    if (!callObject(vm, as_obj(called), argCount)) {
                        return RUNTIME_ERROR;
                    }
                    currentFrame = &vm->frames[vm->fp - 1];
                    break;
                }
            case OP_INDEXING_GET:
                {
                    Value index = vmPeek(vm, 0);
                    Value arrayLike = vmPeek(vm, 1);
                    Value result = indexGetValue(vm->collector, arrayLike, index);
                    if (is_error(result)) {
                        runtimeError(vm, as_error(result)->message->chars);
                        return RUNTIME_ERROR;
                    }
                    vmPop(vm);
                    vmPop(vm);
                    vmPush(vm, result);
                    break;
                }
            case OP_INDEXING_SET:
                {
                    Value assignValue = vmPeek(vm, 0);
                    Value index = vmPeek(vm, 1);
                    Value arrayLike = vmPeek(vm, 2);
                    Value result = indexSetValue(vm->collector, arrayLike, index, assignValue);
                    if (is_error(result)) {
                        runtimeError(vm, as_error(result)->message->chars);
                        return RUNTIME_ERROR;
                    }
                    vmPop(vm);
                    vmPop(vm);
                    vmPop(vm);
                    vmPush(vm, result);
                    break;
                }
            case OP_CLOSURE:
            case OP_CLOSURE_LONG:
                {
                    Value funVal = read_constant_long_if(OP_CLOSURE_LONG);
                    ObjFunction* function = as_function(funVal);
                    ObjClosure* closure = newClosure(vm->collector, function);
                    vmPush(vm, to_vobj(closure));
                    for (int i = 0; i < closure->upvalueCount; i++) {
                        uint8_t ownedAbove = read_byte();
                        uint8_t index = read_byte();
                        if (ownedAbove) {
                            closure->upvalues[i] = currentFrame->closure->upvalues[index];
                        } else {
                            Value* value = currentFrame->localStack + index;
                            ObjUpvalue* current = vm->openUpvalues;
                            while (current != NULL) {
                                if (current->value == value)
                                    break;
                                current = current->next;
                            }
                            if (current == NULL) {
                                closure->upvalues[i] = 
                                    newUpvalue(vm->collector, currentFrame->localStack + index);
                                closure->upvalues[i]->next = vm->openUpvalues;
                                vm->openUpvalues = closure->upvalues[i];
                            } else {
                                closure->upvalues[i] = current;
                            }
                        }
                    }
                    break;
                }
            case OP_UPVALUE_GET:
            case OP_UPVALUE_GET_LONG:
                {
                    uint16_t index = read_long_if(OP_UPVALUE_GET_LONG);
                    vmPush(vm, *currentFrame->closure->upvalues[index]->value);
                    break;
                }
            case OP_UPVALUE_SET:
            case OP_UPVALUE_SET_LONG:
                {
                    uint16_t index = read_long_if(OP_UPVALUE_SET_LONG);
                    *currentFrame->closure->upvalues[index]->value = vmPeek(vm, 0);
                    break;
                }
            case OP_ARRAY:
            case OP_ARRAY_LONG:
                {
                    uint16_t len = read_long_if(OP_ARRAY_LONG);
                    ObjArray* array = newArray(vm->collector);
                    Value* nextsp = vm->sp - len;
                    Value* tmpsp = nextsp;
                    vmPush(vm, to_vobj(array));
                    while (len > 0) {
                        arrayPush(vm->collector, array, *(tmpsp++));
                        len--;
                    }
                    // no vmPop required due to vm->sp = nextsp
                    vm->sp = nextsp;
                    vmPush(vm, to_vobj(array));
                    break;
                }
            case OP_DICT:
            case OP_DICT_LONG:
                {
                    uint16_t len = read_long_if(OP_ARRAY_LONG);
                    ObjDict* dict = newDict(vm->collector);
                    Value* nextsp = vm->sp - len * 2;
                    Value* tmpsp = nextsp;
                    vmPush(vm, to_vobj(dict));
                    while (len > 0) {
                        Value* key = tmpsp++;
                        Value* value = tmpsp++;
                        indexSetDict(vm->collector, dict, key, value);
                        len--;
                    }
                    // no vmPop required due to vm->sp = nextsp
                    vm->sp = nextsp;
                    vmPush(vm, to_vobj(dict));
                    break;
                }
            case OP_CONST: 
            case OP_CONST_LONG:
                {
                    Value constant = read_constant_long_if(OP_CONST_LONG);
                    vmPush(vm, constant);
                    break;
                }
            case OP_GLOBAL_DECL:
            case OP_GLOBAL_DECL_LONG:
                {
                    Value name = read_constant_long_if(OP_GLOBAL_DECL_LONG);
                    mapPut(vm->collector, &vm->globals, name, vmPeek(vm, 0));
                    vmPop(vm); 
                    break;
                }
            case OP_GLOBAL_GET:
            case OP_GLOBAL_GET_LONG:
                {
                    Value name = read_constant_long_if(OP_GLOBAL_GET_LONG);
                    Value value;
                    int present = mapGet(&vm->globals, name, &value);
                    if (!present) {
                        runtimeError(vm, "cannot get value of undefined global variable");
                        return RUNTIME_ERROR;
                    } else {
                        vmPush(vm, value);
                    }
                    break;
                }
            case OP_GLOBAL_SET:
            case OP_GLOBAL_SET_LONG:
                {
                    Value name = read_constant_long_if(OP_GLOBAL_SET_LONG);
                    Value value;
                    if (!mapGet(&vm->globals, name, &value)) {
                        runtimeError(vm, "cannot assign undefined global variable");
                        return RUNTIME_ERROR;
                    } else {
                        mapPut(vm->collector, &vm->globals, name, vmPeek(vm, 0));
                    }
                    break;
                }
            case OP_LOCAL_GET:
            case OP_LOCAL_GET_LONG:
                {
                    uint16_t argument = read_long_if(OP_LOCAL_GET_LONG);
                    vmPush(vm, currentFrame->localStack[argument]);
                    break;
                }
            case OP_LOCAL_SET:
            case OP_LOCAL_SET_LONG:
                {
                    uint16_t argument = read_long_if(OP_LOCAL_SET_LONG);
                    currentFrame->localStack[argument] = vmPeek(vm, 0);
                    break;
                }
            case OP_JUMP_IF_FALSE:
                {
                    uint8_t* oldpc = currentFrame->pc - 1;
                    uint16_t argument = read_long();
                    if (!isTruthy(vmPeek(vm, 0)))
                        currentFrame->pc = oldpc + argument;
                    break;
                }
            case OP_JUMP_IF_TRUE:
                {
                    uint8_t* oldpc = currentFrame->pc - 1;
                    uint16_t argument = read_long();
                    if (isTruthy(vmPeek(vm, 0)))
                        currentFrame->pc = oldpc + argument;
                    break;
                }
            case OP_JUMP:
                {
                    uint8_t* oldpc = currentFrame->pc - 1;
                    uint16_t argument = read_long();
                    currentFrame->pc = oldpc + argument;
                    break;
                }
            case OP_JUMP_BACK:
                {
                    uint8_t* oldpc = currentFrame->pc - 1;
                    uint16_t argument = read_long();
                    currentFrame->pc = oldpc - argument;
                    break;
                }
            case OP_XOR:
                {
                    Value b = vmPeek(vm, 0);
                    Value a = vmPeek(vm, 1);
                    int bb = isTruthy(b);
                    int ba = isTruthy(a);
                    vmPop(vm);
                    vmPop(vm);
                    vmPush(vm, to_vbool(!(ba == bb)));
                    break;
                }
            case OP_NEGATE:
                {
                    if (!is_number(vmPeek(vm, 0))) {
                        runtimeError(vm, "only numbers can be negated");
                        return RUNTIME_ERROR;
                    }
                    Value a = vmPop(vm);
                    vmPush(vm, to_vnumber(-as_cnumber(a)));
                    break;
                }
            case OP_ADD:
                {
                    binary_op(+, to_vnumber);
                    break;      
                }
            case OP_SUB: 
                {
                    binary_op(-, to_vnumber);
                    break;      
                }
            case OP_MUL:
                {
                    binary_op(*, to_vnumber);
                    break;      
                }
            case OP_DIV:
                {
                    if (!valuesNumbers(vmPeek(vm, 0), vmPeek(vm, 1))) {
                        runtimeError(vm, "operands must be numbers");
                        return RUNTIME_ERROR;
                    }
                    if (as_cnumber(vmPeek(vm, 0)) == 0) {
                        runtimeError(vm, "cannot divide by zero (/ 0)");
                        return RUNTIME_ERROR;
                    }
                    double b = as_cnumber(vmPop(vm)); 
                    double a = as_cnumber(vmPop(vm)); 
                    vmPush(vm, to_vnumber(a / b));
                    break;
                }
            case OP_MOD:
                {
                    if (!valuesNumbers(vmPeek(vm, 0), vmPeek(vm, 1))) {
                        runtimeError(vm, "operands must be numbers");
                        return RUNTIME_ERROR;
                    }
                    if (as_cnumber(vmPeek(vm, 0)) == 0) {
                        runtimeError(vm, "cannot divide by 0 (% 0)");
                        return RUNTIME_ERROR;
                    }
                    if (!valuesIntegers(vmPeek(vm, 0), vmPeek(vm, 1))) {
                        runtimeError(vm, "only integer allowed when using %");
                        return RUNTIME_ERROR;
                    }

                    double b = as_cnumber(vmPop(vm)); 
                    double a = as_cnumber(vmPop(vm)); 
                    vmPush(vm, to_vnumber(((long) a) % ((long) b)));
                    break;
                }
            case OP_POW:
                {
                    if (!valuesNumbers(vmPeek(vm, 0), vmPeek(vm, 1))) {
                        runtimeError(vm, "operands must be numbers");
                        return RUNTIME_ERROR;
                    }
                    double b = as_cnumber(vmPop(vm)); 
                    double a = as_cnumber(vmPop(vm)); 
                    vmPush(vm, to_vnumber(pow(a, b)));
                    break;
                }
            case OP_CONST_NIHL:
                {
                    vmPush(vm, to_vnihl());
                    break;
                }
            case OP_CONST_TRUE:
                {
                    vmPush(vm, to_vbool(1));
                    break;
                }
            case OP_CONST_FALSE:
                {
                    vmPush(vm, to_vbool(0));
                    break;
                }
            case OP_NOT:
                {
                    Value val = vmPop(vm);
                    vmPush(vm, to_vbool(!isTruthy(val)));
                    break;
                }
            case OP_POP:
                {
                    vmPop(vm);
                    break;
                }
            case OP_CLOSE_UPVALUE:
                {
                    Value* value = vm->sp - 1; 
                    closeOnStackUpvalue(vm, value);
                    vmPop(vm);
                    break;
                }
            case OP_EQUAL:
                {
                    Value b = vmPop(vm);
                    Value a = vmPop(vm);
                    vmPush(vm, to_vbool(valuesEqual(a, b)));
                    break;
                }
            case OP_NOT_EQUAL:
                {
                    Value b = vmPop(vm);
                    Value a = vmPop(vm);
                    vmPush(vm, to_vbool(!valuesEqual(a, b)));
                    break;
                }
            case OP_LESS:
                {
                    binary_op(<, to_vbool);
                    break;
                }
            case OP_LESS_EQUAL:
                {
                    binary_op(<=, to_vbool);
                    break;
                }
            case OP_GREATER:
                {
                    binary_op(>, to_vbool);
                    break;
                }
            case OP_GREATER_EQUAL:
                {
                    binary_op(>=, to_vbool);
                    break;
                }
            case OP_CONCAT:
                {
                    Value b = vmPeek(vm, 0);
                    Value a = vmPeek(vm, 1);
                    Value result = concatenate(vm->collector, a, b);
                    if (is_error(result)) {
                        runtimeError(vm, as_error(result)->message->chars);
                        return RUNTIME_ERROR;
                    }
                    vmPop(vm);
                    vmPop(vm);
                    vmPush(vm, result);
                    break;
                }
            case OP_PRINT:
                {
                    Value val = vmPeek(vm, 0);
                    printValue(vm->collector, val);
                    printf("\n");
                    vmPop(vm);
                    break;
                }
            default:
                {
                    runtimeError(vm, "unknown instruction");
                    return RUNTIME_ERROR;
                    break;
                }
        }
    }
#undef read_byte
#undef read_constant
#undef read_constant_long
#undef read_long_if
#undef read_constant_long_if
}

int vmExecute(struct sVM* vm, Collector* collector, ObjFunction* function) {
    initVM(vm);
    CallFrame* initialFrame = &vm->frames[0];
    initialFrame->closure = newClosure(collector, function);
    initialFrame->pc = function->bytecode->code;
    initialFrame->localStack = vm->stack;
    mapPut(NULL, &vm->globals, to_vobj(initialFrame->closure), to_vnihl());

    vm->collector = collector;
    collector->vm = vm;

    declareNatives(vm);

    int result = vmRun(vm);
    freeVM(vm);
    return result;
}

void freeVM(struct sVM* vm) {
#ifdef TRACE_INTERNED
    printf("INTERNED:\n");
    printMap(&vm->collector->interned);
    printf("\n");
#endif
#ifdef TRACE_GLOBALS
    printf("GLOBALS:\n");
    printMap(&vm->globals);
    printf("\n");
#endif
    freeCollector(vm->collector);
    freeMap(NULL, &vm->globals);
}
