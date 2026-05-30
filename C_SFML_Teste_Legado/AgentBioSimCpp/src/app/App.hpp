#pragma once

#include "config/ParameterRegistry.hpp"
#include "render/Camera2D.hpp"
#include "render/Renderer.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FixedTimestep.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/DeathSystem.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/FoodSystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "perception/PerceptionSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/ReproductionSystem.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>

namespace agentbiosim
{
class App
{
public:
    App();

    int run();

private:
    void processEvents();
    void handleResize(unsigned int width, unsigned int height);
    void configureFromParameters();
    void fitCameraToWorld();
    void configureRenderOptions();
    void spawnDemoEntities();
    void seedDemoFoodContact();
    [[nodiscard]] double computeSpatialCellSize() const;
    void rebuildSpatialHash();
    void runSimulationStep(double dt);
    void update();
    void render();
    void updateFpsTitle();

    config::ParameterRegistry parameters_;
    simulation::World world_;
    simulation::FixedTimestep timestep_;
    simulation::AgentStore agents_;
    simulation::FoodStore foods_;
    simulation::GenomeStore genomes_;
    simulation::SpeciesStore species_;
    simulation::ObstacleStore obstacles_;
    simulation::SpatialHash spatialHash_;
    perception::PerceptionSystem perceptionSystem_;
    systems::MovementSystem movementSystem_;
    systems::NeuralSystem neuralSystem_;
    systems::EnergySystem energySystem_;
    systems::InteractionSystem interactionSystem_;
    systems::ReproductionSystem reproductionSystem_;
    systems::DeathSystem deathSystem_;
    systems::FoodSystem foodSystem_;
    systems::FoodSystemStats lastFoodStats_{};
    render::Camera2D camera_;
    render::Renderer renderer_;
    render::RenderOptions renderOptions_;
    render::RenderStats lastRenderStats_;
    systems::MovementStats lastMovementStats_{};
    perception::PerceptionStats lastPerceptionStats_{};
    systems::NeuralStats lastNeuralStats_{};
    systems::EnergyStats lastEnergyStats_{};
    systems::InteractionStats lastInteractionStats_{};
    systems::ReproductionStats lastReproductionStats_{};
    systems::DeathStats lastDeathStats_{};
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    sf::Clock fpsClock_;
    sf::Vector2i lastMousePosition_{0, 0};
    bool isPanning_ = false;
    bool spatialEnabled_ = true;
    bool reuseSpatialGrid_ = true;
    unsigned int frames_ = 0;
    unsigned long long simulatedSteps_ = 0;
    unsigned long long spatialHashRebuilds_ = 0;
    std::uint64_t foodEatenCount_ = 0;
    std::uint64_t deathsCount_ = 0;
    unsigned int lastStepsThisFrame_ = 0;
    float lastFps_ = 0.0F;
    double spatialCellSize_ = 36.0;
    simulation::SpatialHashStats lastSpatialStats_{};
    bool visionDebugEnabled_ = false;
    perception::VisionDebugData visionDebug_{};
};
} // namespace agentbiosim
