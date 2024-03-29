//
// Created by dell on 2024/1/10.
//

#ifndef COROUTINE_SCHEDULE_H
#define COROUTINE_SCHEDULE_H

#include "utils.h"
#include "coroutine.h"

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdint>

#if __APPLE__ && __MACH__
#include <sys/ucontext.h>
#else

#include <ucontext.h>

#endif

namespace stackless_co {

    class Coroutine;

    class Schedule {

    private:

        // 保存当前协程的栈
        static void _save_stack(Coroutine *c, char *top);

    public:

        // 构造函数
        static Schedule *coroutine_open();

        //
        static void main_func(uint32_t low32, uint32_t high32);

        void coroutine_close();

        int coroutine_new(coroutine_func func, void *ud);

        void coroutine_resume(int id);

        int coroutine_status(int id);

        int coroutine_running() const;

        void coroutine_yield();

    public:

        constexpr static int COROUTINE_DEAD = 0;
        constexpr static int COROUTINE_READY = 1;
        constexpr static int COROUTINE_RUNNING = 2;
        constexpr static int COROUTINE_SUSPEND = 3;

    private:

        constexpr static int STACK_SIZE = 1024 * 1024;

        constexpr static int DEFAULT_COROUTINE = 16;

    private:

        char stack[STACK_SIZE];
        ucontext_t main;
        int nco;
        int cap;
        int running;
        Coroutine **co;
    };
}

#endif //COROUTINE_SCHEDULE_H
