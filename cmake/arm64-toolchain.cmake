SET(CMAKE_SYSTEM_NAME Linux CACHE STRING "toolchain default")
SET(CMAKE_SYSTEM_PROCESSOR aarch64 CACHE STRING "toolchain default")
#cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm64-toolchain.cmake ..
# ============================================================
# Clang 交叉编译核心配置
# ============================================================
SET(CLANG_TARGET "aarch64-linux-gnu")
SET(CMAKE_SYSROOT /home/yanfa/ubuntu20-arm64-sysroot CACHE STRING "toolchain default")

SET(GCC_ARCH_INCLUDE "/usr/aarch64-linux-gnu/include/c++/10/aarch64-linux-gnu")

# 方式1（推荐）：通过 CXX_FLAGS 注入，对所有目标生效
string(APPEND CMAKE_CXX_FLAGS_INIT " -isystem ${GCC_ARCH_INCLUDE}")
string(APPEND CMAKE_C_FLAGS_INIT   " -isystem ${GCC_ARCH_INCLUDE}")
add_compile_options(-isystem ${GCC_ARCH_INCLUDE})

# 编译器设置（必须带 --target）
SET(CMAKE_C_COMPILER /usr/bin/clang CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER /usr/bin/clang++ CACHE STRING "toolchain default")
SET(CMAKE_ASM_COMPILER /usr/bin/clang CACHE STRING "toolchain default")

# 【关键】Clang 交叉编译必须的标志
SET(CMAKE_C_COMPILER_TARGET ${CLANG_TARGET} CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER_TARGET ${CLANG_TARGET} CACHE STRING "toolchain default")
SET(CMAKE_ASM_COMPILER_TARGET ${CLANG_TARGET} CACHE STRING "toolchain default")

# ============================================================
# Binutils 工具链（保持 GNU 工具不变）
# ============================================================
SET(CMAKE_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_C_COMPILER_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER_AR /usr/bin/aarch64-linux-gnu-ar CACHE STRING "toolchain default")
SET(CMAKE_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_C_COMPILER_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_CXX_COMPILER_RANLIB /usr/bin/aarch64-linux-gnu-ranlib CACHE STRING "toolchain default")
SET(CMAKE_LINKER /usr/bin/aarch64-linux-gnu-ld CACHE STRING "toolchain default")
SET(CMAKE_NM /usr/bin/aarch64-linux-gnu-nm CACHE STRING "toolchain default")
SET(CMAKE_READELF /usr/bin/aarch64-linux-gnu-readelf CACHE STRING "toolchain default")
SET(CMAKE_OBJCOPY /usr/bin/aarch64-linux-gnu-objcopy CACHE STRING "toolchain default")
SET(CMAKE_OBJDUMP /usr/bin/aarch64-linux-gnu-objdump CACHE STRING "toolchain default")

# ============================================================
# 查找路径策略
# ============================================================
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT} CACHE STRING "toolchain default")
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "toolchain default")
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY CACHE STRING "toolchain default")
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY CACHE STRING "toolchain default")
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY CACHE STRING "toolchain default")
