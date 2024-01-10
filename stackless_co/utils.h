//
// Created by dell on 2024/1/10.
//

#ifndef COROUTINE_UTILS_H
#define COROUTINE_UTILS_H

namespace stackless_co {
    class Schedule;
    typedef void (*coroutine_func)(Schedule *, void *);
}

#endif //COROUTINE_UTILS_H
