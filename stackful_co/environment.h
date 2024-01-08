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
