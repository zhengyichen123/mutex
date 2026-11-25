#include <iostream>
#include <thread>
#include <vector>

#include"condition_variable.h"
#include"futex.h"
#include"unique_lock.h"

Futex mtx;
SimpleCondVar cv;
bool data_ready = false;
int data = 0;

// 消费者线程，取一个data
void consumer(int id) {
    UniqueLock lock(mtx);
    while (!data_ready) {       // ← 必须用 while（防虚假唤醒）
        std::cout << "consumer " << id << " waiting...\n";
        cv.wait(lock);          // 放锁 → 睡 → 醒 → 重拿锁
        std::cout << "consumer " << id << " woke up\n";
    }
    data_ready = false;
    std::cout << "consumer " << id << " got data = " << data << "\n";
}

// 生产者线程，生成一个data
void producer(int id) {
    UniqueLock lock(mtx);
    data++;
    data_ready = true;
    std::cout << "producer " << id << ": data ready\n";
    cv.notify_one();  // 唤醒一个
}

int main() {
    int count = 5;
    std::vector<std::thread> consumers;
    std::vector<std::thread> producers;
    consumers.reserve(count);
    producers.reserve(count);

    for (int i = 0; i < count; i++) {
        consumers.emplace_back(consumer, i);  
        producers.emplace_back(producer, i);  
    }

    for(int i = 0; i < count; i++){
        consumers[i].join();
        producers[i].join();
    }
}