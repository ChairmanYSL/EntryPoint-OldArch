# ============================================================================
# ZT-ENTRYPOINT-CORE — 解耦构建配置
#
# 目标：
#   entrypoint  静态库 —— Entry Point 核心（应用选择/组合选择/内核选择/Outcome/
#               共享服务：TLV/Tag/缓冲/ICC/参数持久化/注入式 porting）
#               **不得引用任何非接内核符号**
#   expresspay  静态库 —— AMEX Expresspay 子内核（链接 entrypoint）
#
# 验证门禁：
#   host (gcc/clang) ：cmake -B build_host -DCMAKE_BUILD_TYPE=Release
#   arm  (NDK r23b)  ：见 scripts/build_ndk.ps1
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(entrypoint_core C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# 公共编译选项（体积 + 静态错误门禁）
# ---------------------------------------------------------------------------
add_compile_options(
    -Os
    -ffunction-sections
    -fdata-sections
    -fno-common
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
    # 静态错误门禁：隐式函数声明/指针不兼容/整数↔指针 一律视为错误
    -Werror=implicit-function-declaration
    -Werror=incompatible-pointer-types
    -Werror=int-conversion
    -Werror=return-type
)

# ---------------------------------------------------------------------------
# entrypoint —— Entry Point 核心库
# ---------------------------------------------------------------------------
file(GLOB ENTRYPOINT_SRC CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.c)
list(SORT ENTRYPOINT_SRC)

add_library(entrypoint STATIC ${ENTRYPOINT_SRC})
target_include_directories(entrypoint PUBLIC
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/inc
)
target_link_options(entrypoint INTERFACE -Wl,--gc-sections)

# ---------------------------------------------------------------------------
# expresspay —— AMEX Expresspay 子内核（链接 entrypoint）
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/kernels/expresspay/src)
    file(GLOB EXPRESSPAY_SRC CONFIGURE_DEPENDS
        ${CMAKE_SOURCE_DIR}/kernels/expresspay/src/*.c)
    list(SORT EXPRESSPAY_SRC)
    if(EXPRESSPAY_SRC)
        add_library(expresspay STATIC ${EXPRESSPAY_SRC})
        target_include_directories(expresspay PRIVATE
            ${CMAKE_SOURCE_DIR}/kernels/expresspay/inc
            ${CMAKE_SOURCE_DIR}/kernels/expresspay/include
        )
        target_link_libraries(expresspay PUBLIC entrypoint)
        target_link_options(expresspay INTERFACE -Wl,--gc-sections)
    endif()
endif()
