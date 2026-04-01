# CMakeList.txt : CMake project for Server, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
project ("jb-reset")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS Off) 

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_EXTENSIONS Off)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${SRC_DIR}/*.cpp"
)

# Add source to this project's executable.
add_executable (${CMAKE_PROJECT_NAME} ${SRC_FILES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${SRC_DIR}
)

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -g
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -rdynamic
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    stdc++exp
)
