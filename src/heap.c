/* heap.c — array-backed binary min-heap ordered by expire_at.
 *
 * THE KEY IDEA: we store a binary tree inside a flat array. For the element at
 * index i:
 *     parent(i)      = (i - 1) / 2
 *     left_child(i)  = 2*i + 1
 *     right_child(i) = 2*i + 2
 * No pointers, no tree nodes — just arithmetic on indices. The heap property:
 * every parent's expire_at <= both children's. So the minimum is always at
 * index 0 (the root).
 */
#include "heap.h"

#include <stdlib.h>
#include <string.h>

#define HEAP_INITIAL_CAP 16

struct Heap {
    HeapEntry *data;    /* the flat array holding the tree */
    size_t size;        /* number of entries in use */
    size_t capacity;    /* allocated slots */
};

/* our own string copy (standard-C, no strdup) */
static char *heap_dup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy != NULL) memcpy(copy, s, len);
    return copy;
}

/* swap two entries in the array */
static void swap_entries(HeapEntry *a, HeapEntry *b) {
    HeapEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

Heap *heap_create(void) {
    Heap *h = malloc(sizeof(Heap));
    if (h == NULL) return NULL;
    h->data = malloc(HEAP_INITIAL_CAP * sizeof(HeapEntry));
    if (h->data == NULL) {
        free(h);
        return NULL;
    }
    h->size = 0;
    h->capacity = HEAP_INITIAL_CAP;
    return h;
}

void heap_free(Heap *heap) {
    if (heap == NULL) return;
    /* free every key copy still in the heap */
    for (size_t i = 0; i < heap->size; i++) {
        free(heap->data[i].key);
    }
    free(heap->data);
    free(heap);
}

/* grow the array when full (double it) */
static bool heap_grow(Heap *heap) {
    size_t new_cap = heap->capacity * 2;
    HeapEntry *new_data = realloc(heap->data, new_cap * sizeof(HeapEntry));
    if (new_data == NULL) return false;
    heap->data = new_data;
    heap->capacity = new_cap;
    return true;
}

bool heap_push(Heap *heap, const char *key, uint64_t expire_at) {
    if (heap->size == heap->capacity) {
        if (!heap_grow(heap)) return false;
    }

    /* copy the key first; if that fails we haven't touched the heap yet */
    char *key_copy = heap_dup(key);
    if (key_copy == NULL) return false;

    /* place new entry at the end (bottom of the tree) */
    size_t i = heap->size;
    heap->data[i].key = key_copy;
    heap->data[i].expire_at = expire_at;
    heap->size++;

    /* BUBBLE UP: while this node is smaller than its parent, swap them.
     * This restores "parent <= child" up the path to the root. */
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (heap->data[i].expire_at < heap->data[parent].expire_at) {
            swap_entries(&heap->data[i], &heap->data[parent]);
            i = parent;              /* follow the node up */
        } else {
            break;                   /* parent is smaller: heap property holds */
        }
    }
    return true;
}

bool heap_peek_min(Heap *heap, uint64_t *out_expire_at) {
    if (heap->size == 0) return false;
    *out_expire_at = heap->data[0].expire_at;   /* min is always at the root */
    return true;
}

bool heap_pop_min(Heap *heap, char **out_key, uint64_t *out_expire_at) {
    if (heap->size == 0) return false;

    /* the root is the answer */
    *out_key = heap->data[0].key;                /* ownership transfers out */
    *out_expire_at = heap->data[0].expire_at;

    /* move the LAST element to the root, shrink, then sift it down */
    heap->size--;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];

        /* SIFT DOWN: swap with the SMALLER child while a child is smaller than
         * us, until both children are >= us (or we hit the bottom). */
        size_t i = 0;
        while (true) {
            size_t left = 2 * i + 1;
            size_t right = 2 * i + 2;
            size_t smallest = i;

            if (left < heap->size &&
                heap->data[left].expire_at < heap->data[smallest].expire_at) {
                smallest = left;
            }
            if (right < heap->size &&
                heap->data[right].expire_at < heap->data[smallest].expire_at) {
                smallest = right;
            }
            if (smallest == i) break;            /* both children >= us: done */

            swap_entries(&heap->data[i], &heap->data[smallest]);
            i = smallest;                        /* follow the node down */
        }
    }
    return true;
}

size_t heap_size(Heap *heap) {
    return heap->size;
}
