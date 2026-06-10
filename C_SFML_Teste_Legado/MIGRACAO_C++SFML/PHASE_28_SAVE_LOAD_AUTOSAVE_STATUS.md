# Fase 28 — Save/Load (.agentbiosim) (STATUS: nucleo concluido)

Data: 2026-06-10. Build Debug + Release limpos; selftests 7 a 28 verdes; `--phase28-selftest`
PASS (128 checks); `--phase28-diagnostics` mede tempo/tamanho de save/load por tipo neural.

## Decisao de escopo (alinhada com o usuario)

O usuario pediu explicitamente o **modo simples**: um arquivo `.agentbiosim` que **reabre os
organismos, seus cerebros, comida, obstaculos, mundo, especies, genomas, parametros e camera
exatamente como estavam ao salvar**, e a simulacao continua dali. NAO ha promessa de "linha do
tempo identica bit-a-bit" — o estado dos geradores aleatorios (RNG) NAO e persistido. Tambem,
por decisao do usuario, **sem compatibilidade com saves legados do Python** (formato novo,
versionado, voltado pra frente).

## Escopo entregue

1. **Formato `.agentbiosim`** (JSON, versionado — `schemaVersion`), via uma camada `io/` neutra:
   `meta`, `engine` (seed/steps/timeScale/paused/stats), `world`, `agents` (todos os campos),
   `brains` (1 por agente), `foods` (+velocidade dos pedacos), `obstacles`, `species`, `genomes`,
   `params` (todos os valores do registry) e `camera`.
2. **Cerebros (as "mentes")**: `neural::BrainSerializer` captura config + parametros aprendidos e
   reconstroi via `BrainFactory::createBrain(config)` + sobrescrita (acesso `friend`). Cobre os 6
   tipos: MLP, Gated, Shortcut, Modulated, RNN (estado recorrente) e a familia NEAT (grafo de
   nos/conexoes habilitadas/desabilitadas/recorrentes + contadores de inovacao + estado recorrente).
3. **Salvar / Salvar Como / Abrir** no menu Arquivo (dialogos nativos do Windows, `io/FileDialog`).
   "Salvar" reusa o ultimo caminho; "Salvar Como" sempre pergunta; "Abrir" restaura parametros +
   estado do engine + camera e limpa a selecao.
4. **Versionamento de schema**: o loader recusa um save com schema mais novo que o app, com mensagem.

## Arquitetura (como funciona, sem quebrar o engine headless)

- **`sim::SimulationSnapshot`** (POD) e a fronteira neutra: `SimulationRunner::snapshot()` captura todo
  o estado (stores via acessores + cerebros via `NeuralSystem::captureBrains()` + mundo + contadores);
  `restore()` repoe tudo (stores com **ids exatos** — para `agente->genoma/especie` e o mapa de
  cerebros por id continuarem validos).
- **Stores** ganharam `restore()` + `nextId()` (Agent/Food/Obstacle SoA; Genome/Species por records).
- **`io/Json`** e um leitor/escritor JSON proprio, sem dependencias (nenhuma lib JSON vendada);
  doubles com `%.17g` = roundtrip exato. **`io/SaveFile`** converte snapshot+params+camera <-> JSON
  <-> arquivo.
- **App** orquestra: ao salvar, junta snapshot + valores do registry + camera; ao abrir, aplica os
  params no registry, chama `runner_.restore()`, re-deriva config/render e ajusta a camera. O engine
  trata os comandos de save/load como no-op (a seta de dependencia continua UI -> engine).

## Testes — `--phase28-selftest` (PASS, 128 checks)

Para os 8 tipos de cerebro: salvar -> carregar preserva contagem de agentes/comida/especies/genomas,
o mundo, o contador de passos, posicoes/energia/genoma por id, e as **mentes** (checksum por cerebro,
por id). O loader reabre e a simulacao **continua sem crash**. Parametros e camera fazem roundtrip.

## Diagnostics — `--phase28-diagnostics` (Release)

| tipo | agentes | save (ms) | load (ms) | tamanho (MB) |
|---|---|---|---|---|
| mlp | ~1000 | ~660 | ~800 | ~60 |
| simple_rnn | ~1000 | ~2200 | ~1650 | ~74 |
| neat | ~1000 | ~260 | ~370 | ~13 |

Nota: o arquivo e grande para muitos agentes densos porque e JSON legivel com todos os pesos em
`%.17g`. Aceitavel para o uso (salvar/reabrir um substrato). Compactacao (gzip/binario) e otimizacao
futura, se necessario.

## Arquivos

Criados: `src/io/Json.{hpp,cpp}`, `src/io/SaveFile.{hpp,cpp}`, `src/io/FileDialog.{hpp,cpp}`,
`src/neural/BrainSerializer.{hpp,cpp}`, `src/sim/SimulationSnapshot.hpp`,
`src/systems/Phase28Diagnostics.{hpp,cpp}`, este STATUS.
Modificados: 6 headers de cerebro (`friend struct BrainSerializer;`); stores
Agent/Food/Obstacle/Genome/Species (`restore()`/`nextId()`); `NeuralSystem` (capture/restore brains);
`SimulationRunner` (`snapshot()/restore()`); `core/Command.hpp` (3 comandos); `App.{hpp,cpp}`
(save/load + dialogo); `ui/ImGuiUi.cpp` (menu Arquivo); `main.cpp` (`--phase28-*`); `CMakeLists.txt`
(fontes + `comdlg32`).

## Fora de escopo / pendencias

- **Determinismo bit-a-bit + estado de RNG** — descartado por decisao do usuario (modo simples).
- **Autosave periodico + recovery on close** — no prompt original da fase, mas **nao pedido** pelo
  usuario nesta rodada; nao implementado. Pode ser adicionado depois (timer no App + arquivo de
  recovery; a infra de save ja existe).
- **Export/import separado de substrato (JSON) e de genoma/agente** — idem: o save completo
  `.agentbiosim` ja cobre a necessidade declarada; exports avulsos ficam para quando forem pedidos.
- **Feedback de UI** de sucesso/erro hoje vai para o console (stdout/stderr); um toast/modal in-app
  e um polimento futuro.
- Compactacao do arquivo (gzip/binario) para mundos grandes.

Nao avancar para a Fase 29 sem autorizacao explicita do usuario.
