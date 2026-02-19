
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("multi")

find_package(oboe REQUIRED CONFIG)

add_library(libsord STATIC IMPORTED)
set_target_properties(libsord PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libsord-0_static.a)
add_library(libzix STATIC IMPORTED)
set_target_properties(libzix PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libzix.a)
add_library(libjack STATIC IMPORTED)
set_target_properties(libjack PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libjack_static.a)
add_library(libjackserver STATIC IMPORTED)
set_target_properties(libjackserver PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libjackserver_static.a)
add_library(liblilv STATIC IMPORTED)
set_target_properties(liblilv PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/liblilv.a)
add_library(libsratom STATIC IMPORTED)
set_target_properties(libsratom PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libsratom.a)
add_library(libserd STATIC IMPORTED)
set_target_properties(libserd PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libserd-0.a)
add_library(libjalv STATIC IMPORTED)
set_target_properties(libjalv PROPERTIES IMPORTED_LOCATION
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libjalv_static.a)


# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
    # List C/C++ source files with relative paths to this CMakeLists.txt.
    kitty.cpp
    jni_bridge.cpp
    LiveEffectEngine.cpp
    jalv.cpp
    multi.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
    # List libraries link to the target library
    oboe::oboe
    android
        libsord
        libzix
        libserd
        libsratom
        liblilv
        libjalv
        libjackserver
        libjack
    log)
