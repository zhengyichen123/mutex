# ===== 第 1 步：声明 CMake 的最低版本 =====
# 你的 CMake 版本必须 >= 这里写的数字，否则报错
cmake_minimum_required(VERSION 3.16)

# ===== 第 2 步：声明项目名和用到的语言 =====
# LANGUAGES CXX 表示这是个 C++ 项目（只找 C++ 编译器，不找 C 的）
project(mutex LANGUAGES CXX)

# ===== 产物输出目录 =====
# 默认可执行文件会埋在 build/ 里，和一堆 CMakeFiles/ 混在一起不好找。
# 这行让所有可执行程序统一输出到 <源码目录>/bin，从此固定是 ./bin/mutex。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ===== 第 3 步：设定 C++ 标准 =====
# 你的代码用了 std::atomic、类内成员初始化等，C++17 足够，也最通用
set(CMAKE_CXX_STANDARD 17)
# 如果编译器不支持 C++17，直接报错，而不是偷偷降级
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 关闭编译器扩展（-std=c++17 而不是 -std=gnu++17），保证可移植
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json（编译数据库），让 IDE / clangd 知道真实编译参数
# 不加这行，IDE 就不知道 include/ 是头文件搜索路径，会误报 "file not found"
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===== 第 4 步：打开编译警告（学习期强烈建议）=====
add_compile_options(-Wall -Wextra)

# ===== 第 5 步：找线程库 =====
# 你这个项目迟早要用 std::thread / -pthread，先把它找出来
find_package(Threads REQUIRED)

# ===== 第 6 步：定义一个可执行文件 =====
# 第一个参数是"目标名"(mutex)，也是最终生成的可执行文件名
# 后面列出所有需要编译的 .cpp（头文件不用写）
add_executable(mutex
    src/main.cpp
)

# ===== 第 7 步：告诉编译器去哪找头文件 =====
# 你 main.cpp 里写的是 #include "futex.h"，编译器默认只找当前目录
# 所以必须把 include/ 加进搜索路径，否则找不到头文件
# PRIVATE 表示：这个路径只给 mutex 这个目标用，不给别人继承
target_include_directories(mutex PRIVATE include)

# ===== 第 8 步：链接线程库 =====
target_link_libraries(mutex PRIVATE Threads::Threads)
