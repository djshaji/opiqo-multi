
/*
 * lv2_ringbuffer.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#pragma once

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

/****************************************************************
        lv2_ringbuffer.h - inspired by jack_ringbuffer

****************************************************************/

typedef struct {
    uint8_t* buf;
    size_t   size;
    size_t   size_mask;

    alignas(64) std::atomic<size_t> write_ptr;
    alignas(64) std::atomic<size_t> read_ptr;
} lv2_ringbuffer_t;

static inline bool is_power_of_two(size_t x) {
    return x && !(x & (x - 1));
}

static inline lv2_ringbuffer_t* lv2_ringbuffer_create(size_t sz) {

    if (!is_power_of_two(sz)) return nullptr;

    lv2_ringbuffer_t* rb =
        (lv2_ringbuffer_t*)std::calloc(1, sizeof(lv2_ringbuffer_t));

    if (!rb) return nullptr;

    rb->buf = (uint8_t*)aligned_alloc(64, sz);
    if (!rb->buf) {
        std::free(rb);
        return nullptr;
    }

    rb->size = sz;
    rb->size_mask = sz - 1;
    rb->write_ptr.store(0, std::memory_order_relaxed);
    rb->read_ptr.store(0, std::memory_order_relaxed);

    return rb;
}

static inline void lv2_ringbuffer_free(lv2_ringbuffer_t* rb) {

    if (!rb) return;
    std::free(rb->buf);
    std::free(rb);
}

static inline void lv2_ringbuffer_reset(lv2_ringbuffer_t* rb) {

    rb->write_ptr.store(0, std::memory_order_release);
    rb->read_ptr.store(0, std::memory_order_release);
}

static inline size_t lv2_ringbuffer_read_space(const lv2_ringbuffer_t* rb) {

    size_t w = rb->write_ptr.load(std::memory_order_acquire);
    size_t r = rb->read_ptr.load(std::memory_order_relaxed);
    return w - r;
}

static inline size_t lv2_ringbuffer_write_space(const lv2_ringbuffer_t* rb) {

    return rb->size - lv2_ringbuffer_read_space(rb);
}

static inline size_t lv2_ringbuffer_peek(lv2_ringbuffer_t* rb,
                                        char* dst, size_t cnt) {

    size_t avail = lv2_ringbuffer_read_space(rb);
    if (cnt > avail) cnt = avail;
    size_t r = rb->read_ptr.load(std::memory_order_relaxed);

    for (size_t i = 0; i < cnt; ++i)
        dst[i] = rb->buf[(r + i) & rb->size_mask];

    return cnt;
}

static inline size_t lv2_ringbuffer_read(lv2_ringbuffer_t* rb,
                                        char* dst, size_t cnt) {

    cnt = lv2_ringbuffer_peek(rb, dst, cnt);
    rb->read_ptr.fetch_add(cnt, std::memory_order_release);
    return cnt;
}

static inline size_t lv2_ringbuffer_write(lv2_ringbuffer_t* rb,
                                        const char* src, size_t cnt) {

    size_t space = lv2_ringbuffer_write_space(rb);
    if (cnt > space) cnt = space;
    size_t w = rb->write_ptr.load(std::memory_order_relaxed);

    for (size_t i = 0; i < cnt; ++i)
        rb->buf[(w + i) & rb->size_mask] = src[i];

    rb->write_ptr.fetch_add(cnt, std::memory_order_release);
    return cnt;
}

