#pragma once

#include<optional>
#include<memory>
#include<iostream>

#include "condition_variable.h"
#include "futex.h"
#include "unique_lock.h"

template <typename T>
class Future;

template <typename T>
class Promise {
public:
    Promise() : state_(std::make_shared<State>()) {}
    template<typename U>
    friend class Future;

    Future<T> get_future();   // 给消费者一个 future

    void set_value(T value);  // 生产者放结果

private:
    struct State {
        Futex mutex;
        SimpleCondVar cv;
        bool ready = false;
        std::optional<T> value;
    };
    std::shared_ptr<State> state_;
};

template<typename T>
Future<T> Promise<T>::get_future() {
    return Future<T>(state_);
}

template<typename T>
void Promise<T>::set_value(T value) {
    {
        UniqueLock lock(state_->mutex);
        if (state_->ready) {
            // throw std::runtime_error("set_value called twice");
            std::cout << "set_value called twice" << "\n";
        }
        state_->value = std::move(value);
        state_->ready = true;
    }
    state_->cv.notify_one();
}

template<typename T>
class Future {
public:
    explicit Future(std::shared_ptr<typename Promise<T>::State> s)
        : state_(std::move(s)) {}

    T get() {
        UniqueLock lock(state_->mutex);
        while (!state_->ready) {
            state_->cv.wait(lock);
        }
        T result = std::move(*state_->value);
        state_->value.reset();   // 拿走后清空，防止二次 get
        return result;
    }

private:
    std::shared_ptr<typename Promise<T>::State> state_;
};