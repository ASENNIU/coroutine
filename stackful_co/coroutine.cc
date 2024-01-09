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