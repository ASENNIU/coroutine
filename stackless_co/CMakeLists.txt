cmake_minimum_required(VERSION 3.16)

PROJECT(stackless_co_test)

ENABLE_LANGUAGE(ASM)

SET(STACKLESS_CO_SOURCE_FILES
        coroutine.h
        coroutine.cc
        schedule.h
        schedule.cc
        utils.h)

ADD_EXECUTABLE(stackless_co_test stackless_co_test.cc ${STACKLESS_CO_SOURCE_FILES})