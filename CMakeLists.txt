cmake_minimum_required(VERSION 3.8)
project(FILE-TREE C)

set(CMAKE_CXX_STANDARD "17")
set(CMAKE_C_STANDARD "11")
set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wno-sign-compare -pthread")

# Nie chcemy oglądać komunikatów o procentowym postępie kompilacji.
set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)
# Chcemy zobaczyć polecenia wywołane przez make.
set(CMAKE_VERBOSE_MAKEFILE ON)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

set(SOURCE_FILES
        src/main.c
        src/err.c src/err.h
        src/HashMap.c src/HashMap.h
        src/Tree.c src/Tree.h
        src/mtwister.c src/mtwister.h
        src/safe_allocations.h
        )

# Wskazujemy plik wykonywalny (kalkulatora).
add_executable(file_tree ${SOURCE_FILES})

#[[
set(TEST_SOURCE_FILES
        src/Tree.c src/Tree.h
        src/test.c
        )

# Wskazujemy plik wykonywalny (testów).
add_executable(test EXCLUDE_FROM_ALL ${TEST_SOURCE_FILES})
set_target_properties(test PROPERTIES OUTPUT_NAME file_tree_test)
#]]