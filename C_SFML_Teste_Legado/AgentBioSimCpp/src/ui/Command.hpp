#pragma once

// Phase 25 (Divida 8): Command / CommandQueue and all Cmd* structs now live in
// the neutral agentbiosim::core layer (core/Command.hpp) so the engine
// (sim::SimulationRunner) can consume commands without depending on the ui::
// layer — the dependency arrow now points only UI -> core, never engine -> ui.
//
// This shim re-exports the command types into agentbiosim::ui for transitional
// compatibility with the SFML UI and the Phase 22-24 diagnostics that still say
// ui::Command / ui::CmdXxx. The using-directive inside namespace ui makes both
// qualified (ui::CmdXxx, from the tests) and unqualified (within namespace ui)
// lookups resolve to the core types. This shim is removed at the end of Phase 25
// once the old SFML UI is deleted and the tests are migrated to core::.
#include "core/Command.hpp"

namespace agentbiosim::ui
{
using namespace core;
}
