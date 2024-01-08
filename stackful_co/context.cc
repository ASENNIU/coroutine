//
// Created by dell on 2024/1/8.
//

#include "context.h"

extern "C" {
extern void switch_context(stackful_co::Context *, stackful_co::Context*) asm("switch_context");
}

namespace stackful_co {

    void Context::switch_from(Context *previous) {
        switch_context(previous, this);
    }

    void Context::prepare(stackful_co::Context::Callback ret, stackful_co::Context::Word rdi) {
        Word sp = get_stack_pointer();
        fill_registers(sp, ret, rdi);
    }

    bool Context::test() {
        char current;
        ptrdiff_t diff = std::distance(std::begin(_stack), &current);
        return diff >= 0 && diff < STACK_SIZE;
    }

    Context::Word Context::get_stack_pointer() {
        auto sp = std::end(_stack) - sizeof(Word);
        sp = decltype(sp)(reinterpret_cast<size_t>(sp) & (~0xF));
        return sp;
    }

    void Context::fill_registers(stackful_co::Context::Word sp, stackful_co::Context::Callback ret,
                                 stackful_co::Context::Word rdi, ...) {
        memset(_registers, 0, sizeof _registers);
        auto p_ret = (Word*) sp;
        *p_ret = (Word)ret;
        _registers[RSP] = sp;
        _registers[RET] = *p_ret;
        _registers[RDI] = rdi;
    }
}

