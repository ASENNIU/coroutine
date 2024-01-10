//
// Created by dell on 2024/1/10.
//

#include "coroutine.h"
#include "utils.h"
#include "schedule.h"

namespace stackless_co {

    Coroutine *Coroutine::new_co(stackless_co::Schedule *s, stackless_co::coroutine_func func, void *ud) {
        auto *co = new(Coroutine);
        co->func = func;
        co->ud = ud;
        co->cap = 0;
        co->size = 0;
        co->status = Schedule::COROUTINE_READY;
        co->stack = nullptr;
        return co;
    }

    void Coroutine::delete_co() {
        free(this->stack);
        free(this);
    }
}