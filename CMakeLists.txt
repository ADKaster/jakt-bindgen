cmake_minimum_required(VERSION 3.25)

project(
  jakt-bindgen
  VERSION 1.0.0
  DESCRIPTION "Application to generate importable jakt headers from C++ code"
  HOMEPAGE_URL "https://github.com/ADKaster/jakt-bindgen"
  LANGUAGES C CXX
)

find_package(Clang CONFIG REQUIRED)
find_package(LLVM CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_UNDEFINED_SANITIZER "Check for UB" OFF)
option(ENABLE_ADDRESS_SANITIZER "Check for memory errors" OFF)

add_executable(jakt-bindgen
  src/main.cpp
  src/CXXClassListener.cpp
  src/JaktGenerator.cpp
  src/SourceFileHandler.cpp
)

target_include_directories(jakt-bindgen SYSTEM PRIVATE ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})
target_compile_features(jakt-bindgen PRIVATE cxx_std_20)
target_link_libraries(jakt-bindgen PRIVATE
  LLVMSupport
  clangAST
  clangASTMatchers
  clangBasic
  clangDriver
  clangFormat
  clangFrontend
  clangLex
  clangRewrite
  clangSerialization
  clangToolingCore
  clangTooling
)

if (ENABLE_UNDEFINED_SANITIZER)
  target_compile_options(jakt-bindgen PUBLIC -fsanitize=undefined)
  target_link_options(jakt-bindgen PUBLIC -fsanitize=undefined)
endif()

if (ENABLE_ADDRESS_SANITIZER)
  target_compile_options(jakt-bindgen PUBLIC -fsanitize=address)
  target_link_options(jakt-bindgen PUBLIC -fsanitize=address)
endif()
