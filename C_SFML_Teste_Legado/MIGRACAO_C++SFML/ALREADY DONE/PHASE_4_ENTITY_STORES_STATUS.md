# Fase 4 - Entity Stores Basicos

## Escopo Executado

A Fase 4 criou stores basicos de entidades em C++ para validar a base data-oriented/SoA sem implementar comportamento de simulacao. O aplicativo SFML agora instancia e desenha um smoke test visual estatico com organismos e comida usando os defaults cadastrados no `ParameterRegistry`.

## Arquivos Python Consultados

- `sim/entities.py`: fonte funcional para campos basicos de `Entity`, `Food`, `Agent`, `Bacteria`, `Predator`, factories de spawn e `type_code`.
- `sim/controllers.py`: fonte funcional para defaults de populacao, comida, energia inicial, tamanho corporal, cores e modo de comida.
- `sim/world.py`: fonte funcional para mundo retangular/circular, `is_inside`, `clamp_position`, `wrap_position`, `distance_to_wall` e camera.
- `sim/render.py`: fonte funcional para desenho simples de comida/agente, uso de cor e cabeca preta do agente.
- `sim/game.py`: fonte funcional para integracao visual de camera, zoom, pan e bootstrap da janela.

## Arquivos Python Adicionais Consultados

Nenhum arquivo Python adicional foi consultado nesta fase.

## Conceitos Confirmados Contra o Python

- `Entity` Python possui `x`, `y`, `r`, `color` e `type_code`.
- `Food` Python possui energia inicial baseada em `r * r`, `kind` (`instant`/`chunk`), cor, velocidade e metadados de chunk.
- `Agent` Python possui posicao, velocidade, angulo, raio, energia, idade, cor, flag de predador, labels e outros componentes.
- `Bacteria` e `Predator` ainda existem como classes legadas no Python, mas a arquitetura C++ deve caminhar para organismo/especie mais generico.
- Spawn Python respeita mundo retangular/circular e tenta evitar sobreposicoes.
- Render Python desenha comida e agentes como formas coloridas, com cabeca preta para indicar frente.
- O mundo circular usa centro em `width / 2`, `height / 2` e raio de substrato.

## Divergencias e Decisoes Registradas

- O C++ usa `AgentStore` e `FoodStore` SoA em vez de uma classe por entidade. Isso segue a arquitetura proposta e nao e uma traducao literal do Python.
- IDs C++ sao estaveis (`EntityId` monotonicamente crescente), mas indices internos dos arrays nao sao estaveis porque a remocao usa swap-remove.
- O smoke test visual da Fase 4 nao evita sobreposicoes entre entidades. O Python tenta evitar sobreposicoes no spawn, mas a implementacao completa depende de spatial hash/colisao em fases futuras.
- O parametro Python `random_seed = -1` normalmente indica seed nao fixa. Para este smoke test visual, o C++ usa fallback deterministico `1337` quando `random_seed` e `-1`, para tornar a validacao da fase reprodutivel ate existir `RandomService`.
- Campos frios e comportamentais do Python, como cerebro, sensor, locomocao, dieta, labels detalhadas, chunk id, bite holes e metricas de consumo, nao foram implementados nesta fase.
- `AgentTypeCode::LegacyPredator` existe apenas como codigo de tipo futuro/compatibilidade. Nao ha predador funcional.

## Arquivos Criados

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/EntityId.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/EntityTypes.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/FoodStore.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/FoodStore.cpp`

## Arquivos Modificados

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`

## Estruturas Implementadas

### `EntityId`

- Wrapper simples para ID numerico.
- `0` representa ID invalido.

### `EntityTypes`

- `SpeciesId`.
- `AgentTypeCode`: `Organism`, `LegacyBacteria`, `LegacyPredator`.
- `FoodKind`: `Instant`, `Chunk`.
- `ColorRgb`.
- `AgentSpawn`.
- `FoodSpawn`.

### `AgentStore`

Hot data em arrays separados:

- `id`
- `x`
- `y`
- `vx`
- `vy`
- `angle`
- `radius`
- `energy`
- `age`
- `color`
- `speciesId`
- `typeCode`
- `alive`

Operacoes implementadas:

- `createAgent`
- `removeAgent`
- `clear`
- `size`
- `empty`
- `contains`
- `indexOf`
- `getPosition`
- `setPosition`
- acesso por indice para renderizacao e validacao visual

### `FoodStore`

Hot data em arrays separados:

- `id`
- `x`
- `y`
- `radius`
- `energy`
- `initialEnergy`
- `color`
- `kind`
- `alive`

Operacoes implementadas:

- `createFood`
- `removeFood`
- `clear`
- `size`
- `empty`
- `contains`
- `indexOf`
- `getPosition`
- `setPosition`
- acesso por indice para renderizacao e validacao visual

## Parametros Usados

- `bacteria_count`
- `bacteria_body_size`
- `bacteria_color`
- `bacteria_initial_energy`
- `food_target`
- `food_min_r`
- `food_max_r`
- `food_color`
- `food_mode`
- `random_seed`
- parametros de mundo e tempo da Fase 3 continuam sendo usados.

## Parametros Pendentes

Nenhum parametro exigido pela Fase 4 ficou ausente do `ParameterRegistry`. Parametros ricos de especie, dieta, labels, chunks detalhados, metabolismo e rede neural continuam cadastrados ou documentados, mas nao sao usados por comportamento nesta fase.

## Decisao Sobre IDs e Remocao

- IDs sao estaveis para a vida da entidade.
- Remocao usa swap-remove para manter arrays compactos.
- Qualquer sistema futuro que guarde indices deve recalcular ou escutar remapeamento; sistemas persistentes devem guardar `EntityId`, nao indice.

## Limitacoes Atuais

- Nao ha visao.
- Nao ha rede neural.
- Nao ha alimentacao.
- Nao ha reproducao.
- Nao ha predadores funcionais.
- Nao ha metabolismo completo.
- Nao ha movimento real.
- Nao ha spatial hash.
- Nao ha colisao.
- Nao ha UI completa.
- Nao ha save/load.
- O render de entidades e apenas um smoke test visual.

## Confirmacao de Escopo

Esta fase implementa somente stores basicos de entidades, spawn visual estatico e renderizacao simples para verificar que os stores estao integrados ao esqueleto C++/SFML. Nenhuma regra biologica, neural ou evolutiva foi implementada.
