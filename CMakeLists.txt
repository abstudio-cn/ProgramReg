cmake_minimum_required(VERSION 3.21)
project(ProgramReg LANGUAGES CXX)

# 静态 Qt 仅构建了 Release 版（无 Debug 库），多配置生成器（VS）只暴露 Release 系配置，
# 避免 Debug 配置下 /MTd 与 Qt Release 库(MT)冲突（LNK2038/LIBCMTD/_CrtDbgReport）。
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "" FORCE)
endif()

# 静态 Qt 默认路径回退：Visual Studio 直接打开项目/文件夹时无命令行 -D 参数，
# 此处自动补充前缀，保证 find_package(Qt6) 开箱即用。
if(NOT DEFINED Qt6_DIR AND NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3-static-msvc2022_64")
    message(STATUS "使用默认静态 Qt 前缀：${CMAKE_PREFIX_PATH}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# MSVC：源码含中文，必须 /utf-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ============ 共享静态库 ============
add_library(pr_shared STATIC
    Shared/PeInfo.cpp
    Shared/RegOps.cpp
    Shared/Shortcut.cpp
    Shared/ShellPin.cpp
    Shared/IniFile.cpp
    Shared/InstallCore.cpp
    Shared/UninstallRunner.cpp
    Shared/Theme.cpp
    Shared/IconExtract.cpp
)
target_include_directories(pr_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Shared
)
target_link_libraries(pr_shared PUBLIC Qt6::Widgets)
set_target_properties(pr_shared PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ============ 资源文件 ============
# rc 模板通过 configure_file 注入图标绝对路径（RC 的 ICON 语句不搜索 /I 路径）
set(PROGRAMREG_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ProgramReg/icon.ico")
set(UNINSTALLHELPER_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/UninstallHelper/icon.ico")
# RC 的 ICON 路径需反斜杠
string(REPLACE "/" "\\\\" PROGRAMREG_ICO_PATH "${PROGRAMREG_ICO_PATH}")
string(REPLACE "/" "\\\\" UNINSTALLHELPER_ICO_PATH "${UNINSTALLHELPER_ICO_PATH}")
configure_file(ProgramReg/ProgramReg.rc.in ${CMAKE_CURRENT_BINARY_DIR}/ProgramReg/ProgramReg.rc @ONLY)
configure_file(UninstallHelper/UninstallHelper.rc.in ${CMAKE_CURRENT_BINARY_DIR}/UninstallHelper/UninstallHelper.rc @ONLY)

set(ICONS_QRC ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons.qrc)

# ============ 安装注册程序（需要管理员权限） ============
add_executable(ProgramReg WIN32
    ProgramReg/main.cpp
    ProgramReg/MainWindow.cpp
    ${ICONS_QRC}
    ${CMAKE_CURRENT_BINARY_DIR}/ProgramReg/ProgramReg.rc
)
target_link_libraries(ProgramReg PRIVATE pr_shared)
target_link_options(ProgramReg PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/ProgramReg/ProgramReg.rc PROPERTIES
    COMPILE_FLAGS "/I\"${CMAKE_CURRENT_SOURCE_DIR}/ProgramReg\"")

# ============ 默认卸载程序 ============
# manifest 用 asInvoker（默认）+ 运行时自我提权（runas），
# 避免控制面板启动 requireAdministrator 进程被拒时出现
# 「没有足够的权限卸载…」的系统级错误（对齐 Inno Setup 的做法）。
add_executable(UninstallHelper WIN32
    UninstallHelper/main.cpp
    UninstallHelper/UninstallWindow.cpp
    ${ICONS_QRC}
    ${CMAKE_CURRENT_BINARY_DIR}/UninstallHelper/UninstallHelper.rc
)
target_link_libraries(UninstallHelper PRIVATE pr_shared shell32)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/UninstallHelper/UninstallHelper.rc PROPERTIES
    COMPILE_FLAGS "/I\"${CMAKE_CURRENT_SOURCE_DIR}/UninstallHelper\"")

# 输出到 build/bin
set_target_properties(ProgramReg UninstallHelper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
