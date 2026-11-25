#!/usr/bin/env bash
#
# 用法：
#   ./build.sh         增量构建（只重编改过的文件，日常用这个）
#   ./build.sh clean   删掉 build/ 从零重新构建
#   ./build.sh run     构建完成后运行 ./bin/mutex
#
set -euo pipefail

# 定位脚本所在目录，保证在任意路径下调用都能找对项目根
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build"
TARGET="$ROOT/bin/mutex"

# 本机 Trae 终端的环境坑：LD_LIBRARY_PATH 里有旧版 libstdc++，
# 会让系统 cmake 启动失败（报 GLIBCXX not found）。清掉它，恢复正常。
unset LD_LIBRARY_PATH

# 解析子命令（默认 build）
CMD="${1:-build}"
case "$CMD" in
    clean)
        echo ">>> 清理 $BUILD"
        rm -rf "$BUILD"
        ;;
    build|run) ;;
    *)
        echo "未知参数: $CMD（可用: build | clean | run）" >&2
        exit 1
        ;;
esac

# 1) 配置：首次或改了 CMakeLists.txt 时才真正干活，其余时候瞬间返回
echo ">>> 配置 (cmake -S . -B build)"
cmake -S "$ROOT" -B "$BUILD"

# 2) 构建：增量编译；-j 用满所有 CPU 核，只重编改动过的文件
echo ">>> 构建 (cmake --build build -j)"
cmake --build "$BUILD" -j"$(nproc)"

echo ">>> 产物: $TARGET"

# 3) 可选运行
if [[ "$CMD" == "run" ]]; then
    echo ">>> 运行"
    exec "$TARGET"
fi
