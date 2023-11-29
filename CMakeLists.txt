cmake_minimum_required(VERSION 3.17)
project(qt_flutter_sample)

set(CMAKE_CXX_STANDARD 14)


# 引入QT
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



# 引入FlutterEmbedder
if (DEFINED ENV{FLUTTER_ENGINE_LIB_PATH})
    set(FLUTTER_ENGINE_LIB_PATH $ENV{FLUTTER_ENGINE_LIB_PATH})
else()
    message(FATAL_ERROR "Error: FLUTTER_ENGINE_LIB_PATH not set in local environment")
endif ()
#set(FLUTTER_ENGINE_LIB_PATH /Users/xzp/Documents/myCode/flutter/engine/src/out/mac_debug_unopt) # 设置
message("FLUTTER_ENGINE_LIB_PATH: ${FLUTTER_ENGINE_LIB_PATH}")
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Darwin")
    add_definitions(-DFLUTTER_ICUDTL_PATH="${FLUTTER_ENGINE_LIB_PATH}/Resources/icudtl.dat")
    add_library(flutter_engine SHARED IMPORTED)
    set_target_properties(flutter_engine PROPERTIES
            FRAMEWORK TRUE
            IMPORTED_LOCATION ${FLUTTER_ENGINE_LIB_PATH}/FlutterEmbedder
            INTERFACE_INCLUDE_DIRECTORIES ${FLUTTER_ENGINE_LIB_PATH}/Headers
            )
    #add_library(FlutterMacOS SHARED IMPORTED)
    find_library(COCOA_FRAMEWORK Cocoa)
elseif (CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    set_target_properties(flutter_engine PROPERTIES
            IMPORTED_LOCATION ${FLUTTER_ENGINE_LIB_PATH}/flutter_engine.dll
            INTERFACE_INCLUDE_DIRECTORIES ${FLUTTER_ENGINE_LIB_PATH})
else()
    message(FATAL_ERROR "Error: This project not support in ${CMAKE_HOST_SYSTEM_NAME}")
endif()



# 添加可执行文件
add_executable(flutter_embedder_qt main.cpp
        flutter_embedder_utils.cpp
        flutter_embedder_utils.h
        flutter_view.cpp
        flutter_view.h
        )


# 链接Qt库
target_link_libraries(flutter_embedder_qt PRIVATE
        Qt5::Widgets
        Qt5::Core
        Qt5::OpenGL
        flutter_engine
        )

