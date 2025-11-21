// Lock-free shared buffer for PSXInputState between core0 (writer) and core1 (reader)
// Uses a version sequence counter: even = stable, odd = writing
// Small, header-only helper with C11 atomics.
#pragma once

#include <stdatomic.h>
#include <string.h>
#include "device_files/psx-device/controller_simulator.h"

typedef struct {
    atomic_uint seq;
    PSXInputState data;
} PSXSharedBuffer;

// Exported shared buffer instance (define in main/writer file)
extern PSXSharedBuffer g_psx_shared;

static inline void psx_shared_init(PSXSharedBuffer *b) {
    atomic_init(&b->seq, 0u);
    // initialize data to defaults
    memset(&b->data, 0xFF, sizeof(b->data));
}

static inline void psx_shared_write(PSXSharedBuffer *b, const PSXInputState *src) {
    // increment to odd = writing
    atomic_fetch_add_explicit(&b->seq, 1u, memory_order_acq_rel);
    // copy data
    memcpy(&b->data, src, sizeof(b->data));
    // increment to even = written
    atomic_fetch_add_explicit(&b->seq, 1u, memory_order_release);
}

static inline void psx_shared_read(const PSXSharedBuffer *b, PSXInputState *dst) {
    // Read loop: load seq before and after copy and retry if changed
    for (;;) {
        unsigned int v1 = atomic_load_explicit(&b->seq, memory_order_acquire);
        if (v1 & 1u) continue; // currently being written
        memcpy(dst, &b->data, sizeof(b->data));
        unsigned int v2 = atomic_load_explicit(&b->seq, memory_order_acquire);
        if (v1 == v2) return; // stable copy
        // otherwise retry
    }
}
