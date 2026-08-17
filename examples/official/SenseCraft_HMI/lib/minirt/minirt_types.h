#pragma once

#include <cstddef>

namespace MiniRT 
{
    enum class Priority {
        Low,
        High
    };

    struct Task {
        void (*callback)(){nullptr};
        unsigned long interval{0};
        unsigned long last_run_ms{0};
        bool enabled{false};
    };

}
