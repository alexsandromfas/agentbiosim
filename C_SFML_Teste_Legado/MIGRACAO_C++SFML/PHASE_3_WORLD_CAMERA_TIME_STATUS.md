# Fase 3 - Status de Mundo, Camera e Tempo Fixo

## Objetivo

Implementar a base minima de mundo, camera e timestep fixo na versao C++/SFML, sem criar entidades reais e sem iniciar sistemas de simulacao biologica.

## Arquivos Criados

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/World.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/World.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/FixedTimestep.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/FixedTimestep.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/render/Camera2D.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/render/Camera2D.cpp`

## Arquivos Modificados

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`

## Funcionalidades Implementadas

### Mundo

- Mundo retangular.
- Mundo circular.
- Largura, altura, raio e centro.
- `isInside`.
- `clampPosition`.
- `wrapPosition` para mundo retangular; no circular, volta para clamp.
- `distanceToWall`.
- Bounds min/max para camera e render.

### Camera

- `worldToScreen`.
- `screenToWorld`.
- Pan por delta de tela.
- Zoom.
- `zoomAt` preservando ponto sob o cursor.
- `fitWorld` basico.
- Limites minimo e maximo de zoom.

### Tempo Fixo

- `dt` fixo baseado em `physics_steps_per_second`.
- Acumulador de tempo.
- `max_physics_steps_per_frame`.
- `max_physics_backlog_seconds`.
- Pause.
- `time_scale`.
- `interpolationAlpha` preparado para uso futuro.

### Integracao Minima no App

- Janela SFML continua abrindo.
- Limite do mundo e desenhado.
- Mouse wheel aplica zoom.
- Botao direito ou meio do mouse aplica pan.
- Tecla `F` ajusta a camera ao mundo.
- Tecla `Space` pausa/despausa o timestep.
- Titulo mostra FPS, formato do mundo, zoom, dt e steps acumulados.
- Nao existem entidades ou sistemas biologicos.

## Parametros Usados

Lidos do `ParameterRegistry` da Fase 2:

- `substrate_shape`
- `world_w`
- `world_h`
- `substrate_radius`
- `physics_steps_per_second`
- `max_physics_steps_per_frame`
- `max_physics_backlog_seconds`
- `time_scale`
- `paused`

## Verificacao Contra Referencia Python

Arquivos Python consultados:

- `sim/world.py`
- `sim/engine.py`
- `sim/game.py`
- `sim/render.py`
- `sim/controllers.py`

Conceitos confirmados e alinhados:

- mundo retangular;
- mundo circular;
- largura e altura;
- raio do substrato circular;
- centro do mundo no centro do retangulo base;
- `isInside`/containment com raio opcional de objeto;
- `clampPosition` com raio opcional de objeto;
- `wrapPosition` retangular e clamp no mundo circular;
- `distanceToWall`;
- camera com pan;
- camera com zoom focalizado no mouse;
- conversoes mundo/tela e tela/mundo;
- timestep fixo por `physics_steps_per_second`;
- `time_scale`;
- `paused`;
- `max_physics_steps_per_frame`;
- `max_physics_backlog_seconds`;
- descarte de backlog excedente quando o numero de substeps disponiveis passa do limite por frame.

Correcoes feitas apos a verificacao:

- `World` agora aplica raio minimo circular `10.0`, como no Python.
- `World::isInside` e `World::clampPosition` agora aceitam raio opcional do objeto.
- `substrate_radius` no `ParameterRegistry` agora usa range minimo `10.0`.
- `Camera2D` agora usa zoom minimo `0.01`, como no Python.
- Zoom por scroll no `App` agora usa fator `1.1`, como no Python.
- `FixedTimestep` agora garante backlog minimo de um `physics_dt`, usa epsilon numerico pequeno e descarta backlog excedente quando `available_steps > max_steps_per_frame`, como no Python.

Divergencia registrada:

- `Camera2D::fitWorld` enquadra os bounds reais do circulo quando o substrato e circular. O Python atual usa `world.width/world.height` no `Camera.fit_world`, mesmo para mundo circular. A decisao C++ foi manter o enquadramento do circulo real porque ele evita cortar bordas quando `substrate_radius` excede metade da altura/largura base. Se paridade visual estrita for exigida, isso deve virar uma opcao ou ser ajustado antes da Fase 5/render completo.

## Limitacoes Atuais

- Nao ha engine de simulacao.
- Nao ha agentes.
- Nao ha comida.
- Nao ha visao.
- Nao ha redes neurais.
- Nao ha reproducao.
- Nao ha predadores.
- Nao ha UI tecnica completa.
- Nao ha arquivo de configuracao externo; os valores ainda vem dos defaults do `ParameterRegistry`.
- O timestep apenas conta steps disponiveis; nao executa sistemas ainda.

## Confirmacao de Escopo

Esta fase implementa somente mundo, camera e tempo fixo. A Fase 4 nao foi iniciada.
