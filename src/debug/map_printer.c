#include <stdio.h>

#include "map_printer.h"
#include "../datastructs/value.h"

void printEntry(Entry* entry) {
    dumpValue(entry->key);
    printf(" => ");
    dumpValue(entry->value);
}

static void printBucket(Entry* head) {
    printf("    bucket:\n");
    while (head != NULL) {
        printf("    ");
        printEntry(head);
        printf(";\n");
        head = head->next;
    }
}

void printMap(HashMap* map) {
    printf("{\n");
    for (int i = 0; i < map->capacity; i++) {
        if (map->entries[i] != NULL) {
            printBucket(map->entries[i]);
        }
    }
    printf("}\n");
}
