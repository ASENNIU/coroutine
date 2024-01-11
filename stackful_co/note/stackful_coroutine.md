STACKFUL_COROUTINE 有栈协程的原理和实现

# 实现



基于汇编实现有栈协程，参考微信的libco 

```http
参考代码：https://github.com/JasonkayZK/cpp-learn/tree/coroutine/stack_co
微信libco注释版：https://github.com/chenyahui/AnnotatedCode/tree/master/libco
```

## 协程环境

本例中实现的协程不支持跨线程，而是每个线程分配一个环境，来维护该线程下运行中的协程之间的层次关系，代码如下：

**stackful_co/environment.h** 

```C++
//
// Created by dell on 2024/1/8.
//

#ifndef coroutine_ENVIRONMENT_H
#define coroutine_ENVIRONMENT_H


#include "coroutine.h"

#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>

namespace stackful_co {
    class Coroutine;

    class Environment {
        friend class Coroutine;

    public:
        // Thread-local instance
        static Environment& instance();

        // Factory method
        template<typename Entry, typename ...Args>
        std::shared_ptr<Coroutine> create_coroutine(Entry &&entry, Args&& ...args);

        // No copy constructor
        Environment(const Environment&) = delete;

        // No Assignment Operator
        Environment &operator=(const Environment&) = delete;

        // Get current coroutine in the stack;
        Coroutine* current();

    private:
        // No explicit constructor
        Environment();

        void push(std::shared_ptr<Coroutine> coroutine);

        void pop();

    private:
        // Coroutine calling stack
        std::array<std::shared_ptr<Coroutine>, 1024> _c_stack;

        // Top of the coroutine calling stack
        size_t _c_stack_top;

        // Main coroutine(root)
        std::shared_ptr<Coroutine> _main;
    };

    // A default factory method
    template<typename Entry,typename ...Args>
    inline std::shared_ptr<Coroutine> Environment::create_coroutine(Entry&& entry, Args&& ...args) {
        return std::make_shared<Coroutine>(this, std::forward<Entry>(entry), std::forward<Args>(args)...);
    }
} // namespace stackful_co


#endif //COROUTINE_ENVIRONMENT_H

```

上面的代码定义了协程运行的环境（Environment）。

**需要注意的是：**

**我们显式的删除了 Environment 的拷贝构造函数和赋值运算符，并且将构造函数声明为 private，仅提供工厂方法来创建 Environment 实例。**

**而 Environment 在实现时，使用的是 thread_local，从而保证了每个线程仅会存在单个实例。**

对外暴露了 `current` 方法用于获取当前环境下调用栈中的协程，而三个成员变量是用来保存或记录当前调用协程的：

- `_c_stack`；
- `_c_stack_top`；
- `_main`；



当使用 `Environment` 类的 `create_coroutine` 方法时，该方法接受一个入口函数和一组可变参数，并返回一个指向 `Coroutine` 类对象的 `std::shared_ptr`。以下是对该方法的解释：

```cpp
template<typename Entry, typename ...Args>
inline std::shared_ptr<Coroutine> Environment::create_coroutine(Entry&& entry, Args&& ...args) {
    return std::make_shared<Coroutine>(this, std::forward<Entry>(entry), std::forward<Args>(args)...);
}
```

- **模板声明：**
  - `create_coroutine` 是一个模板方法，它接受模板参数 `Entry` 和可变参数模板 `Args...`。这使得方法能够接受不同类型的入口函数和参数。

- **返回类型：**
  - 返回类型为 `std::shared_ptr<Coroutine>`，即指向 `Coroutine` 类对象的智能指针。

- **创建协程对象：**
  - 使用 `std::make_shared` 创建一个新的 `Coroutine` 类对象。
  - 将 `this` 指针和通过 `std::forward` 完美转发的入口函数和参数传递给 `Coroutine` 构造函数。（由于是对类成员函数的封装，需要传递this指针）

- **完美转发：**

  完美转发是 C++ 中的一种机制，它允许函数或者方法将它们接收到的参数，以相同的值类别（左值引用或右值引用）转发给另一个函数或者方法。这个机制的目的是保留原始调用者传递给调用函数的值的左值或右值特性。

- - `std::forward` 用于实现完美转发，确保传递的参数类型保持不变，避免不必要的拷贝和移动操作。


  - 使用 `std::forward<Entry>(entry)` 将入口函数 `entry` 完美转发给 `Coroutine` 构造函数。
  - 使用 `std::forward<Args>(args)...` 将参数包 `args` 完美转发给 `Coroutine` 构造函数。

这个模板方法允许在 `Environment` 类中创建协程的智能指针，并支持不同类型的入口函数和参数。通过模板和完美转发，这段代码实现了通用性和灵活性。



`Environment`类对应的实现:

**stackful_co/environment.cc**

```C++
//
// Created by dell on 2024/1/8.
//

#include "environment.h"

namespace stackful_co {

    Environment& Environment::instance() {
        static thread_local Environment env;
        return env;
    }

    Coroutine* Environment::current() {
        return this->_c_stack[this->_c_stack_top - 1].get();
    }

    void Environment::push(std::shared_ptr <Coroutine> coroutine) {
        _c_stack[_c_stack_top++] = std::move(coroutine);
    }

    void Environment::pop() {
        _c_stack_top--;
    }

    Environment::Environment() : _c_stack_top(0) {
        _main = std::make_shared<Coroutine>(this, [](){});
        push(_main);
    }
} // namespace stackful_co
```

实现内容比较简单，主要是：

- `instance`：工厂方法；
- `current`：获取当前环境下栈顶的协程实例；
- `push/pop`：协程压栈/出栈；

下面来看协程实例相关的定义。



## 协程状态

协程相关的状态在 `status.h` 头文件中定义了：

**stackful_co/status.h**

```C++
//
// Created by dell on 2024/1/8.
//

#ifndef COROUTINE_STATUS_H
#define COROUTINE_STATUS_H

namespace stackful_co {

    // The status of the coroutine
    struct Status {
        using Bitmask = unsigned char ;

        constexpr static Bitmask MAIN = 1 << 0;
        constexpr static Bitmask IDLE = 1 << 1;
        constexpr static Bitmask RUNNING = 1 << 2;
        constexpr static Bitmask EXIT = 1 << 3;

        Bitmask operator&(Bitmask mask) const {
            return flag & mask;
        }

        Bitmask operator|(Bitmask mask) const {
            return flag | mask;
        }

        Bitmask operator^(Bitmask mask) const {
            return flag ^ mask;
        }

        Bitmask operator&=(Bitmask mask) {
            flag &= mask;
        }

        Bitmask operator|=(Bitmask mask) {
            flag |= mask;
        }

        Bitmask operator^=(Bitmask mask) {
            flag ^= mask;
        }

        Bitmask flag;
    };
}

#endif //COROUTINE_STATUS_H

```

协程相关的状态主要包括了下面几类：

- MAIN：仅作为协程入口调用栈的标记；
- IDLE：空闲状态；
- RUNNING：执行中；
- EXIT：线程退出。

并重载了一些运算符。



##协程实例 

协程的实例主要是用于支持接口 `resume` 和 `yield`；

代码如下：

**stackful_co/coroutine.h**

```c++
//
// Created by dell on 2024/1/8.
//

#ifndef COROUTINE_COROUTINE_H
#define COROUTINE_COROUTINE_H

#include "status.h"
#include "context.h"

#include <functional>
#include <memory>

namespace stackful_co {

    class Environment;

    class Coroutine : public std::enable_shared_from_this<Coroutine> {

        friend class Environment;
        friend class Context;

    public:
        static Coroutine& current();

        // 测试当前控制流是否位于协程上下文
        static bool test();

        // 获取当前运行时信息
        // 目前仅支持运行、退出和主协程的判断
        Status runtime() const;

        bool exit() const;

        bool running() const;

        // 核心操作：resume 和 yield
        // usage: Corountine::current().yield()
        static void yield();

        // Note1: 允许处于EXIT状态的协程重入，从而再次resume
        //        如果不使用这种特性，则用exit()/running()判断
        //
        // Note2: 返回值可以得知resume执行后的运行时状态
        //        但是这个只适合简单的场合
        //        如果接下来其他协程的运行也影响了该协程的状态
        //        那建议用runtime()获取
        Status resume();

        // 类比于线程，线程不可复制和转移，但控制权可以转移
        Coroutine(const Coroutine&) = delete;

        Coroutine(const Coroutine&&) = delete;

        Coroutine& operator=(const Coroutine&) = delete;
        Coroutine& operator=(const Coroutine&&) = delete;

    public:

        // 构造Coroutine执行函数，entry为入口函数，对应传参为args
        // Note：处于可重入的考虑，entry强制为值语义
        template<typename Entry, typename ...Args>
        Coroutine(Environment* master, Entry entry, Args ...args) :
            _entry([=]{ entry(/*std::move*/(args)...); }),
            _master(master) {}

    private:
        static void call_when_finish(Coroutine* coroutine);

    private:
        Status _runtime{};

        Context _context{};

        std::function<void()> _entry;

        Environment* _master;
    };
}


#endif //COROUTINE_COROUTINE_H

```

在 Coroutine 中定义了：

- `_runtime`：当前线程的状态；
- `_context`：当前协程的上下文信息（核心）；
- `_entry`：协程函数入口；
- `_master`：协程调用环境；



 `Coroutine` 类的构造函数，用于初始化协程对象。以下是对其主要部分的解释：

```cpp
// 构造Coroutine执行函数，entry为入口函数，对应传参为args
// Note：处于可重入的考虑，entry强制为值语义
template<typename Entry, typename ...Args>
Coroutine(Environment* master, Entry entry, Args ...args) :
    _entry([=]{ entry(/*std::move*/(args)...); }),
    _master(master) {}
```

主要要点解释：

1. **模板构造函数：** 这是一个模板构造函数，使用了模板参数 `Entry` 和可变参数模板 `Args...`。这使得构造函数能够接受不同类型的入口函数和参数。

2. **入口函数及参数：** 构造函数接受一个指向 `Environment` 对象的指针 `master`，入口函数 `entry`，以及参数包 `args...`。

3. **_entry 成员初始化：** `_entry` 是一个 `std::function` 对象，通过 lambda 表达式初始化。该 lambda 表达式捕获了参数 `entry` 和参数包 `args...`，并在其内部调用了入口函数。

4. **可重入性注释：** 注释中提到了“可重入性”的考虑，表示入口函数 `entry` 被强制为值语义。这意味着在初始化 `_entry` 时，`entry` 和 `args...` 的值会被拷贝到 lambda 表达式内，以避免在协程执行期间对外部变量的引用问题。

5. **_master 成员初始化：** `_master` 成员用于存储协程所属的 `Environment` 对象，通过构造函数的参数进行初始化。

总体而言，这个构造函数的作用是将入口函数和参数初始化为协程对象的内部成员，为协程的执行做好准备工作。注释中的可重入性提醒着开发者，这样的设计考虑到了协程可能需要在不同的上下文中执行，因此使用值语义以避免潜在的问题。



对应的方法实现：

**stackful_co/coroutine.cc**

```c++
//
// Created by dell on 2024/1/8.
//

#include "coroutine.h"
#include "environment.h"

namespace stackful_co {

    Coroutine& Coroutine::current() {
        return *Environment::instance().current();
    }

    bool Coroutine::test() {
        return current()._context.test();
    }

    Status Coroutine::runtime() const {
        return _runtime;
    }

    bool Coroutine::exit() const {
        return _runtime & Status::EXIT;
    }

    bool Coroutine::running() const {
        return _runtime & Status::RUNNING;
    }

    Status Coroutine::resume() {
        // 检查协程是否正在运行
        if (!(_runtime & Status::RUNNING)) {
            // 准备协程的上下文，设置回调函数为 call_when_finish
            _context.prepare(Coroutine::call_when_finish, this);

            // 将协程标记为正在运行中，同时清除退出标记
            _runtime |= Status::RUNNING;
            _runtime &= ~Status::EXIT;
        }

        // 当前正在执行的协程
        auto previous = _master->current();

        // 将当前协程压栈
        _master->push(shared_from_this());

        // 切换上下文
        _context.switch_from(&previous->_context);

        // 返回当前协程的运行时状态
        return _runtime;
    }

    void Coroutine::yield() {
        // 获取当前协程的引用
        auto &coroutine = current();
        auto &current_context = coroutine._context;

        coroutine._master->pop();

        // 切换到上一个协程
        auto &previous_context = current()._context;
        previous_context.switch_from(&current_context);
    }

    void Coroutine::call_when_finish(Coroutine *coroutine) {
        auto &routine = coroutine->_entry;
        auto &runtime = coroutine->_runtime;
        
        // 调用协程的入口函数
        if (routine) routine();
        
        // 这里已经运行结束了，标记协程为退出状态和非运行中状态
        runtime ^= (Status::EXIT | Status::RUNNING);

        // coroutine->yield
        yield();
    }
}
```

协程内部的各种操作主要是调用其内部的 Context 实现的，下面我们来看。



## 上下文实例

上下文信息 `Context` 用于维护协程 Coroutine 的函数调用信息。

需要注意的是：上下文需要确保内存布局准确无误才能使用，一个`context`的起始地址必须是`regs[0]`，否则会影响后面的协程切换正确性。

代码如下：

**stack_co/context.h**

```c++
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

```

对应的 C++ 文件：

**stackful_co/context.cc**

```C++
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
```

其中，C++中的实现使用了汇编：

**stackful_co/switch_context.S**

```asm
.globl switch_context
.type  switch_context, @function
switch_context:
    ; 保存当前协程的寄存器状态到上下文
    movq %rsp, %rax
    movq %rax, 104(%rdi)
    movq %rbx, 96(%rdi)
    movq %rcx, 88(%rdi)
    movq %rdx, 80(%rdi)
    movq 0(%rax), %rax
    movq %rax, 72(%rdi)
    movq %rsi, 64(%rdi)
    movq %rdi, 56(%rdi)
    movq %rbp, 48(%rdi)
    movq %r8, 40(%rdi)
    movq %r9, 32(%rdi)
    movq %r12, 24(%rdi)
    movq %r13, 16(%rdi)
    movq %r14, 8(%rdi)
    movq %r15, (%rdi)

    ; 从目标协程的上下文（%rsi）恢复寄存器状态
    movq 48(%rsi), %rbp
    movq 104(%rsi), %rsp
    movq (%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 32(%rsi), %r9
    movq 40(%rsi), %r8
    movq 56(%rsi), %rdi
    movq 72(%rsi), %rax
    movq 80(%rsi), %rdx
    movq 88(%rsi), %rcx
    movq 96(%rsi), %rbx
    movq 64(%rsi), %rsi

    ; 将返回地址（目标协程）放到栈顶，然后返回
    movq %rax, (%rsp)
    xorq %rax, %rax
    ret
```

上面的 Context 的核心功能 `switch_context` 主要就是通过汇编 `stack_co/switch_context.S` 实现的，主要核心就是一个`switch`过程。

这里调用时`rdi`（previous）和`rsi`（next）分别指向`Context`实例的地址。首先是保存当前的寄存器上下文到 previous 的 _registers 中：

- 真实的`rsp`存放到 previous 的 104 中（13*8，可能是位于栈区的rsp，也可能是协程伪造的rsp），而返回地址放到previous的 72 中；
- 其余的按部就班赋值到 previous 的 _registers 中；

恢复过程则是从`next`的_registers中恢复：

- 首次启动时已经由`preapre`方法把必要的 ret 和函数传参rdi被写到_registers上了，因此恢复时相当于直接调用对应函数（既调用`call_when_finish(next)`，里面嵌套着实际的用户回调）；
- 如果非首次启动，那么ret就是协程执行中的控制流；
- 不管怎样，在恢复时，把当前 `rsp` 覆盖 `return address`，然后使用 `ret` 指令执行后就能切换到对应的控制流。