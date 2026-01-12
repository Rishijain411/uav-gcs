#pragma once

enum class EngagementDecision {
    HOLD,               // Keep tracking
    REQUEST_CONFIRM,    // Ask operator
    ENGAGE,             // Begin intercept
    REENGAGE,           // Retry after miss
    ABORT               // Unsafe / illegal
};
