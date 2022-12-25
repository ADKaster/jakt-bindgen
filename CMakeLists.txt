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

add_executable(jakt-bindgen
  src/main.cpp
  src/ClassDeclPrinter.cpp
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
