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

add_executable(AgentBioSimCpp
    src/main.cpp
    src/app/App.cpp
    src/config/ParameterDefaults.cpp
    src/config/ParameterMetadata.cpp
    src/config/ParameterRegistry.cpp
    src/neural/BrainFactory.cpp
    src/neural/GatedMLPBrain.cpp
    src/neural/MLPBrain.cpp
    src/neural/ModulatedMLPBrain.cpp
    src/neural/NEATGraphBrain.cpp
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
    src/ui/InputRouter.cpp
    src/ui/UiPanel.cpp
    src/ui/UiPreferencesPanel.cpp
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
        "${SFML_ROOT}/lib/sfml-graphics.lib"
        "${SFML_ROOT}/lib/sfml-window.lib"
        "${SFML_ROOT}/lib/sfml-system.lib"
)

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
