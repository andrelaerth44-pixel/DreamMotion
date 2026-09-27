cmake_minimum_required(VERSION 3.22.1)
project(dreamsengine)

add_library(dreamsengine SHARED
    src/native_bridge.cpp
    src/gl_render_engine.cpp
    src/timeline.cpp
    src/stroke.cpp
)

target_include_directories(dreamsengine PRIVATE include)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles3-lib GLESv3)

target_link_libraries(dreamsengine
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${gles3-lib}
)

target_compile_features(dreamsengine PUBLIC cxx_std_17)
