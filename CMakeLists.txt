cmake_minimum_required(VERSION 3.21)
project(EaiLidarViewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# 核心库：协议解析 + 组帧（纯 C++，无 Qt 依赖，便于独立单测）
# ---------------------------------------------------------------------------
add_library(eai_core STATIC
    src/protocol/parser.cpp
    src/protocol/frame_assembler.cpp
)
target_include_directories(eai_core PUBLIC src)

# ---------------------------------------------------------------------------
# 单元测试（纯 C++，用协议.md 的算例做基准）
# ---------------------------------------------------------------------------
enable_testing()
add_executable(parser_tests tests/parser_test.cpp)
target_link_libraries(parser_tests PRIVATE eai_core)
add_test(NAME parser_tests COMMAND parser_tests)

# ---------------------------------------------------------------------------
# Qt 上位机应用
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

qt_add_executable(eailidar_viewer
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/ui/scan_view.cpp
    src/ui/scan_view.h
)
target_link_libraries(eailidar_viewer PRIVATE Qt6::Core Qt6::Widgets eai_core)

# 串口传输（可选）：Fedora 安装后自动启用 `sudo dnf install qt6-qtserialport-devel`
find_package(Qt6 QUIET COMPONENTS SerialPort)
if(Qt6SerialPort_FOUND)
    target_compile_definitions(eailidar_viewer PRIVATE EAI_HAVE_SERIALPORT)
    target_link_libraries(eailidar_viewer PRIVATE Qt6::SerialPort)
    message(STATUS "Qt6 SerialPort 已启用")
else()
    message(WARNING "未找到 Qt6 SerialPort，串口传输暂不可用。安装：sudo dnf install qt6-qtserialport-devel")
endif()
