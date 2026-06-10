cmake_minimum_required(VERSION 3.20)

project(AgentBioSimCpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT SFML_ROOT)
    set(_LOCAL_SFML_ROOT "C:/Users/Alex Martins/Desktop/Meus Projetos/simulacoes_biologicas/AntSimulator-master/AntSimulator-master/third_party/SFML-2.6.2")
    if(EXISTS "${_LOCAL_SFML_ROOT}")
        set(SFML_ROOT "${_LOCAL_SFML_ROOT}" CACHE PATH "SFML 2.6.2 root directory")
    endif()
endif()

if(NOT EXISTS "${SFML_ROOT}/include/SFML/Graphics.hpp")
    message(FATAL_ERROR "SFML_ROOT invalido. Configure com -DSFML_ROOT=C:/caminho/para/SFML-2.6.2")
endif()

# Phase 25: Dear ImGui + ImGui-SFML (vendored in third_party/). Built as a
# separate static library with relaxed warnings so the project's /W4 /permissive-
# does not flag third-party code. The headless engine never links this; only the
# app/UI layer does. imconfig-SFML.h (via IMGUI_USER_CONFIG) adds the
# sf::Vector2<->ImVec2 and sf::Color<->ImVec4 conversions and must be visible to
# every TU that includes imgui.h, so it is set PUBLIC.
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(IMGUI_SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui-sfml")

add_library(imgui_sfml STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${IMGUI_SFML_DIR}/imgui-SFML.cpp"
)

target_include_directories(imgui_sfml PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/misc/cpp"
    "${IMGUI_SFML_DIR}"
    "${SFML_ROOT}/include"
)

target_compile_definitions(imgui_sfml PUBLIC IMGUI_USER_CONFIG="imconfig-SFML.h")

target_link_libraries(imgui_sfml PUBLIC
    "${SFML_ROOT}/lib/sfml-graphics.lib"
    "${SFML_ROOT}/lib/sfml-window.lib"
    "${SFML_ROOT}/lib/sfml-system.lib"
    opengl32
)

if(MSVC)
    target_compile_options(imgui_sfml PRIVATE /W0)
endif()

add_executable(AgentBioSimCpp
    src/main.cpp
    src/app/App.cpp
    src/bench/Benchmark.cpp
    src/config/ParameterDefaults.cpp
    src/config/ParameterMetadata.cpp
    src/config/ParameterRegistry.cpp
    src/core/AssetPath.cpp
    src/core/Logger.cpp
    src/core/Profiler.cpp
    src/i18n/Locale.cpp
    src/io/Json.cpp
    src/io/SaveFile.cpp
    src/io/FileDialog.cpp
    src/neural/BrainFactory.cpp
    src/neural/GatedMLPBrain.cpp
    src/neural/MLPBrain.cpp
    src/neural/ModulatedMLPBrain.cpp
    src/neural/NEATGraphBrain.cpp
    src/neural/NeuralView.cpp
    src/neural/BrainSerializer.cpp
    src/neural/Phase9Diagnostics.cpp
    src/neural/Phase14Diagnostics.cpp
    src/neural/Phase15Diagnostics.cpp
    src/neural/Phase16Diagnostics.cpp
    src/neural/ShortcutMLPBrain.cpp
    src/neural/SimpleRNNBrain.cpp
    src/perception/PerceptionSystem.cpp
    src/perception/Phase10Diagnostics.cpp
    src/perception/Phase11Diagnostics.cpp
    src/perception/Phase12Diagnostics.cpp
    src/perception/SceneQuery.cpp
    src/render/Camera2D.cpp
    src/render/Renderer.cpp
    src/simulation/AgentStore.cpp
    src/simulation/FixedTimestep.cpp
    src/simulation/FoodStore.cpp
    src/simulation/GenomeStore.cpp
    src/simulation/ObstacleStore.cpp
    src/simulation/Phase17Diagnostics.cpp
    src/simulation/SpatialHash.cpp
    src/simulation/SpeciesBootstrap.cpp
    src/simulation/SpeciesStore.cpp
    src/simulation/World.cpp
    src/sim/SimulationRunner.cpp
    src/systems/CollisionSystem.cpp
    src/systems/FoodSystem.cpp
    src/systems/Phase18Diagnostics.cpp
    src/systems/Phase19Diagnostics.cpp
    src/systems/Phase20Diagnostics.cpp
    src/systems/Phase21Diagnostics.cpp
    src/systems/Phase22Diagnostics.cpp
    src/systems/Phase22_1Diagnostics.cpp
    src/systems/Phase23Diagnostics.cpp
    src/systems/Phase23_1Diagnostics.cpp
    src/systems/Phase23_2Diagnostics.cpp
    src/systems/Phase24Diagnostics.cpp
    src/systems/Phase25Diagnostics.cpp
    src/systems/Phase26Diagnostics.cpp
    src/systems/Phase27Diagnostics.cpp
    src/systems/Phase28Diagnostics.cpp
    src/systems/Phase30Diagnostics.cpp
    src/systems/MetricsSystem.cpp
    src/ui/DevWindow.cpp
    src/ui/InputRouter.cpp
    src/ui/UiPanel.cpp
    src/ui/UiPreferencesPanel.cpp
    src/ui/UiLeftDock.cpp
    src/ui/ImGuiTheme.cpp
    src/ui/ImGuiUi.cpp
    src/systems/DeathSystem.cpp
    src/systems/EnergySystem.cpp
    src/systems/InteractionSystem.cpp
    src/systems/MovementSystem.cpp
    src/systems/NeuralSystem.cpp
    src/systems/Phase7Diagnostics.cpp
    src/systems/Phase8Diagnostics.cpp
    src/systems/Phase13Diagnostics.cpp
    src/systems/ReproductionSystem.cpp
)

target_include_directories(AgentBioSimCpp
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${SFML_ROOT}/include"
)

target_link_libraries(AgentBioSimCpp
    PRIVATE
        imgui_sfml
        "${SFML_ROOT}/lib/sfml-graphics.lib"
        "${SFML_ROOT}/lib/sfml-window.lib"
        "${SFML_ROOT}/lib/sfml-system.lib"
)

if(WIN32)
    # Phase 28: native Save/Open dialogs (GetSaveFileNameA / GetOpenFileNameA).
    target_link_libraries(AgentBioSimCpp PRIVATE comdlg32)
endif()

# Phase 29: embed the git commit (configure-time) for benchmark report metadata.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE AGENTBIOSIM_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT AGENTBIOSIM_GIT_COMMIT)
    set(AGENTBIOSIM_GIT_COMMIT "unknown")
endif()
target_compile_definitions(AgentBioSimCpp PRIVATE AGENTBIOSIM_GIT_COMMIT="${AGENTBIOSIM_GIT_COMMIT}")

if(MSVC)
    target_compile_options(AgentBioSimCpp PRIVATE /W4 /permissive-)
else()
    target_compile_options(AgentBioSimCpp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    set(_SFML_BIN_DIR "${SFML_ROOT}/bin")
    foreach(_dll sfml-graphics-2.dll sfml-window-2.dll sfml-system-2.dll)
        add_custom_command(TARGET AgentBioSimCpp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_SFML_BIN_DIR}/${_dll}"
                "$<TARGET_FILE_DIR:AgentBioSimCpp>/${_dll}"
        )
    endforeach()
endif()

# Phase 25.3: bundle runtime assets (toolbar icons, etc.) next to the executable
# so they resolve via core::executableDir() regardless of the working directory.
add_custom_command(TARGET AgentBioSimCpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:AgentBioSimCpp>/Assets"
)
