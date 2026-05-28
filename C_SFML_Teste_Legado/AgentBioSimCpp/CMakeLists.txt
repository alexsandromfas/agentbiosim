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
    src/config/ParameterRegistry.cpp
    src/neural/BrainFactory.cpp
    src/neural/MLPBrain.cpp
    src/neural/Phase9Diagnostics.cpp
    src/perception/PerceptionSystem.cpp
    src/perception/Phase10Diagnostics.cpp
    src/perception/SceneQuery.cpp
    src/render/Camera2D.cpp
    src/render/Renderer.cpp
    src/simulation/AgentStore.cpp
    src/simulation/FixedTimestep.cpp
    src/simulation/FoodStore.cpp
    src/simulation/SpatialHash.cpp
    src/simulation/World.cpp
    src/systems/DeathSystem.cpp
    src/systems/EnergySystem.cpp
    src/systems/InteractionSystem.cpp
    src/systems/MovementSystem.cpp
    src/systems/NeuralSystem.cpp
    src/systems/Phase7Diagnostics.cpp
    src/systems/Phase8Diagnostics.cpp
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
