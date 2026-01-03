cmake_minimum_required(VERSION 3.10)
project(archiver C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra -ggdb")

# Find pg_config
find_program(PG_CONFIG pg_config)
if(NOT PG_CONFIG)
    message(FATAL_ERROR "pg_config not found. Please install PostgreSQL development files.")
endif()

# Get PostgreSQL include and library directories
execute_process(
    COMMAND ${PG_CONFIG} --includedir
    OUTPUT_VARIABLE PG_INCLUDES
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${PG_CONFIG} --libdir
    OUTPUT_VARIABLE PG_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Find PostgreSQL library
find_library(PQLIB pq HINTS ${PG_LIBS} REQUIRED)

# Collect source files
file(GLOB SRCS "src/*.c")

# Create executable
add_executable(archiver ${SRCS})

# Include directories
target_include_directories(archiver PRIVATE ${PG_INCLUDES})

# Link libraries
target_link_libraries(archiver PRIVATE ${PQLIB})
