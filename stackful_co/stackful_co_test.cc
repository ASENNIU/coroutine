//
// Created by dell on 2024/1/8.
//

#include "coroutine.h"
#include "environment.h"
#include "utils.h"

#include <iostream>

void where() {
    std::cout << "running code in a "
        << (stackful_co::test() ? "corountine" : "thread")
        << std::endl;
}

void print1() {
    std::cout << 1 << std::endl;
    stackful_co::this_coroutine::yield();
    std::cout << 2 << std::endl;
}

void print2(int i, stackful_co::Coroutine *co) {
    std::cout << i << std::endl;
    co->resume();
    where();
    std::cout << "bye" << std::endl;
}

int main()
{
    auto &env = stackful_co::open();
    auto co1 = env.create_coroutine(print1);
    auto co2 = env.create_coroutine(print2, 3, co1.get());
    co1->resume();
    co2->resume();
    where();
    return 0;
}