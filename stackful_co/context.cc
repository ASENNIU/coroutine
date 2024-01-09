//
// Created by dell on 2024/1/8.
//

#include "context.h"

// 使用 extern "C" 包裹，以便与汇编代码进行链接
extern "C" {
extern void switch_context(stackful_co::Context *, stackful_co::Context*) asm("switch_context");
}

namespace stackful_co {

    void Context::switch_from(Context *previous) {
        switch_context(previous, this);
    }

    void Context::prepare(stackful_co::Context::Callback ret, stackful_co::Context::Word rdi) {
        // 获取栈指针
        Word sp = get_stack_pointer();
        fill_registers(sp, ret, rdi);
    }

    bool Context::test() {
        char current;
        ptrdiff_t diff = std::distance(std::begin(_stack), &current);

        // current的栈地址是否在协程的执行栈范围
        return diff >= 0 && diff < STACK_SIZE;
    }

    Context::Word Context::get_stack_pointer() {
        // 获取栈指针，确保对齐到16字节边界
        auto sp = std::end(_stack) - sizeof(Word);
        sp = decltype(sp)(reinterpret_cast<size_t>(sp) & (~0xF));
        return sp;
    }

    void Context::fill_registers(stackful_co::Context::Word sp, stackful_co::Context::Callback ret,
                                 stackful_co::Context::Word rdi, ...) {
        // 清空寄存器数组，初始化为零
        memset(_registers, 0, sizeof _registers);

        // 将返回地址（回调函数的地址）存储在栈上
        auto p_ret = (Word*) sp;
        *p_ret = (Word)ret;

        // 填充 RSP、RET、RDI 寄存器
        _registers[RSP] = sp;
        _registers[RET] = *p_ret;
        _registers[RDI] = rdi;
    }
}

/***
 * 在很多系统中，特别是 x86-64 架构的计算机体系结构中，对齐到 16 字节的边界是一种常见的做法。
 * 这种对齐方式的背后有一些性能和硬件相关的考虑：
 *
 * 1.性能优化：很多 CPU 架构对于数据的访问在地址按照 16 字节对齐的情况下更为高效。例如，SSE（Streaming SIMD Extensions）和
 * AVX（Advanced Vector Extensions）等指令集通常要求数据在内存中的地址是 16 字节对齐的。
 * 对齐到 16 字节可以使得程序在使用这些指令集进行 SIMD（单指令多数据）操作时获得更好的性能。
 *
 * 2.缓存性能：缓存行的大小通常是 64 字节，而 16 字节对齐可以确保数据不会横跨两个缓存行。
 * 这有助于减少缓存的冲突和提高缓存的利用率，从而提升程序的整体性能。
 *
 * 3.硬件要求：有些硬件可能对数据的对齐方式有要求，不对齐可能导致硬件异常或者额外的性能开销。
 *
 * 对于协程的上下文切换，尤其是在需要进行低级别的汇编操作的情况下，采用对齐到 16 字节的做法可以更好地满足硬件的要求，
 * 并且可能获得更好的性能。在这种场景下，对齐到 16 字节是一种良好的实践。
*/