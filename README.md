# jakt-bindgen

Bindings generator for C and C++ libraries to jakt


## Building:

Requires llvm and clang-tooling development packages for LLVM 15.x or higher

Configure with

```
cmake -GNinja -B build
# Optionally, -DCMAKE_CXX_COMPILER=clang++-15 -DCMAKE_C_COMPILER=clang-15
```

Build with

```
cmake --build build
```

Run with

```
./build/jakt-bindgen -p <path to compile_commands.json> -n <namespace> -b <base directory for includes> <header files>
```
