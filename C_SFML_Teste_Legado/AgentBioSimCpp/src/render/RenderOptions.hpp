#pragma once

#include <SFML/Graphics/Color.hpp>

namespace agentbiosim::render
{
struct RenderOptions
{
    bool renderEnabled = true;
    bool simpleRender = true;
    float renderResolutionScale = 1.0F;

    sf::Color backgroundColor{10, 10, 20};
    bool backgroundGradientEnabled = false;
    sf::Color backgroundColorTop{10, 10, 20};
    sf::Color backgroundColorBottom{10, 10, 20};

    bool substrateGradientEnabled = false;
    sf::Color substrateColorTop{10, 10, 20};
    sf::Color substrateColorBottom{10, 10, 20};
    bool substrateBorderEnabled = true;
    sf::Color substrateBorderColor{40, 200, 40};

    sf::Color agentHeadColor{0, 0, 0};
    sf::Color chunkFoodOutlineColor{35, 25, 20};

    // Fase 32.1 (auditoria): overlay da grade do spatial hash (menu Exibir).
    // Antes o item de menu so alternava um flag sem desenhar nada.
    bool showSpatialHashOverlay = false;
    double spatialHashCellSize = 36.0;
};
} // namespace agentbiosim::render
