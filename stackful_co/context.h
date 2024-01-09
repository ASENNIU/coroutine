//
// Created by dell on 2024/1/8.
//

#ifndef COROUTINE_CONTEXT_H
#define COROUTINE_CONTEXT_H

#include <cstddef>
#include <cstring>
#include <iterator>

namespace stackful_co {
    class Coroutine;

    /**
     * The context of coroutine(in x86-64)
     *
     * low | _registers[0]: r15  |
     *     | _registers[1]: r14  |
     *     | _registers[2]: r13  |
     *     | _registers[3]: r12  |
     *     | _registers[4]: r9   |
     *     | _registers[5]: r8   |
     *     | _registers[6]: rbp  |
     *     | _registers[7]: rdi  |
     *     | _registers[8]: rsi  |
     *     | _registers[9]: ret  |
     *     | _registers[10]: rdx |
     *     | _registers[11]: rcx |
     *     | _registers[12]: rbx |
     * hig | _registers[13]: rsp |
     *
     */

    class Context final {

    public:
        using Callback = void (*)(Coroutine*);
        using Word = void *;

        /***
         * 在 x86-64 汇编语言中，这些寄存器对应的寄存器名称分别为：
         * "ret" 对应的是 "rax" 寄存器，用于存储函数的返回值。
         * "sp" 对应的是 "rsp" 寄存器，栈指针寄存器。
         * "rdi" 对应的是 "rdi" 寄存器，通用寄存器之一，用于存储函数的第一个参数。
         */

        constexpr static size_t STACK_SIZE = 1 << 17;
        constexpr static size_t RDI = 7;
        constexpr static size_t RSI = 8;
        constexpr static size_t RET = 9;
        constexpr static size_t RSP = 13;

    public:
        void prepare(Callback ret, Word rdi);

        void switch_from(Context *previous);

        bool test();

    private:
        Word get_stack_pointer();

        void fill_registers(Word sp, Callback ret, Word rdi, ...);

    private:
        /**
         * We must ensure that registers are at the top of the memory layout.
         *
         * So the context must have no virtual method, and len at least 14！
         */
         Word _registers[14];

         char _stack[STACK_SIZE];
    };
}

#endif //COROUTINE_CONTEXT_H
