cmake_minimum_required(VERSION 3.16)

ENABLE_LANGUAGE(ASM)

PROJECT(stackful_co_test VERSION 1.0)
SET(STACKFUL_CO_SOURCE_FILES
        switch_context.S
        context.cc
        context.h
        coroutine.cc
        coroutine.h
        environment.cc
        environment.h
        status.h
        utils.h)

add_executable(stackful_co_test stackful_co_test.cc ${STACKFUL_CO_SOURCE_FILES})