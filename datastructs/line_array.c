#include "../standardtypes.h"
#include "line_array.h"
#include "../services/memory.h"

void initLineArray(LineArray* linearr) {
    linearr->count = 0;
    linearr->capacity = 0;
    linearr->lines = NULL;
}

int writeLineArray(LineArray* linearr, int line) {
    if (linearr->count + 1 >= linearr->capacity) {
        int newcap = compute_capacity(linearr->capacity);
        linearr->lines = grow_array(LineData, linearr->lines, linearr->capacity, newcap);
        linearr->capacity = newcap; 
    }
    if (linearr->count == 0 || linearr->count > 0 && linearr->lines[linearr->count - 1].line != line) {
        linearr->lines[linearr->count].line = line;
        linearr->lines[linearr->count].count = 1;
        linearr->count++;
    } else {
        linearr->lines[linearr->count - 1].count++; 
    }
    return linearr->count - 1;
}

void freeLineArray(LineArray* linearr) {
    free_array(int, linearr->lines, linearr->capacity);
    initLineArray(linearr);
}

int lineArrayGet(LineArray* linearr, int index) {
    int accumulator = 0;
    int i = 0;
    for (; i < linearr->count; i++) {
        if (accumulator >= index) 
            break;
        accumulator += linearr->lines[i].count;
    }
    return linearr->lines[i].line;
}