#pragma once

// Phase 25 (Divida 8): CanvasTool now lives in the neutral agentbiosim::core
// layer (core/CanvasTool.hpp) so the engine can use it without depending on ui::.
// This shim re-exports it into agentbiosim::ui for transitional compatibility
// with the SFML UI/tests that still reference ui::CanvasTool. The using-directive
// makes both qualified (ui::CanvasTool) and unqualified (within namespace ui)
// lookups resolve to the core type. This shim is removed at the end of Phase 25
// once the old SFML UI is deleted and the tests are migrated to core::.
#include "core/CanvasTool.hpp"

namespace agentbiosim::ui
{
using namespace core;
}
