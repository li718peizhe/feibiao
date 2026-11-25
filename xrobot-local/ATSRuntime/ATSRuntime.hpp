#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: Board service initialization adapter for the ATS firmware
depends: []
=== END MANIFEST === */
// clang-format on

#include "libxr.hpp"

extern "C" void ATS_Runtime_Init(void);

class ATSRuntime
{
 public:
  ATSRuntime() { ATS_Runtime_Init(); }

  void OnMonitor() {}
};
