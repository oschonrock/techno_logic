cmake_minimum_required(VERSION 3.28)
project(techno_logic LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
cmake_policy(SET CMP0168 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0168 NEW)

if(PROJECT_IS_TOP_LEVEL)
    if(MSVC)
        set(PROJECT_COMPILE_OPTIONS /Wall /analyze)
        string(APPEND CMAKE_CXX_FLAGS_DEBUG " /fsanitize=address")
        string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " /fsanitize=address")
    else()
        set(PROJECT_COMPILE_OPTIONS -Wall -Wextra -Wpedantic -Wshadow -Wextra-semi
            # -ftime-trace # for compile time reporting
            -Wmissing-noreturn -Wconversion -Wsign-conversion -Wno-ignored-attributes)
        if(CMAKE_BUILD_TYPE STREQUAL "debug")
            if(NOT MINGW) # sanitizers are not working under mingw
                if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
                    message(WARNING "FreeBSD detected: skipping santizers which cause weird errors in OpenSSL")
                else()
                    string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=address,undefined,leak")
                    # string(APPEND CMAKE_CXX_FLAGS_DEBUG " -fsanitize=thread") # alternative to above
                endif()
            endif()
        endif()
    endif()
endif()

if(WIN32)
    # combine everthing into one binary on windows
    set(BUILD_SHARED_LIBS OFF)
    # -static removes dependencies on libgcc.dll, libstdc++.dll and winthread.dll on windows
    set(PROJECT_STATIC_OPTIONS "-static")
else()
    # on linux there is not much point being statically linked
    set(PROJECT_STATIC_OPTIONS "")
endif()

option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." TRUE)
if(${FORCE_COLORED_OUTPUT})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-fdiagnostics-color=always)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fcolor-diagnostics)
    endif()
endif()

include(FetchContent)

fetchcontent_declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
fetchcontent_makeavailable(SFML)

fetchcontent_declare(ImGui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
fetchcontent_makeavailable(ImGui)
fetchcontent_getproperties(ImGui SOURCE_DIR IMGUI_DIR)

set(IMGUI_SFML_FIND_SFML OFF)
fetchcontent_declare(ImGui-SFML
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_BRANCH master
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
fetchcontent_makeavailable(ImGui-SFML)

set(ABSL_PROPAGATE_CXX_STD ON)
fetchcontent_declare(abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp
    GIT_TAG 20240722.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
fetchcontent_makeavailable(abseil-cpp)

add_library(techno_logic_internal include/block/Editor.cpp include/block/Block.cpp)
target_include_directories(techno_logic_internal PUBLIC ./include)
target_link_libraries(techno_logic_internal PUBLIC SFML::Graphics ImGui-SFML::ImGui-SFML absl::flat_hash_map ${PROJECT_STATIC_OPTIONS})

add_executable(techno_logic app/main.cpp)
target_link_libraries(techno_logic techno_logic_internal)
target_compile_features(techno_logic PRIVATE cxx_std_17)
target_compile_options(techno_logic PRIVATE ${PROJECT_COMPILE_OPTIONS})

add_subdirectory(tests)

if(WIN32)
    add_custom_command(
        TARGET techno_logic
        COMMENT "Copy OpenAL DLL"
        PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${SFML_SOURCE_DIR}/extlibs/bin/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>/openal32.dll $<TARGET_FILE_DIR:techno_logic>
        VERBATIM)
endif()
