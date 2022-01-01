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
        src/path.c src/path.h
        src/Tree.c src/Tree.h
        src/mtwister.c src/mtwister.h
        src/safe_allocations.h
        )

# Wskazujemy plik wykonywalny (kalkulatora).
add_executable(file_tree ${SOURCE_FILES})

set(TESTS_PATH "src/tests/")
set(TEST_SOURCE_FILES
        ${TESTS_PATH}concurrent_same_as_some_sequential.c
        ${TESTS_PATH}concurrent_same_as_some_sequential.h
        #${TESTS_PATH}create_sequential_big_random.c
        ${TESTS_PATH}deadlock.c
        ${TESTS_PATH}deadlock.h
        ${TESTS_PATH}liveness.c
        ${TESTS_PATH}liveness.h
        ${TESTS_PATH}sequential_big_random.c
        ${TESTS_PATH}sequential_big_random.h
        ${TESTS_PATH}sequential_small.c
        ${TESTS_PATH}sequential_small.h
        ${TESTS_PATH}test.c
        ${TESTS_PATH}utils.c
        ${TESTS_PATH}utils.h
        ${TESTS_PATH}valid_path.c
        ${TESTS_PATH}valid_path.h
        src/err.c src/err.h
        src/HashMap.c src/HashMap.h
        src/path.c src/path.h
        src/Tree.c src/Tree.h
        src/safe_allocations.h
        )

# Wskazujemy plik wykonwalny (testów).
add_executable(test EXCLUDE_FROM_ALL ${TEST_SOURCE_FILES})
set_target_properties(test PROPERTIES OUTPUT_NAME file_tree_test)