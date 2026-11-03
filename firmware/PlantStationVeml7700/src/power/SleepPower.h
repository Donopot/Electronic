#pragma once

namespace plant {
class SleepPower {
public:
    static void restore();
    static void prepare(bool radioAsleep = false);
    static void enter();
};
}
