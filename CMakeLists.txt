cmake_minimum_required(VERSION 3.17)
project(qt_flutter_sample)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
# qt
if (DEFINED ENV{QT_PATH})
    set(QT_PATH $ENV{QT_PATH})
else()
#    set(QT_PATH /usr/local/Cellar/qt@5/5.15.10_1)
    message(FATAL_ERROR "Error: QT_PATH not set in local environment")
endif ()
message("QT_PATH: ${QT_PATH}")
set(CMAKE_PREFIX_PATH "${QT_PATH}/lib/cmake") # 此处设置本地qt cmake地址
set(QT_PLUGIN_PATH "${QT_PATH}/plugins") # 设置本地qt plugins地址
message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
find_package(Qt5 COMPONENTS Widgets Core OpenGL REQUIRED)
set(CMAKE_AUTOMOC ON)

# flutter
if (DEFINED ENV{FLUTTER_ENGINE_LIB_PATH})
    set(FLUTTER_ENGINE_LIB_PATH $ENV{FLUTTER_ENGINE_LIB_PATH})
else()
    set(FLUTTER_ENGINE_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/Debug/)
endif ()
#set(FLUTTER_ENGINE_LIB_PATH /Users/xzp/Documents/myCode/flutter/engine/src/out/mac_debug_unopt) # 设置
message("FLUTTER_ENGINE_LIB_PATH: ${FLUTTER_ENGINE_LIB_PATH}")
add_definitions(-DFLUTTER_ICUDTL_PATH="${FLUTTER_ENGINE_LIB_PATH}/FlutterEmbedder.framework/Resources/icudtl.dat")
add_library(flutter_engine SHARED IMPORTED)
set_target_properties(flutter_engine PROPERTIES
        FRAMEWORK TRUE
        IMPORTED_LOCATION ${FLUTTER_ENGINE_LIB_PATH}/FlutterEmbedder.framework/FlutterEmbedder
        INTERFACE_INCLUDE_DIRECTORIES ${FLUTTER_ENGINE_LIB_PATH}/FlutterEmbedder.framework/Headers
        )
#add_library(FlutterMacOS SHARED IMPORTED)
find_library(COCOA_FRAMEWORK Cocoa)
# 添加可执行文件
add_executable(qt_flutter_sample main.cpp
        flutter_embedder_utils.cpp
        flutter_embedder_utils.h
        flutter_view.cpp
        flutter_view.h
        )

# 链接Qt库
target_link_libraries(qt_flutter_sample PRIVATE
        Qt5::Widgets
        Qt5::Core
        Qt5::OpenGL
        flutter_engine
#        OpenGL::GL
        )

