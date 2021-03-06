#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "./debug/debug_switches.h"

static void collectGarbage(struct sCollector* collector) {
#ifdef TRACE_GC
    printf("START GC\n");
    size_t oldAllocatedBytes = collector->allocatedBytes;
#endif

    // mark stack
    for (Value* stackValue = collector->vm->stack; stackValue < collector->vm->sp; stackValue++) {
        markValue(collector, *stackValue);
    }

    // mark globals

    markMap(collector, &collector->vm->globals);

    // todo: mark frames (mark main script function)
    
    // mark open upvalues

    for (ObjUpvalue* upvalue = collector->vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
        markObject(collector, (Obj*) upvalue);
    }

    // blacken
    
    for (int i = 0; i < collector->worklistCount; i++) {
        blackenObject(collector, collector->worklist[i]);
    }
    collector->worklistCount = 0;

    // remove unmarked interned

    removeUnmarkedKeys(collector, &collector->interned);

    // sweep

    Obj* previous = NULL;         
    Obj* object = collector->objects;     
    while (object != NULL) {
        if (!object->marked) {
            Obj* toFree = object;
            if (previous == NULL) {
                object = object->next;
                collector->objects = object;
            } else {
                previous->next = object->next;
                object = object->next;
            }
            freeObject(NULL, toFree);
        } else {
            object->marked = 0;
            previous = object;
            object = object->next;
        }
    }

    // update threshold
    
    collector->triggerGCThreshold = collector->allocatedBytes * GC_TRESHOLD_FACTOR;
#ifdef TRACE_GC
    printf("freed bytes: %d\n", oldAllocatedBytes - collector->allocatedBytes);
    printf("END GC\n");
#endif 
}

void* reallocate(Collector* collector, void* pointer, size_t oldsize, size_t newsize) {
    if (collector != NULL) {
        collector->allocatedBytes += newsize - oldsize;
        if (collector->vm != NULL && oldsize < newsize) {
#ifndef STRESS_GC
            if (collector->allocatedBytes >= collector->triggerGCThreshold) {
                collectGarbage(collector);
            }
#else
            collectGarbage(collector);
#endif
        }
    }
    if (newsize == 0) {
        free(pointer);
        return NULL;
    }
    return realloc(pointer, newsize);
}

void initCollector(Collector* collector) {
    collector->allocated = 0;
    collector->objects = NULL;
    collector->vm = NULL;
    collector->worklist = NULL;
    collector->worklistCount = 0;
    collector->worklistCapacity = 0;
    collector->allocatedBytes = 0;
    collector->triggerGCThreshold = BASE_TRIGGER_GC_THRESHOLD;
    initMap(&collector->interned);
}

void freeCollector(Collector* collector) {
    freeMap(NULL, &collector->interned);
    while (collector->objects != NULL) {
        Obj* next = collector->objects->next;
        freeObject(NULL, collector->objects);
        collector->objects = next;
    }
    if (collector->worklist != NULL)
        free(collector->worklist);
}

void pushSafe(struct sCollector* collector, Value value) {
    if (collector->vm != NULL)
        vmPush(collector->vm, value);
}
void popSafe(struct sCollector* collector) {
    if (collector->vm != NULL)
        vmPop(collector->vm);
}
