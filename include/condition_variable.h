#pragma once

#include "futex.h"
#include "unique_lock.h"

class SimpleCondVar {
    std::atomic<int> seq{0};  // 序列号，每次 notify 递增

public:
    // ========== wait ==========
    void wait(UniqueLock& lock) {

        // ② 放 mutex 锁（原子地放锁 + 准备睡）
        //    注意：此时生产者可以拿到锁、修改数据、notify
        lock.unlock();

        // ③ 睡在 seq 上
        //    内核原子检查：seq 是否还等于 old_seq？
        //    如果相等 → 入队 → 睡
        //    如果不等（被 notify 了）→ 不入队 → 直接返回
        futex_wait(&seq, 0);

        // ④ 被唤醒后，重拿 mutex 锁
        lock.lock();
        seq = 0;
    }

    // ========== notify_one ==========
    void notify_one() {
        // 把 seq 状态修改为 1；
        futex_wake(&seq, 1);
    }
};