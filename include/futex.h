#pragma once

#include<atomic>

// 模拟系统调用
// 内核伪代码
void futex_wait(std::atomic<int> *uaddr, int expected) {
    
    // // 2. 把当前线程挂到哈希表对应的等待队列，锁住队列。
    // queue = &futex_hash[uaddr % N];
    // add_to_queue(queue, current_task);
    
    // // 3. 把线程状态设为 TASK_INTERRUPTIBLE（睡眠）
    // current_task->state = TASK_INTERRUPTIBLE;
    
    // // 4. 让出 CPU
    // schedule();  // 调度器选别的线程跑
    while(uaddr -> load() == expected);
    
    return;
}

// 内核伪代码, 叫醒一个线程
void futex_wake(std::atomic<int> *uaddr, int style) {
    // // 1. 找到对应的等待队列
    // queue = &futex_hash[uaddr % N];
    
    // // 2. 从队列里取 count 个等待者
    // for (i = 0; i < count; i++) {
    //     waiter = dequeue(queue);
        
    //     // 3. 把等待者唤醒
    //     waiter->task->state = TASK_RUNNING;
    //     // 放到运行队列，调度器下次会调度到它
    // }
    uaddr -> exchange(style, std::memory_order_release);
}

class Futex{
    std::atomic<int> locked{0};  // 0=空闲, 1=被占无等待者, 2=被占有等待者
    
public:
    void lock() {
    	int expected = 0;
        // 快路径，CAS抢锁
        if (locked.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            return;
        }    
        while (true) {
            // 此时说明该线程没抢到锁，lock的状态只可能为0（抢到的线程瞬间释放）或1（锁被抢到的线程霸占）
            // 此时expected = 1, 如果 lock 为 1，就修改为 2，表明该线程在等待，expected = 1；
            // 如果 lock 为 0，expected = 2;
            locked.compare_exchange_strong(expected, 2, std::memory_order_acquire);
            // 慢路径，认为锁被占用，陷入内核态，让线程睡眠（内核原子检查 locked==2）
            // 此时，lock只可能为0或者2; 等于2直接睡，不等于2系统调用直接返回，下面再抢
            futex_wait(&locked, 2);
            // 重新抢锁
            expected = 0;
            if (locked.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
                return;
            } 
        }
    }
    
    void unlock() {
        // int old = locked.exchange(0, std::memory_order_release);  // 直接释放为0
        // if (old == 2) {
        //     // 之前有等待者 → 唤醒
        //     // futex_wake(&locked);
        // }
        futex_wake(&locked, 0);
    }
};

// std::mutex 的底层实现思路和你写的 futex mutex 高度相似：快路径用原子 CAS 抢锁，慢路径用 futex 睡眠等待。
// 但 std::mutex 是高度优化的生产级实现，细节上比你的版本更严谨、更高效。