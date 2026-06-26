#pragma once

#include "perception/VisionDebug.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>

namespace agentbiosim::app
{
// Fase 35 (Estágio 1): cópia imutável do estado de MUNDO que o render desenha,
// desacoplada dos stores vivos do SimulationRunner.
//
// Por que existir: no Estágio 2 a simulação roda numa thread separada (worker) que
// MUTA os stores enquanto a main thread desenha. Para nunca ler um store em mutação,
// a main captura este snapshot ENQUANTO o worker está ocioso (barreira por frame) e o
// desenho do mundo passa a ler daqui — não dos stores vivos.
//
// Os stores são SoA (apenas std::vector + unordered_map), logo copiáveis por valor. A
// cópia é barata perto de um passo (~0,7 MB / ~0,1 ms a 5000 agentes vs ~23 ms/passo) e
// mantém a assinatura do Renderer intacta (ele continua recebendo const AgentStore&,
// agora a do snapshot).
//
// Apenas o que o passo de simulação MUTA precisa ser copiado (agents/foods/visionDebug);
// world e obstacles são copiados também por clareza (o render lê tudo de mundo daqui, sem
// ambiguidade vivo-vs-snapshot) — o passo não os mexe, então a cópia é trivial.
struct RenderSnapshot
{
    simulation::World world{};
    simulation::AgentStore agents{};
    simulation::FoodStore foods{};
    simulation::ObstacleStore obstacles{};
    perception::VisionDebugData vision{};

    bool visionActive = false;        // visionDebug().active no instante da captura
    bool spatialOverlay = false;      // desenhar o grid do spatial hash (menu Exibir)
    double spatialCellSize = 0.0;     // tamanho de célula p/ o grid

    // Contadores p/ a barra de info da UI (lidos na fase com worker ocioso).
    std::size_t agentCount = 0;
    std::size_t foodCount = 0;
    std::size_t obstacleCount = 0;
};
} // namespace agentbiosim::app
