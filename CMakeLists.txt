cmake_minimum_required(VERSION 3.4)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tools/cmake")

file(STRINGS "${CMAKE_SOURCE_DIR}/dependencies.list" DEPENDENCIES)
message("Dependencies: ${DEPENDENCIES}")
foreach(LINE IN LISTS DEPENDENCIES)
    string(REGEX MATCHALL "([^=]+)" KEY_VALUE ${LINE})
    list(GET KEY_VALUE 0 KEY)
    list(GET KEY_VALUE 1 VALUE)
    set(DEP_${KEY} ${VALUE})
endforeach()

if(${CMAKE_EXTRA_GENERATOR} MATCHES "Eclipse CDT4")
    set(CMAKE_CXX_COMPILER_ARG1 "-std=c++14" CACHE STRING "C++ version for eclipse" FORCE)
    set(CMAKE_ECLIPSE_VERSION "4.6.2" CACHE STRING "Eclipse version" FORCE)
endif()

#check version format
string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+$" CONFIG_VERSION ${DEP_VERSION})
if (NOT CONFIG_VERSION)
    string(REGEX MATCH "^[0-9]+\.[0-9]+\.[0-9]+-.+$" CONFIG_VERSION ${DEP_VERSION})
    if (NOT CONFIG_VERSION)
        message(FATAL_ERROR "Wrong version number format: ${DEP_VERSION}.")
    endif()
    set(extended_version 1)
endif()

# Split "x.y.z-t" into a list x;y;z;t
string(REGEX MATCHALL "[^.-]+" VERSION_LIST ${CONFIG_VERSION})
list(GET VERSION_LIST 0 CONFIG_VERSION_MAJOR)
list(GET VERSION_LIST 1 CONFIG_VERSION_MINOR)
list(GET VERSION_LIST 2 CONFIG_VERSION_PATCH)
if (extended_version)
    list(GET VERSION_LIST 3 CONFIG_VERSION_TWEAK)
endif()

project(realm)

# Include general CMake modules
include(GNUInstallDirs)
include(CheckIncludeFiles)
include(GetGitRevisionDescription)
include(Utilities)
include(SpecialtyBuilds)

# Get accurate git-describe version
git_describe(REALM_VERSION)

# Project-wide build flags
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set global warnings settings
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wundef -Wshadow")
    # TODO: Remove this when fixed
    if(ANDROID)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized -Wno-uninitialized")
    elseif(${CMAKE_CXX_COMPILER_ID} MATCHES ".*[Cc]lang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code -Wshorten-64-to-32 -Wold-style-cast -Wconditional-uninitialized -Wextra-semi -Wno-nested-anon-types")
    endif()
endif()

# Add REALM_DEBUG=1 to compile definitions for Debug / MinSizeDebug targets
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:REALM_DEBUG=1>
    $<$<CONFIG:MinSizeDebug>:REALM_DEBUG=1>
)

set(CMAKE_DEBUG_POSTFIX "-dbg")
set(CMAKE_MINSIZEDEBUG_POSTFIX "-dbg")

set(CMAKE_POSITION_INDEPENDENT_CODE true)

# Add REALM_UWP=1 to compile with Windows UWP support
if(WINDOWS_STORE AND CMAKE_SYSTEM_VERSION STREQUAL 10.0)
    add_definitions(-DREALM_UWP=1 -D_SCL_SECURE_NO_WARNINGS)
endif()

if(MSVC)
    add_compile_options(
        /MP # Enable multi-processor compilation
    )
    if(NOT WINDOWS_STORE)
        # Statically link the run-time library
        # https://docs.microsoft.com/bg-bg/cpp/build/reference/md-mt-ld-use-run-time-library
        # https://cmake.org/Wiki/CMake_FAQ#How_can_I_build_my_MSVC_application_with_a_static_runtime.3F
        foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
           if(${flag_var} MATCHES "/MD")
              string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
           endif()
        endforeach()
    endif()
endif()

# Platform-specific build configuration
if(APPLE)
    find_library(Foundation Foundation)
elseif(ANDROID)
    set(ANDROID_OPENSSL_VERSION "1.0.2k")
    list(APPEND PLATFORM_LIBRARIES log android)
endif()

# Find dependencies
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
list(APPEND PLATFORM_LIBRARIES Threads::Threads)

# Options (passed to CMake)
option(REALM_ENABLE_ASSERTIONS "Enable assertions in release mode." OFF)
option(REALM_ENABLE_ALLOC_SET_ZERO "Zero all allocations." OFF)
option(REALM_ENABLE_ENCRYPTION "Enable encryption." ON)
option(REALM_ENABLE_MEMDEBUG "Add additional memory checks" OFF)
option(REALM_VALGRIND "Tell the test suite we are running with valgrind" OFF)
set(REALM_MAX_BPNODE_SIZE "1000" CACHE STRING "Max B+ tree node size.")

check_include_files(malloc.h HAVE_MALLOC_H)

# Store configuration in header file
configure_file(src/realm/util/config.h.in src/realm/util/config.h)

# Configure source code to use right version number
configure_file(src/realm/version.hpp.in src/realm/version.hpp)

set(DEPRECATED_CONFIG_FILE "${CMAKE_SOURCE_DIR}/src/realm/util/config.h")
if(EXISTS "${DEPRECATED_CONFIG_FILE}")
    message(FATAL_ERROR "${DEPRECATED_CONFIG_FILE} exists in the source directory, and will take precedence over the generated configuration in the build directory. Please remove this file before continuing. Alternatively, you can also clean your realm-core to remove this and other stale files: git clean -xfd")
endif()

# Tell the build system where to find the sources (and generated sources)
include_directories(src)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src) # For generated files (like config.h)

# Include additional CMakeLists
add_subdirectory(src)

# Enable CTest and include unit tests
if(NOT REALM_BUILD_LIB_ONLY AND NOT REALM_NO_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# CPack
install(FILES LICENSE CHANGELOG.md DESTINATION "doc/realm" COMPONENT devel)

if(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(ANDROID)
    set(CPACK_GENERATOR "TGZ")
else()
    set(CPACK_GENERATOR "TGZ;RPM;DEB")
endif()
set(CPACK_PACKAGE_NAME "realm-core-${CMAKE_BUILD_TYPE}")
set(CPACK_PACKAGE_VERSION ${REALM_VERSION})
set(CPACK_PACKAGE_VENDOR "Realm Inc.")
set(CPACK_PACKAGE_CONTACT "Realm <help@realm.io>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Realm is a mobile database: a replacement for Core Data & SQLite")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/tools/cmake/run_ldconfig.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/tools/cmake/run_ldconfig.sh")
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/tools/cmake/run_ldconfig.sh")
set(CPACK_DEBIAN_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/tools/cmake/run_ldconfig.sh")
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
include(CPack)
cpack_add_component(runtime DEPENDS runtime)
cpack_add_component(devel DEPENDS devel)

