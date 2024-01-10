# 编译

在build目录下编译

```shell
cmake ..
make -j4
```

# demo

```c++
#include "schedule.h"

#include <cstdio>

struct args {
    int n;
};

void foo(stackless_co::Schedule *s, void *ud) {
    args *arg = static_cast<args *>(ud);
    int start = arg->n;
    for (int i = 0; i < 5; i++) {
        printf("coroutine %d : %d\n", s->coroutine_running(), start + i);
        s->coroutine_yield();
    }
}

void test(stackless_co::Schedule *s) {
    struct args arg1 = {0};
    struct args arg2 = {100};

    int co1 = s->coroutine_new(foo, &arg1);
    int co2 = s->coroutine_new(foo, &arg2);
    printf("main start\n");

    while (s->coroutine_status(co1) && s->coroutine_status(co2)) {
        s->coroutine_resume(co1);
        s->coroutine_resume(co2);
    }
    printf("main end\n");
}

int main() {
    auto *s = stackless_co::Schedule::coroutine_open();
    test(s);
    s->coroutine_close();

    return 0;
}

// 上面的代码首先利用 coroutine_open 创建了协程调度器 s，用来统一管理全部的协程。
// 同时，在 test 函数中，创建了两个协程 co1 和 co2，不断的反复 yield 和 resume 协程，直至两个协程执行完毕。

//执行后输出：
// main start
// coroutine 0 : 0
// coroutine 1 : 100
// coroutine 0 : 1
// coroutine 1 : 101
// coroutine 0 : 2
// coroutine 1 : 102
// coroutine 0 : 3
// coroutine 1 : 103
// coroutine 0 : 4
// coroutine 1 : 104
// main end
```

