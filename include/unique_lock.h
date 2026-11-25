#pragma once

#include "futex.h"

// RAII，析构函数自动回收资源
class UniqueLock {
    Futex& mtx;
    bool owns;

public:
    explicit UniqueLock(Futex& m) : mtx(m), owns(false) {
        mtx.lock();
        owns = true;
    }

    ~UniqueLock() {
        if (owns) {
            mtx.unlock();
        }
    }

    void unlock() {
        mtx.unlock();
        owns = false;
    }

    void lock() {
        mtx.lock();
        owns = true;
    }

    // 禁止拷贝
    UniqueLock(const UniqueLock&) = delete;
    UniqueLock& operator=(const UniqueLock&) = delete;
};