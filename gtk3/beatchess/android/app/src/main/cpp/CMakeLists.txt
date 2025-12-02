cmake_minimum_required(VERSION 3.22.1)
project(beatchess)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add position independent code flag
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

# Find required packages
find_package(log REQUIRED)

# Find Cairo - using pkg-config or manual setup
include(FindPkgConfig)
pkg_check_modules(CAIRO cairo QUIET)

if(NOT CAIRO_FOUND)
    message(STATUS "Cairo not found via pkg-config, attempting manual configuration")
    # Set paths for pre-built Cairo libraries (you'll need to build or download these)
    # See: https://github.com/preshing/cairo-android
    set(CAIRO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../cairo/include")
    set(CAIRO_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../cairo/lib/${ANDROID_ABI}")
    
    # For simplicity, we'll try to find it in standard paths
    find_path(CAIRO_INCLUDE_DIR cairo.h PATHS /usr/include /usr/local/include)
    find_library(CAIRO_LIBRARY cairo)
else()
    set(CAIRO_LIBRARY ${CAIRO_LIBRARIES})
endif()

message(STATUS "CAIRO_INCLUDE_DIR: ${CAIRO_INCLUDE_DIR}")
message(STATUS "CAIRO_LIBRARY: ${CAIRO_LIBRARY}")

# Chess engine library (static)
add_library(beatchess_core STATIC
    beatchess.c
)

target_include_directories(beatchess_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CAIRO_INCLUDE_DIR}
)

# JNI wrapper library (shared)
add_library(beatchess_jni SHARED
    beatchess_jni.c
)

target_include_directories(beatchess_jni PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CAIRO_INCLUDE_DIR}
)

target_link_libraries(beatchess_jni
    beatchess_core
    log
    android
    ${CAIRO_LIBRARY}
)

# Optional: If linking against prebuilt Cairo, use this approach
# target_link_directories(beatchess_jni PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../cairo/lib/${ANDROID_ABI})
# target_link_libraries(beatchess_jni beatchess_core log android cairo)
