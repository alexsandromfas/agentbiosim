# Registro de Dividas Tecnicas

Este registro acompanha dividas tecnicas identificadas durante a migracao C++/SFML. Cada item deve indicar descricao, impacto, acao recomendada, momento sugerido, prioridade e se bloqueia a proxima fase.

Data de criacao: 2026-05-27 (pos-auditoria da Fase 9).

## Divida 1 — Helpers de parametros duplicados

Descricao:
Os helpers `parameterDouble`, `parameterInt`, `parameterBool` e `parameterString` aparecem copiados em pelo menos quatro arquivos:

- `src/app/App.cpp`
- `src/systems/MovementSystem.cpp`
- `src/systems/NeuralSystem.cpp`
- `src/neural/BrainFactory.cpp`

Cada copia e uma funcao anonima identica em namespace anonimo local. Novos sistemas tendem a adicionar mais copias.

Impacto:
- Aumenta manutencao: uma correcao deve ser replicada em todas as copias.
- Aumenta risco de comportamento inconsistente entre sistemas.
- Tende a piorar conforme novos sistemas forem adicionados (Fases 10 a 21).

Acao recomendada:
Extrair para um header utilitario como:

```
src/config/ParameterHelpers.hpp
```

Ou como metodos de conveniencia em `ParameterRegistry`.

Momento sugerido:
Antes da Fase 13 (reproducao e genoma) ou na primeira fase de cleanup tecnico.

Prioridade:
Media.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `App.cpp`, `MovementSystem.cpp`, `NeuralSystem.cpp`, `BrainFactory.cpp`.
- Futuros: `PerceptionSystem`, `ReproductionSystem`, `CollisionSystem` e qualquer sistema que leia parametros.

## Divida 2 — BrainSlot dependente de MLPBrain

Descricao:
Em `NeuralSystem.hpp`, o `BrainSlot` interno contem `std::unique_ptr<neural::MLPBrain>` como tipo concreto. A Fase 9 implementou somente MLP, entao isso e suficiente agora. Porem, quando Gated MLP, RNN ou NEAT forem implementados, o slot precisara suportar multiplos tipos de cerebro.

Impacto:
- Limita a coexistencia de multiplos tipos neurais no mesmo `NeuralSystem`.
- Exigira refatoracao interna do `NeuralSystem` antes de adicionar qualquer tipo alem de MLP.
- A interface publica (`produceMovementControls` retornando `std::vector<MovementControl>`) nao sera afetada — o impacto e interno.

Acao recomendada:
Trocar `unique_ptr<MLPBrain>` por `std::variant` de tipos neurais, ou usar polimorfismo com `IBrain` base, ou usar um union tagged com `BrainType`. A decisao final depende do modelo de batch/executor das redes avancadas.

Momento sugerido:
Antes ou no inicio da Fase 14 (redes densas avancadas).

Prioridade:
Alta futura.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/systems/NeuralSystem.hpp` (struct `BrainSlot`).
- `src/systems/NeuralSystem.cpp` (`syncBrains`, `produceMovementControls`).

## Divida 3 — Nomes herdados de Numba/Python no C++

Descricao:
Alguns campos em `BrainPerformanceConfig` (dentro de `BrainConfig.hpp`) usam nomes originarios do Python/Numba:

- `useNumbaBrainForward` — conceito Python; em C++ o equivalente e batch/optimized forward.
- `numbaBrainForwardMinBatch` — idem.

Esses campos foram preservados para manter paridade de parametros com o Python, mas o nome no C++ deve refletir o conceito C++, nao a implementacao Python.

Impacto:
- Confusao conceitual para quem le o codigo C++ sem conhecer o Python.
- Mistura de terminologia que faz a arquitetura parecer portada, nao redesenhada.
- Risco baixo a medio, pois sao campos de configuracao que ainda nao sao usados funcionalmente.

Acao recomendada:
Renomear para nomes genericos como `useBatchForward`, `batchForwardMinSize` ou equivalentes. Manter aliases no `ParameterRegistry` para os nomes Python antigos.

Momento sugerido:
Antes da implementacao de batch neural real (Fase 14 ou Fase 30).

Prioridade:
Baixa a media.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/neural/BrainConfig.hpp` (struct `BrainPerformanceConfig`).
- `src/config/ParameterDefaults.cpp` (registro dos parametros).

## Divida 4 — App acumulando responsabilidades

Descricao:
A classe `App` atualmente orquestra:

- criacao e gerenciamento da janela SFML;
- processamento de eventos de input;
- gerenciamento da camera;
- spawn de entidades demo;
- configuracao de parametros;
- execucao do simulation step;
- chamada de todos os sistemas (Neural, Movement, Energy, Interaction, Death);
- rebuild do spatial hash;
- renderizacao;
- atualizacao do titulo da janela com estatisticas.

Isso e aceitavel para as fases iniciais, mas tende a crescer quando UI (Dear ImGui), input avancado, selecao de agentes e ferramentas de canvas forem adicionados.

Impacto:
- Dificuldade de manutenção quando App crescer.
- Mistura de orquestracao, runtime e interface.
- Pode dificultar o modo headless puro se o App estiver acoplado a logica de janela.

Acao recomendada:
Fatorar em componentes como:

- `SimulationRunner`: executa steps, gerencia sistemas e stores.
- `AppController`: liga janela, UI, input e renderer ao runner.
- `InputRouter`: traduz eventos SFML em comandos para o engine.

Momento sugerido:
Antes ou durante a Fase 22 (UI base).

Prioridade:
Media.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/app/App.hpp`
- `src/app/App.cpp`

## Divida 5 — Alocacoes temporarias no NeuralSystem

Descricao:
O metodo `NeuralSystem::produceMovementControls` aloca um `std::vector<double>` de input e recebe um `std::vector<double>` de output para cada agente, a cada step. Com 1000 agentes a 30 steps/s, sao ~60.000 alocacoes/dealocacoes por segundo de vetores pequenos.

O benchmark da Fase 9 mostra ~2.25 us/forward para a arquitetura padrao (4 -> 20x4 -> 2), o que e viavel para tempo real com 1000 agentes. Mas conforme sensores reais (Fase 10+) aumentem o `input_size` e mais agentes sejam suportados, esse padrao se tornara um gargalo mensuravel.

Impacto:
- Custo de alocacao no hot loop.
- Pode limitar escala acima de 2000 agentes.
- Sera mais relevante quando `input_size` crescer com sensores/visao.

Acao recomendada:
- Usar buffers persistentes pre-alocados por assinatura neural.
- Implementar batch forward para redes densas (MLP, Gated, Shortcut, Modulated, RNN).
- Manter fallback individual para NEAT.

Momento sugerido:
Fase 30 (otimizacao data-oriented) ou antes, se benchmarks mostrarem gargalo real na Fase 12/14.

Prioridade:
Alta futura.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/systems/NeuralSystem.cpp` (`produceMovementControls`, `temporaryInputForAgent`).
- Futuro: `BrainExecutor` quando batch for implementado.

## Divida 6 — Version.hpp desatualizado

Descricao:
O arquivo `src/core/Version.hpp` define a versao do executavel. Atualmente pode ainda indicar `v0.1.0-phase1`, apesar do projeto estar apos a Fase 9.

Impacto:
- Confusao em logs, diagnosticos e titulo do executavel.
- Impacto funcional nulo.

Acao recomendada:
Atualizar para `v0.9.0-phase9` ou equivalente. Considerar se o versionamento da migracao deve seguir um esquema formal.

Momento sugerido:
Qualquer fase curta de housekeeping, ou junto com a proxima fase que altere `App`.

Prioridade:
Baixa.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/core/Version.hpp`

## Resumo por fase futura impactada

| Divida | Fase recomendada para resolver | Prioridade |
|---|---|---|
| 1. Helpers duplicados | Antes da Fase 13 | Media |
| 2. BrainSlot/MLPBrain concreto | Antes da Fase 14 | Alta futura |
| 3. Nomes Numba/Python | Antes da Fase 14 ou 30 | Baixa/media |
| 4. App acumulando responsabilidades | Antes da Fase 22 | Media |
| 5. Alocacoes temporarias neural | Fase 30 ou antes se gargalo medido | Alta futura |
| 6. Version.hpp | Qualquer housekeeping | Baixa |

## Regra

Nenhuma divida registrada aqui bloqueia a Fase 10 (sensores e visao single).

Dividas devem ser revisadas antes de iniciar a fase indicada como momento sugerido. Se uma divida for resolvida, registrar a fase e commit que a resolveu neste documento.
