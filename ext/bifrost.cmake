include(ExternalProject)

set(BIFROST_TAG "v1.0.5")
set(BIFROST_BUILD_TYPE "Release")

ExternalProject_Add(bifrost
    GIT_REPOSITORY https://github.com/pmelsted/bifrost.git
    GIT_TAG ${BIFROST_TAG}
    CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_BUILD_TYPE=${BIFROST_BUILD_TYPE}
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/bifrost"
    INSTALL_COMMAND "")

# Get Bifrost source and binary directories from CMake project
ExternalProject_Get_Property(bifrost source_dir binary_dir)

set(bifrost_source_dir ${source_dir})
set(bifrost_binary_dir ${binary_dir})

# Create a libbifrost target to be used as a dependency
add_library(libbifrost IMPORTED STATIC GLOBAL)

set(BIFROST_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/bifrost/src")
set(BIFROST_LIBRARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/bifrost/src")

# Set libbifrost properties
set_target_properties(libbifrost PROPERTIES
        "IMPORTED_LOCATION" "${BIFROST_LIBRARY_DIR}/libbifrost.a")

add_dependencies(libbifrost bifrost)
