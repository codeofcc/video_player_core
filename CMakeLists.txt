cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME "video_player_core")
project(${PROJECT_NAME} LANGUAGES CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Git REQUIRED)

# 通用 git clone 函数
# 用法: git_clone_if_needed(<url> <tag> <dest_dir>)
function(git_clone_if_needed _url _tag _dest)
    if(EXISTS "${_dest}/.git")
        message(STATUS "[Git] ${_dest} already exists, skipping clone.")
        return()
    endif()

    message(STATUS "[Git] Cloning ${_url} (tag: ${_tag}) -> ${_dest}")
    file(MAKE_DIRECTORY "${_dest}")

    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --depth 1 -b "${_tag}" "${_url}" "${_dest}"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _output
        ERROR_VARIABLE _error
    )

    if(NOT _result EQUAL 0)
        # clone 失败时清理残留空目录，避免下次误判为"已存在"
        file(REMOVE_RECURSE "${_dest}")
        message(FATAL_ERROR "[Git] Clone failed (${_result}): ${_error}")
    endif()

    message(STATUS "[Git] Clone succeeded: ${_dest}")
endfunction()


include(FetchContent)



set(FFI_PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps")

# ── 下载 SDL2 ──
git_clone_if_needed(
    "https://github.com/libsdl-org/SDL.git"
    "release-2.30.12"
    "${FFI_PACKAGE_DIR}/SDL2"
)

# ── 下载 libgo ──
git_clone_if_needed(
    "https://github.com/codeofcc/libgo.git"
    "master"
    "${FFI_PACKAGE_DIR}/libgo"
)




# ── SDL2 极简音频配置 ──
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
# 音频核心（必须）
set(SDL_AUDIO ON CACHE BOOL "" FORCE)

# 音频依赖（必须）
set(SDL_THREADS ON CACHE BOOL "" FORCE) # ⚠️ SDL 强制要求，不能关闭

set(SDL_IME OFF CACHE BOOL "" FORCE) # 禁用输入法框架 (IBus/Fcitx)
set(SDL_IBUS OFF CACHE BOOL "" FORCE) # 显式禁用 IBus
set(SDL_FCITX OFF CACHE BOOL "" FORCE) # 显式禁用 Fcitx
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)

# 关闭其他不需要的模块
foreach(_sdl_mod
    EVENTS
    FILE
    MISC
    TIMERS
    ATOMIC
    VIDEO RENDER JOYSTICK HAPTIC SENSOR TOUCH POWER
    DIRECTX OPENGLES VULKAN LOADSO CPUINFO
    VIRTUAL_JOYSTICK RENDER_D3D LIBUDEV HIDAPI_JOYSTICK
    FILESYSTEM LOCALE
    # 各种音频后端（仅保留 WASAPI）
    ALSA PULSEAUDIO JACK PIPEWIRE ESD ARTS NAS SNDIO FUSIONSOUND
    # 各种平台
    X11 WAYLAND DIRECTFB KMSDRM COCOA
    DISKAUDIO
    DUMMYAUDIO
    DUMMYVIDEO
    HIDAPI
    OPENGL
    OFFSCREEN
    XINPUT
    ALTIVEC
    CCACHE
)
    set(SDL_${_sdl_mod} OFF CACHE BOOL "" FORCE)
endforeach()

FetchContent_Declare(
    SDL2
    SOURCE_DIR "${FFI_PACKAGE_DIR}/SDL2"
)
FetchContent_MakeAvailable(SDL2)


# 在 MakeAvailable 之前设置 libgo 的选项（覆盖默认值）
set(LIBGO_ENABLE_DEBUGGER OFF CACHE BOOL "" FORCE)
set(LIBGO_DISABLE_HOOK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libgo
    SOURCE_DIR "${FFI_PACKAGE_DIR}/libgo"
)


FetchContent_MakeAvailable(libgo)


# include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../ffi/include")
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(APPEND LIBRARIES
    SDL2::SDL2
    libgo::libgo
)

# ── 平台相关配置 ──
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")

    set(VCPKG_DIR "${CMAKE_BINARY_DIR}/vcpkg")
    set(FFMPEG_VERSION_TAG "2026.06.24") # 👈 指定目标版本 8.1.2 //查看方式：在vcpkg的git查看ffmpeg包的版本对应的tag
    find_package(Git REQUIRED)

    if(NOT EXISTS "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
        execute_process(COMMAND ${GIT_EXECUTABLE} clone -b ${FFMPEG_VERSION_TAG} --depth=1 https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}" COMMAND_ERROR_IS_FATAL ANY)
        execute_process(COMMAND "${VCPKG_DIR}/bootstrap-vcpkg.bat" -disableMetrics WORKING_DIRECTORY "${VCPKG_DIR}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
    # 3. 注入 toolchain 并安装（保持 HEAD 最新状态）
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")

    execute_process(
        COMMAND "${VCPKG_DIR}/vcpkg.exe" install "ffmpeg[avcodec,avformat,swresample,swscale]:x64-windows-static-md"
        WORKING_DIRECTORY "${VCPKG_DIR}"
        COMMAND_ECHO STDOUT
        COMMAND_ERROR_IS_FATAL ANY
    )

    list(APPEND CMAKE_MODULE_PATH "${VCPKG_DIR}/installed/x64-windows-static-md/share/ffmpeg")

    find_package(FFMPEG MODULE REQUIRED)
    list(APPEND INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NPROC EQUAL 0)
        set(NPROC 1)
    endif()

    # 路径变量
    set(FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/ffmpeg-build/install")
    set(FFMPEG_SOURCE_DIR "${FFI_PACKAGE_DIR}/ffmpeg")

    # ── 下载 FFmpeg ──
    git_clone_if_needed(
        "https://git.ffmpeg.org/ffmpeg.git"
        "n8.1.2"
        "${FFMPEG_SOURCE_DIR}"
    )



    if(NOT EXISTS "${FFMPEG_INSTALL_DIR}/lib/libavcodec.a")
        # 下载并构建
        include(ExternalProject)
        ExternalProject_Add(ffmpeg
            BUILD_ALWAYS OFF
            # UPDATE_COMMAND ""
            # UPDATE_DISCONNECTED ON
            SOURCE_DIR "${FFMPEG_SOURCE_DIR}"
            BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/ffmpeg-build"
            PREFIX "${FFMPEG_INSTALL_DIR}"
            #   GIT_REPOSITORY "https://git.ffmpeg.org/ffmpeg.git"
            #   GIT_TAG "n8.1.2"
            #   GIT_SHALLOW TRUE
            CONFIGURE_COMMAND
            #${CMAKE_COMMAND} -E echo "FFmpeg configure skipped (using pre-built libraries)"
            ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER}
            C_INCLUDE_PATH=$ENV{CPLUS_INCLUDE_PATH}
            # ASFLAGS=-fPIC
            ${FFMPEG_SOURCE_DIR}/configure
            --prefix=${FFMPEG_INSTALL_DIR}
            --cc=${CMAKE_C_COMPILER}
            --arch=x86_64
            --cpu=x86-64
            --target-os=linux
            # === 核心策略 ===
            --disable-everything
            --disable-autodetect # ← 防止隐式依赖
            --disable-shared --enable-static
            --disable-programs # 不构建 ffmpeg/ffprobe/ffplay
            --disable-doc
            # === 移除无用子系统 ===
            --disable-encoders --disable-muxers --disable-bsfs
            --disable-filters
            --disable-indevs --disable-outdevs
            # === 移除外部库依赖 ===
            --disable-iconv --disable-zlib --disable-bzlib --disable-lzma
            --disable-sdl2 --disable-xlib #--disable-xcb
            --disable-alsa --disable-sndio #--disable-pulseaudio --disable-jack
            # === 移除平台特定硬件加速 ===
            --disable-vaapi --disable-vdpau
            --disable-cuda --disable-cuvid --disable-nvenc --disable-nvdec
            --disable-amf --disable-d3d11va --disable-dxva2
            --disable-videotoolbox --disable-audiotoolbox
            --disable-mediacodec
            --disable-v4l2-m2m #--disable-v4l2
            #禁用
            --disable-asm
            # === 保留的功能 ===
            --enable-network --enable-protocols
            --enable-demuxers --enable-parsers
            --enable-decoders --enable-hwaccels
            --enable-swscale --enable-swresample
            #静态库
            --disable-shared --enable-static
            # --pkg-config=pkg-config --pkg-config-flags=--static
            --enable-pic
            # === 编译选项 ===
            "--extra-cflags=-fPIC $<$<CONFIG:Debug>:-g -O0 -DDEBUG>$<$<CONFIG:RelWithDebInfo>:-g -O2 -DNDEBUG>$<$<CONFIG:MinSizeRel>:-Os -DNDEBUG>$<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>>:-O2 -DNDEBUG>"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:--enable-debug>$<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>>:--disable-debug>


            BUILD_COMMAND make -j${NPROC}

            INSTALL_COMMAND make install

            BUILD_BYPRODUCTS
            "${FFMPEG_INSTALL_DIR}/lib/libavcodec.a"
            "${FFMPEG_INSTALL_DIR}/lib/libavformat.a"
            "${FFMPEG_INSTALL_DIR}/lib/libavutil.a"
            "${FFMPEG_INSTALL_DIR}/lib/libswscale.a"
            "${FFMPEG_INSTALL_DIR}/lib/libswresample.a"
        )
    endif()
    list(APPEND INCLUDE_DIRS
        ${FFMPEG_INSTALL_DIR}/include
    )

    list(APPEND LIBRARIES
        ${FFMPEG_INSTALL_DIR}/lib/libavformat.a
        ${FFMPEG_INSTALL_DIR}/lib/libavcodec.a
        ${FFMPEG_INSTALL_DIR}/lib/libswscale.a
        ${FFMPEG_INSTALL_DIR}/lib/libswresample.a
        ${FFMPEG_INSTALL_DIR}/lib/libavutil.a
    )

    # Linux 配置占位
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "macOS build")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

if(NOT EXISTS "${FFMPEG_INSTALL_DIR}/lib/libavcodec.a")
    add_dependencies(${PROJECT_NAME} ffmpeg)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIRS}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden)

target_include_directories(${PROJECT_NAME} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})


