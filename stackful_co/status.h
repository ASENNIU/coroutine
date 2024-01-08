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
