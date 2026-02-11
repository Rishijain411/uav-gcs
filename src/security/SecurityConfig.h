#pragma once

// =====================================================
// Phase 5.3 — Security Configuration
// =====================================================
//
// 0 → PLAINTEXT mode (Dev / No Jetson present)
// 1 → ENCRYPTED mode (Jetson present, PRD compliant)
//
// Flip this to 1 when Jetson hardware is available.
// -----------------------------------------------------

#ifndef SECURE_CHANNEL_ENABLED
#define SECURE_CHANNEL_ENABLED 0
#endif
