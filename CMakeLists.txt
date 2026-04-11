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

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE kernel32)
endif ()

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT WIN32)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        -rdynamic
    )
endif()

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:-static>
            $<$<CONFIG:RelWithDebInfo>:-static>
    )
endif ()

# For Debug
if(MINGW AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MINGW_BIN}/libstdc++-6.dll"
        "${MINGW_BIN}/libgcc_s_seh-1.dll"
        "${MINGW_BIN}/libwinpthread-1.dll"
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>
        COMMENT "Copying MinGW runtime DLLs (Debug)"
        VERBATIM
    )
endif()
