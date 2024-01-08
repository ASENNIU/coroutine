//
// Created by dell on 2024/1/8.
//

#ifndef COROUTINE_UTILS_H
#define COROUTINE_UTILS_H

#include "coroutine.h"
#include "environment.h"

namespace stackful_co {
    namespace this_coroutine {
        inline void yield() {
            return ::stackful_co::Coroutine::yield();
        }
    }

    inline bool test() {
        return Coroutine::test();
    }

    inline Environment &open() {
        return Environment::instance();
    }
}

#endif //COROUTINE_UTILS_H
