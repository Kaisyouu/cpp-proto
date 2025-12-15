cmake_minimum_required(VERSION 3.20)

project(win-app-template LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
set(SRC
    src/WinMain.cpp
    src/utils/Environment.hpp
    src/utils/Utils.hpp
)

add_executable(${PROJECT_NAME} ${SRC})

# Include directories for header-only 3rd party libs
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/3rd
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4           # warnings
        /EHsc         # exceptions
        /Zc:__cplusplus
        /utf-8
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)

    # If caller specified CMAKE_MSVC_RUNTIME_LIBRARY, honor it on the target
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY ${CMAKE_MSVC_RUNTIME_LIBRARY})
    endif()
endif()

# Map MSVC runtime library to a short flavor name for install layout
set(RUNTIME_FLAVOR "bin")
if(MSVC)
    if(DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        if(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDLL")
            set(RUNTIME_FLAVOR "MD")
        elseif(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDebugDLL")
            set(RUNTIME_FLAVOR "MDd")
        elseif(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreaded")
            set(RUNTIME_FLAVOR "MT")
        elseif(CMAKE_MSVC_RUNTIME_LIBRARY STREQUAL "MultiThreadedDebug")
            set(RUNTIME_FLAVOR "MTd")
        else()
            set(RUNTIME_FLAVOR "unknown")
        endif()
    else()
        # Default VS generators typically use MD/MDd by config; if you want
        # MT/MTd please pass -DCMAKE_MSVC_RUNTIME_LIBRARY=...
        set(RUNTIME_FLAVOR "MD")
    endif()
endif()

# Output directories inside build tree (optional)
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${RUNTIME_FLAVOR}
    LIBRARY DESTINATION ${RUNTIME_FLAVOR}
    ARCHIVE DESTINATION ${RUNTIME_FLAVOR}
)

# Install config.json next to exe if it exists in the source tree
if(EXISTS ${CMAKE_SOURCE_DIR}/config.json)
    install(FILES ${CMAKE_SOURCE_DIR}/config.json DESTINATION ${RUNTIME_FLAVOR})
endif()
