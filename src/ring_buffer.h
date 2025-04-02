#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <iostream>
#include <functional>
#include "logger.h"

template <typename T>
class RingBuffer {
public:
    RingBuffer(size_t size) : size_(size), buffer_(size), writeIndex_(0), readIndex_(0) {}

    // Adds an item to the ring buffer
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[writeIndex_] = item;
        writeIndex_ = (writeIndex_ + 1) % size_;
        if (writeIndex_ == readIndex_) {
            readIndex_ = (readIndex_ + 1) % size_;  // Overwrite old data when full
        }
    }

    // Get the number of items available to read
    size_t available() const {
        return (writeIndex_ + size_ - readIndex_) % size_;
    }

    // Gets the entire buffer in a contiguous array
    std::vector<T> get_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> result;
        size_t current = readIndex_;
        while (current != writeIndex_) {
            result.push_back(buffer_[current]);
            current = (current + 1) % size_;
        }
        return result;
    }
    
    std::vector<T> get(size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<T> result;
        size_t current = readIndex_;
        size_t available = (writeIndex_ + size_ - readIndex_) % size_; // Calculate available data
    
        // Ensure we don't try to read more than what is available
        count = std::min(count, available);
    
        for (size_t i = 0; i < count; ++i) {
            result.push_back(buffer_[current]);
            current = (current + 1) % size_;
        }
    
        return result;
    }

private:
    size_t size_;
    std::vector<T> buffer_;
    size_t writeIndex_;
    size_t readIndex_;
    mutable std::mutex mutex_;
};
