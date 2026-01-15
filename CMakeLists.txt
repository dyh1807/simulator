# How to use:
# 
# - mkdir build
# - cd build
# - cmake .. # generate Makefile
# - make -j8 # build with 8 threads
# - make run # run the simulator with default memory image path
cmake_minimum_required(VERSION 3.10)

# 根据选项设置编译器
option(USE_CLANG "Use Clang/LLVM toolchain" OFF)
if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
    message(STATUS "Using LLVM/Clang toolchain")
else()
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
    message(STATUS "Using GNU/GCC toolchain")
endif()

project(rv_simu_mmu)
# Configurable options
option(USE_DEFAULT_IMG "Use default memory image path" ON)
set(EXECUTABLE_NAME "a.out" CACHE STRING "Name of the executable")
set(MEMORY_IMAGE_PATH "${CMAKE_SOURCE_DIR}/baremetal/memory" CACHE STRING "Path to memory image file")
set(LINUX_IMAGE_PATH "${CMAKE_SOURCE_DIR}/baremetal/linux.bin" CACHE STRING "Path to linux image file")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # generate compile_commands.json for IDE support

# Set C++ standard to C++20 (matching --std=c++2a in Makefile)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find all source files
file(GLOB_RECURSE BACKEND_SOURCES "${CMAKE_SOURCE_DIR}/back-end/*.cpp")
file(GLOB_RECURSE FRONTEND_SOURCES "${CMAKE_SOURCE_DIR}/front-end/*.cpp") 
file(GLOB_RECURSE DIFF_SOURCES "${CMAKE_SOURCE_DIR}/diff/*.cpp")
file(GLOB_RECURSE MMU_SOURCES "${CMAKE_SOURCE_DIR}/mmu/*.cpp")
file(GLOB_RECURSE MEMORY_SOURCES "${CMAKE_SOURCE_DIR}/memory/*.cpp")

# Combine all sources (matching Makefile: main.cpp and rv_simu_mmu_v2.cpp)
set(SOURCES 
    ${BACKEND_SOURCES}
    ${FRONTEND_SOURCES}
    ${DIFF_SOURCES}
    ${MMU_SOURCES}
    ${MEMORY_SOURCES}   
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/rv_simu_mmu_v2.cpp
)

# Set include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include/
    ${CMAKE_SOURCE_DIR}/back-end/include/
    ${CMAKE_SOURCE_DIR}/back-end/EXU/include/
    ${CMAKE_SOURCE_DIR}/back-end/tools/include/
    ${CMAKE_SOURCE_DIR}/diff/include/
    ${CMAKE_SOURCE_DIR}/front-end/
    ${CMAKE_SOURCE_DIR}/mmu/include/
    ${CMAKE_SOURCE_DIR}/memory/include/
)

# Create executable
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Set optimization flags (matching Makefile: -O3 -march=native -funroll-loops -mtune=native)
target_compile_options(${EXECUTABLE_NAME} PRIVATE 
    -O3 
    -march=native 
    -funroll-loops 
    -mtune=native
)

# Link libraries (matching Makefile: -lz -lstdc++fs and softfloat.a)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/softfloat.a
    z
    stdc++fs
)

# Add run target (custom target)
add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME} ${MEMORY_IMAGE_PATH}
    DEPENDS ${EXECUTABLE_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running the simulator with memory image: ${MEMORY_IMAGE_PATH}"
)

add_custom_target(linux
    COMMAND ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME} ${LINUX_IMAGE_PATH}
    DEPENDS ${EXECUTABLE_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running the simulator with memory image: ${LINUX_IMAGE_PATH}"
)

# Add clean target (custom target that removes build artifacts)
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/a.out
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_SOURCE_DIR}/baremetal/memory
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_SOURCE_DIR}/baremetal/test.code
    COMMENT "Cleaning all build artifacts and generated files"
)

# Coverage build target (matching Makefile's cov target)
add_custom_target(cov
    COMMAND ${CMAKE_CXX_COMPILER} 
        -I${CMAKE_SOURCE_DIR}/include/
        -I${CMAKE_SOURCE_DIR}/back-end/include/
        -I${CMAKE_SOURCE_DIR}/back-end/EXU/include/
        -I${CMAKE_SOURCE_DIR}/back-end/tools/include/
        -I${CMAKE_SOURCE_DIR}/diff/include/
        -I${CMAKE_SOURCE_DIR}/front-end/
        -I${CMAKE_SOURCE_DIR}/mmu/include/
        -I${CMAKE_SOURCE_DIR}/memory/include/
        ${SOURCES} ${CMAKE_SOURCE_DIR}/softfloat.a -O0 --coverage -o ${EXECUTABLE_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building with coverage support"
)

# Add gdb target (matching Makefile)
add_custom_target(gdb
    COMMAND ${CMAKE_CXX_COMPILER}
        -I${CMAKE_SOURCE_DIR}/include/
        -I${CMAKE_SOURCE_DIR}/back-end/include/
        -I${CMAKE_SOURCE_DIR}/back-end/EXU/include/
        -I${CMAKE_SOURCE_DIR}/back-end/tools/include/
        -I${CMAKE_SOURCE_DIR}/diff/include/
        -I${CMAKE_SOURCE_DIR}/front-end/
        -I${CMAKE_SOURCE_DIR}/mmu/include/
        -I${CMAKE_SOURCE_DIR}/memory/include/
        ${SOURCES} ${CMAKE_SOURCE_DIR}/softfloat.a -O0 -g -march=native -lz -o ${EXECUTABLE_NAME}
    COMMAND gdb --args ./${EXECUTABLE_NAME} ${MEMORY_IMAGE_PATH}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building and running with gdb debugger"
)

# Add gdb_linux target (matching Makefile)
add_custom_target(gdb_linux
    COMMAND ${CMAKE_CXX_COMPILER}
        -I${CMAKE_SOURCE_DIR}/include/
        -I${CMAKE_SOURCE_DIR}/back-end/include/
        -I${CMAKE_SOURCE_DIR}/back-end/EXU/include/
        -I${CMAKE_SOURCE_DIR}/back-end/tools/include/
        -I${CMAKE_SOURCE_DIR}/diff/include/
        -I${CMAKE_SOURCE_DIR}/front-end/
        -I${CMAKE_SOURCE_DIR}/mmu/include/
        -I${CMAKE_SOURCE_DIR}/memory/include/
        ${SOURCES} ${CMAKE_SOURCE_DIR}/softfloat.a -g -o ${EXECUTABLE_NAME}
    COMMAND gdb --args ./${EXECUTABLE_NAME} ${LINUX_IMAGE_PATH}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building and running with gdb debugger (linux image)"
)
