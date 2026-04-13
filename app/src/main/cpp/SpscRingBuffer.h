// Minimal single-producer single-consumer ring buffer (header-only)
// Designed for realtime audio: pre-allocated, lock-free for one producer + one consumer.
// Usage:
//   SpscRingBuffer<float> q;
//   q.init(65536); // capacity will be rounded up to power-of-two
//   q.push(data, n);
//   q.read(out, n);

#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <cassert>

template<typename T>
class SpscRingBuffer {
public:
    SpscRingBuffer() : capacity_(0), mask_(0) {}

    // Initialize with a requested capacity; actual capacity will be next power of two.
    void init(size_t requestedCapacity) {
        size_t cap = nextPowerOfTwo(std::max<size_t>(requestedCapacity, 2));
        buffer_.assign(cap, T());
        capacity_ = cap;
        mask_ = cap - 1;
        writeIdx_.store(0, std::memory_order_relaxed);
        readIdx_.store(0, std::memory_order_relaxed);
    }

    void reset() {
        writeIdx_.store(0, std::memory_order_relaxed);
        readIdx_.store(0, std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

    // Number of items currently stored.
    size_t size() const {
        uint64_t w = writeIdx_.load(std::memory_order_acquire);
        uint64_t r = readIdx_.load(std::memory_order_acquire);
        return static_cast<size_t>(w - r);
    }

    // Free space available for writing.
    size_t available() const {
        return capacity_ - size();
    }

    // Push up to n items from src. Returns true if all items were written; false if not enough space.
    bool push(const T* src, size_t n) {
        if (n == 0) return true;
        assert(buffer_.size() == capacity_);

        uint64_t w = writeIdx_.load(std::memory_order_relaxed);
        uint64_t r = readIdx_.load(std::memory_order_acquire);
        size_t used = static_cast<size_t>(w - r);
        if (capacity_ - used < n) return false; // not enough space

        size_t idx = static_cast<size_t>(w) & mask_;
        size_t first = std::min(n, capacity_ - idx);
        // copy first contiguous part
        if (first) std::memcpy(&buffer_[idx], src, first * sizeof(T));
        // copy wrap-around remainder
        if (n > first) std::memcpy(&buffer_[0], src + first, (n - first) * sizeof(T));

        // publish write index
        writeIdx_.store(w + n, std::memory_order_release);
        return true;
    }

    // Read up to n items into dst. Returns the number actually read (<= n).
    size_t read(T* dst, size_t n) {
        if (n == 0) return 0;
        uint64_t r = readIdx_.load(std::memory_order_relaxed);
        uint64_t w = writeIdx_.load(std::memory_order_acquire);
        size_t avail = static_cast<size_t>(w - r);
        size_t toRead = std::min(n, avail);
        if (toRead == 0) return 0;

        size_t idx = static_cast<size_t>(r) & mask_;
        size_t first = std::min(toRead, capacity_ - idx);
        if (first) std::memcpy(dst, &buffer_[idx], first * sizeof(T));
        if (toRead > first) std::memcpy(dst + first, &buffer_[0], (toRead - first) * sizeof(T));

        // publish read index
        readIdx_.store(r + toRead, std::memory_order_release);
        return toRead;
    }

    // Push a single item.
    bool push_one(const T& v) { return push(&v, 1); }

    // Read a single item; returns true if read.
    bool read_one(T& out) { return read(&out, 1) == 1; }

    // Drop up to n oldest items. Returns number actually dropped.
    size_t drop(size_t n) {
        uint64_t r = readIdx_.load(std::memory_order_relaxed);
        uint64_t w = writeIdx_.load(std::memory_order_acquire);
        size_t avail = static_cast<size_t>(w - r);
        size_t toDrop = std::min(n, avail);
        if (toDrop == 0) return 0;
        readIdx_.store(r + toDrop, std::memory_order_release);
        return toDrop;
    }

private:
    static size_t nextPowerOfTwo(size_t v) {
        // round up to next power of two
        if (v <= 1) return 2;
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
        v |= v >> 32;
#endif
        return ++v;
    }

    size_t capacity_;
    size_t mask_;
    std::vector<T> buffer_;
    std::atomic<uint64_t> writeIdx_{0};
    std::atomic<uint64_t> readIdx_{0};
};

