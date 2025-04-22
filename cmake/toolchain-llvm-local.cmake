# Point CMake at your local LLVM build:
if(NOT DEFINED ENV{LLVM_ROOT})
    message(FATAL_ERROR
            "Please set LLVM_ROOT to your LLVM install, e.g.\n"
            "  export LLVM_ROOT=\"$HOME/tools/llvm/LLVM-20.1.0-Linux-X64\"")
endif()
set(LLVM_ROOT $ENV{LLVM_ROOT} CACHE PATH "Local LLVM root")

# Use clang/clang++ from there:
set(CMAKE_C_COMPILER   "${LLVM_ROOT}/bin/clang"  CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang++" CACHE FILEPATH "" FORCE)

# Force C++23:
set(CMAKE_CXX_STANDARD 23            CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell clang to use libc++ from your tree:
add_compile_options(-stdlib=libc++ -I${LLVM_ROOT}/include/c++/v1)
add_link_options   (-stdlib=libc++ -Wl,-rpath,${LLVM_ROOT}/lib/x86_64-unknown-linux-gnu/)
