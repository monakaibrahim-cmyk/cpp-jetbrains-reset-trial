# CMakeList.txt : CMake project for cpp-jetbrains-reset-trial, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
project ("jb-reset")

if (DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
endif ()

if (NOT VCPKG_ROOT)
    find_program(VCPKG_EXECUTABLE vcpkg)
    if (VCPKG_EXECUTABLE)
        get_filename_component(VCPKG_ROOT "${VCPKG_EXECUTABLE}" DIRECTORY)

        if (NOT EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
            get_filename_component(VCPKG_PARENT "${VCPKG_ROOT}" DIRECTORY)
            if (EXISTS "${VCPKG_PARENT}/scripts/buildsystems/vcpkg.cmake")
                set(VCPKG_ROOT "${VCPKG_PARENT}")
            endif ()
        endif ()
    endif ()
endif ()

if (NOT VCPKG_ROOT OR NOT EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_ROOT "${CMAKE_BINARY_DIR}/_vcpkg")
    message(STATUS "vcpkg not found. Using temporary: ${VCPKG_ROOT}")
else ()
    message(STATUS "vcpkg found: ${VCPKG_ROOT}")
endif ()

set(VCPKG_TOOLCHAIN "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

if (NOT EXISTS "${VCPKG_TOOLCHAIN}")
    message(STATUS "Bootstrapping vcpkg...")
    find_package(Git REQUIRED)
    if (NOT EXISTS "${VCPKG_ROOT}/.git")
        execute_process(COMMAND ${GIT_EXECUTABLE} clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT})
    endif ()

    if (WIN32)
        execute_process(COMMAND "${VCPKG_ROOT}/bootstrap-vcpkg.bat" WORKING_DIRECTORY ${VCPKG_ROOT})
    else ()
        execute_process(COMMAND bash "${VCPKG_ROOT}/bootstrap-vcpkg.sh" WORKING_DIRECTORY ${VCPKG_ROOT})
    endif ()
endif ()

set(VCPKG_MANIFEST_MODE ON CACHE BOOL "")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN}" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS Off)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_EXTENSIONS Off)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${SRC_DIR}/*.cpp"
)

# Add source to this project's executable.
add_executable (${CMAKE_PROJECT_NAME} ${SRC_FILES})

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${SRC_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        nlohmann_json::nlohmann_json
        CURL::libcurl
)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            ws2_32
            crypt32
            wldap32
            kernel32
    )
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
