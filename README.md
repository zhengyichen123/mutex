# mutex —— 从零理解互斥锁与条件变量

一个小玩具项目：用 C++ 手写一个"迷你 futex"，在它之上依次实现互斥锁、RAII 锁和条件变量，
再用一个生产者–消费者 demo 串起来，帮助理解 `std::mutex` / `std::condition_variable` 的底层原理。

glibc 的 `pthread_mutex` 底层就是 futex。这个项目用几百行把核心思想复刻一遍：
**用户态 CAS 快路径 + 慢路径睡眠/等待**。

---

## 目录结构

```
mutex/
├── CMakeLists.txt          # 构建配置
├── build.sh                # 一键构建脚本
├── include/
│   ├── futex.h             # 核心：futex 原语 + Futex 互斥锁
│   ├── unique_lock.h       # RAII 锁封装
│   └── condition_variable.h# 条件变量
├── src/
│   └── main.cpp            # 生产者–消费者 demo
└── bin/mutex               # 构建产物（可执行文件）
```

---

## 四层结构（自底向上）

```
SimpleCondVar  ← 条件变量（wait / notify）
     ↑ 用到
UniqueLock     ← RAII 封装（构造加锁 / 析构解锁）
     ↑ 用到
Futex          ← 互斥锁（lock / unlock，state = 0/1/2）
     ↑ 用到
futex_wait / futex_wake  ← 最底层的等待/唤醒原语
```

---

## 1. futex 原语（`futex.h`）

futex = **Fast User-space muTEX**。它是内核提供的一对原语：

- `futex_wait(uaddr, expected)`：**仅当** `*uaddr == expected` 时线程才睡，否则立即返回。
  这个"比较 + 睡眠"在内核里是原子完成的，专门用来避免**丢失唤醒（lost wakeup）**。
- `futex_wake(uaddr, n)`：唤醒等待在 `uaddr` 上的 n 个线程。

> **本项目的简化**：真正的入队、`TASK_INTERRUPTIBLE`、`schedule()`（让出 CPU）都被注释掉了，
> 改用一个**忙等自旋** `while (*uaddr == expected);` 顶替。
> 语义上等价（都是"等到值变了才往下走"），但**不省 CPU**——真实现会让线程真正睡下。

---

## 2. Futex 互斥锁（`futex.h`）：0 / 1 / 2 三态

```
locked = 0  空闲
locked = 1  已占用，但没有等待者
locked = 2  已占用，且有等待者
```

- **lock() —— 快路径**：一次 CAS 把 0 改成 1，成功直接拿到锁（无系统调用，最快）。
- **lock() —— 慢路径**：CAS 失败说明锁被占，把状态标记成 2（告诉持锁者"有人在等"），
  然后等锁释放，再重试抢锁。
- **unlock()**：把状态放回 0。

> 真实实现里 unlock 会检查 `old == 2` 来决定是否 `wake`（如果有人在等才唤醒，省系统调用）。
> 本项目为简洁起见，直接把锁置 0。

---

## 3. UniqueLock（`unique_lock.h`）：RAII

构造时加锁，析构时自动解锁（如果还持有着），并**禁止拷贝**。

好处：即使中途 `return` 或抛异常，析构函数也一定会解锁，不会把锁泄漏掉。
对应标准库的 `std::unique_lock`。

---

## 4. SimpleCondVar（`condition_variable.h`）：序列号 + futex

条件变量解决的是"**等某个条件成立**"的问题，而不是"抢占资源"。核心是一个版本号 `seq`：

```
wait(lock):
    lock.unlock()          // ① 先放锁，否则生产者永远拿不到锁去改共享状态
    futex_wait(&seq, 0)    // ② 睡在 seq 上：等到 seq != 0 才返回
    lock.lock()            // ③ 醒来后重新拿锁
    seq = 0                // ④ 复位，为下一次等待准备

notify_one():
    futex_wake(&seq, 1)    // 把 seq 改成 1，唤醒等待者
```

为什么 `wait` 必须先放锁？因为消费者睡下时如果还抱着锁，生产者就永远进不来、也就永远无法通知。
这与 `std::condition_variable::wait` 的语义一致。

---

## demo：生产者–消费者（`main.cpp`）

```
共享状态：int data = 0;  bool data_ready = false;
```

- 5 个消费者：抢锁 → `while (!data_ready)` 就 `cv.wait(lock)` → 拿到数据后打印
- 5 个生产者：抢锁 → `data++`、`data_ready = true` → `cv.notify_one()`
- `main` 里创建 5+5 个线程，最后 `join`

用 `while` 而不是 `if` 包住 `wait`，是为了应对**虚假唤醒**（线程可能无故被叫醒，必须重新检查条件）。

---

## 构建与运行

```bash
./build.sh          # 增量构建（日常用）
./build.sh clean    # 删掉 build/ 从零构建
./build.sh run      # 构建并运行 ./bin/mutex
```

等价的手动命令：

```bash
cmake -S . -B build      # 配置（改了 CMakeLists.txt 才需要）
cmake --build build -j   # 构建（增量）
./bin/mutex              # 运行
```

---

## 已知简化点（教学用途，便于对照真实实现）

| 简化 | 真实实现 |
|---|---|
| `futex_wait` 是忙等自旋，一直占 CPU | 进内核睡眠，`schedule()` 让出 CPU |
| `notify_one` 用 `exchange(1)` 改值，**实际是广播**，唤醒所有等待者 | 真正只唤醒一个 |
| `seq` 用 0/1 二值，并在 `wait` 末尾手动复位 | 用单调递增的版本号，天然区分"每一次通知" |
| `unlock` 直接置 0，不区分有无等待者 | 检查 `old == 2` 决定是否 wake |
| demo 用全局变量，未做完整边界处理 | 封装成类、加测试 |

> 观察 demo 运行时可能出现消费者空转、输出重复等现象——这正是上述简化带来的"教学破绽"，
> 顺着它去对比真实 futex 语义，是理解这套机制最好的切入点。

---

## 可以动手试试的改进

1. 把 `futex_wait` / `futex_wake` 换成真正的系统调用 `syscall(SYS_futex, ...)`，观察线程真的睡下去后 CPU 占用下降。
2. 把 `seq` 改成 `fetch_add(1)` 单调递增，去掉 `seq = 0` 的复位，解决并发下的丢失唤醒。
3. 区分 `notify_one` 与 `notify_all`（唤醒数量不同）。
4. 给 demo 加多轮"生产–消费"和断言，验证没有丢数据、没有死锁。
