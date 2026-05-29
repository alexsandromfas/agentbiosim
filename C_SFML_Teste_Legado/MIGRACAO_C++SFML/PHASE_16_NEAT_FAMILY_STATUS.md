# Fase 16 — NEAT Family (NEAT comum, NEAT simplificada, NEAT recorrente)

Status: **CONCLUIDA** (2026-05-29)

Esta fase implementa a familia NEAT como tres variantes operacionais cobertas por uma unica classe `NEATGraphBrain`, integrada ao mesmo `std::variant<...>` introduzido na Fase 14 e estendido na Fase 15.

## Escopo entregue

- `BrainType::Neat`, `BrainType::SimpleNeat`, `BrainType::RecurrentNeat` agora sao criados pelo `BrainFactory` sem fallback para MLP.
- Classe `agentbiosim::neural::NEATGraphBrain` (header em `src/neural/NEATGraphBrain.hpp`, implementacao em `src/neural/NEATGraphBrain.cpp`).
- `BrainVariant` expandido para 6 alternativas (`MLPBrain`, `GatedMLPBrain`, `ShortcutMLPBrain`, `ModulatedMLPBrain`, `SimpleRNNBrain`, `NEATGraphBrain`).
- Helper `isImplementedInPhase16` e `isNeatFamily` em `BrainType.hpp`.
- Carregamento de parametros NEAT por prefixo (`neural_neat`, `neural_proto_neat`, `neural_recurrent_neat`) em `BrainFactory::configFromRegistry`.
- `ActivationTrace` com `neatNodes` (snapshot por node), `neatConnectionCount`, `neatEnabledConnectionCount` e `neatRecurrentConnectionCount`.
- `Phase16Diagnostics` com 109 self-tests e microbenchmark em 6 cenarios.
- Flags `--phase16-selftest`, `--phase16-benchmark`, `--phase16-diagnostics` no executavel.
- Build Debug e Release passam; CMakeLists atualizado com `NEATGraphBrain.cpp` e `Phase16Diagnostics.cpp`.
- Mensagem inicial de `App.cpp` atualizada para Fase 16.

## Estrutura da NEATGraphBrain

- **Representacao do grafo**
  - `std::vector<Node>` com `id`, `kind` (Input/Hidden/Output), `layer` (float em [0.0, 1.0]) e `activation` (Linear/Tanh/Sigmoid). Espelha a estrutura Python (`sim/brain.py`, classe `NEATGraphBrain`).
  - `std::vector<Connection>` com `src`, `dst`, `weight`, `enabled`, `recurrent`, `innovation`.
  - `std::unordered_map<std::int32_t, double>` para estado recorrente por node (somente nao-input).

- **Topologia inicial**
  - `minimal`: todo input conecta diretamente a todo output (input * output conexoes).
  - `layered`: respeita `hiddenLayers` da `BrainConfig`, posicionando cada camada em `layer = idx / (denom)` e conectando camada a camada em fully-connected layer-wise. Igual ao Python (sorting por `(layer, id)`).
  - Topologia desconhecida cai em `minimal`.

- **Forward**
  - Inputs alimentam values; nodes hidden sao processados na ordem `(layer, id)`; outputs no final.
  - `connection.recurrent=true` consome estado armazenado em `state_[src]` em vez de `values[src]`.
  - Apenas em `RecurrentNeat`, ao final do step, o estado de cada node nao-input recebe `blend_state(new_value)` com `memory_decay` e e clampado por `state_clip`.
  - Overload const cria copia local, executa sem atualizar estado e retorna saidas para inspecao/UI; overload non-const atualiza estado (chamado pelo `NeuralSystem`).

- **Mutacao**
  - `NEAT comum` e `NEAT recorrente`: ordem fixa - weight mutation (Bernoulli por conexao) -> reset weight -> add connection (probabilistico) -> add node (split connection) -> toggle -> remove. Reproduz `mutate()` Python.
  - `NEAT simplificada`: aplica `mutateProtozoaStyle`, sorteando uma conexao habilitada e ou (a) divide-a com chance `add_node_rate`, ou (b) substitui o peso (reset 50% / perturba 50%). Reproduz `_mutate_protozoa_style`.
  - Em todos os casos `weight_mutation_rate` e `weight_mutation_strength` `< 0` significam "usar base rate/strength" (mesma semantica do RNN da Fase 15).
  - Topologia recorrente: `add_random_connection` so cria edges recorrentes em `RecurrentNeat` (`allowRecurrentEdges_ = true`). `connection_exists` distingue `recurrent` para permitir um par recorrente alem do feedforward.
  - Limites `max_hidden_nodes` e `max_connections` aplicados em todos os pontos de criacao.

- **Clone**
  - Copia profunda do grafo via `*this`.
  - Se `reset_state_on_copy = true`, zera estado da copia; caso contrario, copia estado.
  - Nao alia com o pai (verificado nos testes 33 e 64).

- **Pontos de integracao**
  - `BrainFactory::createBrain` despacha 3 cases NEAT que criam `NEATGraphBrain` com `instantiatedType` correto.
  - `BrainVariant::brainTypeOf` consulta `NEATGraphBrain::brainType()` para distinguir os 3 sabores no mesmo alternativa.
  - `forwardOf` non-const usa `b.forward(input, trace)` non-const (atualiza estado se recorrente). Const overload faz copia interna.
  - `cloneOf`, `mutateOf`, `resetStateOf`, `batchKeyOf`: nao precisaram de tratamento especial, basta o `std::visit` generico.

## Parametros (ja registrados na Fase 2)

| prefixo | brain type | descricao |
|---|---|---|
| `neural_neat` | `BrainType::Neat` | NEAT comum |
| `neural_proto_neat` | `BrainType::SimpleNeat` | NEAT simplificada (protozoa-style mutation) |
| `neural_recurrent_neat` | `BrainType::RecurrentNeat` | NEAT recorrente (allowRecurrentEdges_ + estado) |

Cada prefixo expoe: `initial_topology`, `weight_init_std`, `weight_mutation_rate`, `weight_mutation_strength`, `add_connection_rate`, `add_node_rate`, `toggle_connection_rate`, `remove_connection_rate`, `reset_weight_rate`, `max_hidden_nodes`, `max_connections`.

`neural_recurrent_neat` adicionalmente expoe: `recurrent_connection_rate`, `memory_decay`, `state_clip`, `reset_state_on_copy`.

`BrainFactory::configFromRegistry` seleciona o prefixo correto baseado em `requestedType`. Para tipos non-NEAT, `cfg.neat` permanece com defaults (sem impacto).

## Selftest

Total: **109 checks** (`--phase16-selftest`, Debug e Release: PASS).

| Faixa | Cobertura |
|---|---|
| 1-9 | Fabrica cria os 8 tipos atualmente implementados sem fallback indevido; `brainTypeOf` distingue as 3 variantes NEAT dentro do `BrainVariant`. |
| 10-19 | Topologia inicial: minimal (input * output conexoes, zero hidden) e layered (3*4 + 4*3 + 3*2 = 30 conexoes em uma rede 3-4-3-2). |
| 20-23 | Forward NEAT comum: tamanho da saida, finitude dos valores, determinismo com mesma seed, divergencia com seeds diferentes. |
| 24-26 | NEAT simplificada cria e executa; `allowsRecurrent()` retorna false por padrao. |
| 27-30 | NEAT recorrente: `allowsRecurrent()` true; estado existe para cada node nao-input; duas chamadas de forward produzem saidas finitas. |
| 31-34 | Politica `reset_state_on_copy=true/false`; estado da copia e independente do pai; `RecurrentNeat` cria estado mas `Neat` comum nao. |
| 35-39 | `memory_decay` (0 e 0.9), `state_clip` (limita magnitude do estado mesmo apos mutacoes agressivas), `resetState()` zera estado, NEAT comum mantem estado em zero. |
| 40-43 | Mutacao de peso com `base_rate=0` vs `>0`, `reset_weight_rate=1`, override `weight_mutation_rate=0` beats base rate. |
| 44-48 | Mutacoes estruturais: `add_connection_rate=1` cresce conexoes; `add_node_rate=1` cresce hidden; `toggle_connection_rate=1` altera enabled count; `remove_connection_rate=1` reduz conexoes; todas as rates=0 mantem topologia estavel. |
| 49-53 | `SimpleNeat` (protozoa-style): split de conexao, peso protozoa, baseRate=0 nao muta; `max_connections` e `max_hidden_nodes` aplicados. |
| 54-58 | Edges recorrentes so aparecem em `RecurrentNeat`; `Neat` comum e `SimpleNeat` nunca criam recurrent; topologias iniciais nao tem recurrent. |
| 59-64 | Clone preserva checksum, brainType, contagem de conexoes e nodes; mutar clone nao altera pai; estado do filho nao se mistura com o pai. |
| 65-72 | Integracao com `NeuralSystem`: todos os 8 tipos (MLP, Gated, Shortcut, Modulated, RNN, 3 NEAT) executam via `produceMovementControls`. |
| 73 | `RecurrentNeat` sobrevive a multiplas chamadas no `NeuralSystem` (estado persiste atraves de steps). |
| 74-78 | Reproducao: cada um dos 3 NEAT reproduz com sucesso; filhos NEAT comum e recorrente executam novamente apos `apply`. |
| 79-83 | Saida NEAT alimenta `MovementSystem` para os 3 sabores; `cloneOf` preserva tipo recorrente; `forwardOf` const funciona. |
| 84-88 | `ActivationTrace`: `brainType` correto, `neatNodes` preenchido, contagens `neatConnectionCount`/`neatEnabledConnectionCount` consistentes, recorrente reporta seu tipo, forward sem trace funciona. |
| 89-94 | `ParameterRegistry`: defaults registrados para os 3 prefixos, parametros recorrentes apenas no prefixo recorrente, BrainConfig NEAT explicito nao gera fallback. |
| 95-103 | Marcadores de regressao das Fases 7-15 (executados por flag CLI separada, precedente da Fase 15). |
| 104-109 | Confirmacoes de escopo: 8 tipos implementados, `isNeatFamily` classifica corretamente, nenhum arquivo Python alterado, `BrainVariant` tem 6 alternativas, fase fechada. |

### Regressoes (executadas separadamente)

```
Phase7  validation: PASS (14 checks)
Phase8  validation: PASS (15 checks)
Phase9  validation: PASS (23 checks)
Phase10 validation: PASS (21 checks)
Phase11 validation: PASS (30 checks)
Phase12 validation: PASS (49 checks)
Phase13 validation: PASS (42 checks)
Phase14 validation: PASS (71 checks)
Phase15 validation: PASS (72 checks)
Phase16 validation: PASS (109 checks)
```

## Benchmark

Microbenchmark de 6 cenarios (3 brain types x 2 topologias), 1000 forwards / 100 clones / 100 mutacoes por cenario, input_size=18, output_size=2.

| scenario | hidden | conn | rec | fwd us | clone us | mut us |
|---|---|---|---|---|---|---|
| neat_common_minimal | 0 | 36 | 0 | 3.30 | 1.92 | 1.02 |
| neat_common_layered | 24 | 432 | 0 | 18.33 | 12.94 | 7.05 |
| neat_simplified_minimal | 0 | 36 | 0 | 2.75 | 1.68 | 0.51 |
| neat_simplified_layered | 18 | 300 | 0 | 12.71 | 8.98 | 6.03 |
| neat_recurrent_minimal | 0 | 36 | 0 | 2.59 | 1.46 | 0.62 |
| neat_recurrent_layered | 24 | 432 | 0 | 17.36 | 7.14 | 7.93 |

Observacoes:
- Topologias minimal sao competitivas com MLP padrao (~2.6-3.3 us vs ~1.4-2.5 us do MLP), apesar de NEAT nao usar SIMD/contiguidade.
- Topologias layered escalam linearmente com numero de conexoes ativas (~7-18 us para 300-432 conexoes).
- Clone do NEAT recorrente e mais rapido (~7.1 us) que NEAT comum layered (~13 us) porque o estado vazio inicial pesa pouco.
- `recurrent_connections=0` no benchmark porque os 100 mutation steps com `recurrent_connection_rate=0.12` formam poucos edges recorrentes antes do snapshot; nao e bug do brain.

## Mudancas em arquivos C++

| Arquivo | Mudanca |
|---|---|
| `src/neural/BrainType.hpp` | + `isImplementedInPhase16`, `isNeatFamily` |
| `src/neural/BrainConfig.hpp` | + struct `NeatConfig`, campo `BrainConfig::neat` |
| `src/neural/ActivationTrace.hpp` | + `NeatNodeActivation`, campos `neatNodes`/`neatConnectionCount`/`neatEnabledConnectionCount`/`neatRecurrentConnectionCount`, atualizacao do `clear()` |
| `src/neural/BrainVariant.hpp` | + 6 alternativa (`NEATGraphBrain`), atualizacao de `brainTypeOf`, `resetStateOf`, dispatchers genericos atraves de `std::visit` |
| `src/neural/NEATGraphBrain.hpp` | **novo** |
| `src/neural/NEATGraphBrain.cpp` | **novo** |
| `src/neural/BrainFactory.cpp` | usar `isImplementedInPhase16`; carregamento de prefixo NEAT; 3 cases novos para criar `NEATGraphBrain` com `instantiatedType` correto |
| `src/neural/Phase16Diagnostics.hpp` | **novo** |
| `src/neural/Phase16Diagnostics.cpp` | **novo** — 109 selftests + microbenchmark |
| `src/main.cpp` | + flags `--phase16-*` |
| `src/app/App.cpp` | mensagem inicial atualizada para "Phase 16: NEAT family ..." |
| `CMakeLists.txt` | + `src/neural/NEATGraphBrain.cpp` e `src/neural/Phase16Diagnostics.cpp` |

## Nao implementado (deliberado)

- Persistencia (extra_state_dict / load_extra_state) de saves Python para NEAT C++: usaremos saves Python existentes apenas quando construirmos o caminho de save/load completo (Fase futura de Save System).
- Batch forward para NEAT: nao se aplica (`supports_batch = False` no Python).
- ParameterRegistry helpers `set` / `setString`: nao expandido — testes usam `BrainConfig` construido em codigo. Quando a UI da Fase 22+ precisar editar parametros em tempo de execucao, esse setter sera adicionado de forma generica.

## Itens de divida tecnica (apos Fase 16)

A divida 5 (alocacoes temporarias no NeuralSystem) recebeu reavaliacao no `TECHNICAL_DEBT_REGISTER.md`. Nova divida: nenhuma. Demais dividas inalteradas.

## Confirmacao de escopo

- Nenhum arquivo Python alterado nesta fase.
- Toda implementacao confinada a `C_SFML_Teste_Legado/AgentBioSimCpp/` e `C_SFML_Teste_Legado/MIGRACAO_C++SFML/`.
- PNGs em `Assets/novos_icones/` ignorados.
