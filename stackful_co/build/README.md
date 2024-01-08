# 编译

在build目录下编译

```shell
cmake ..
make -j4
```

# demo

```c++
#include "coroutine.h"
#include "utils.h"

#include <iostream>

void where() {
    std::cout << "running code in a "
              << (stack_co::test() ? "coroutine" : "thread")
              << std::endl;
}

void print1() {
    std::cout << 1 << std::endl;
    stack_co::this_coroutine::yield();
    std::cout << 2 << std::endl;
}

void print2(int i, stack_co::Coroutine *co1) {
    std::cout << i << std::endl;
    co1->resume();
    where();
    std::cout << "bye" << std::endl;
}

int main() {
    auto &env = stack_co::open();
    auto co1 = env.create_coroutine(print1);
    auto co2 = env.create_coroutine(print2, 3, co1.get());
    co1->resume();
    co2->resume();
    where();
    return 0;
}

// 1
// 3
// 2
// running code in a coroutine
// bye
// running code in a thread
```

