# Fase 32 — Otimizacao Data-Oriented e Escala (STATUS: concluida)

Data: 2026-06-10. Build Debug + Release limpos; regressoes 7 a 32 verdes;
`--phase32-selftest` PASS (5 checks). **Speedup ~3x em toda a escala com regressao
funcional ZERO** (estado bit-identico, provado por checksum).

## Metodo (regra inviolavel cumprida)

1. **Baseline ANTES** (`--phase29-bench`, commit 70013c0, Release) com a escada estendida a
   2000/5000 agentes (novos cenarios na suite da Fase 29).
2. **Golden checksum ANTES** (`--phase32-checksum`): digest `%.17g` do estado final (posicoes,
   energias, idades, comida, pesos dos cerebros) de 4 cenarios seedados cobrindo MLP
   single/sector, RNN e NEAT, 80–120 passos cada.
3. Otimizar; recomparar: **digest identico byte a byte** ⇒ mesma seed -> mesmo estado.
4. **Benchmark DEPOIS** com os mesmos cenarios.

## Gargalos atacados (guiado pela baseline: percepcao ~47–54%, neural ~17–36%)

### 1. Multithreading deterministico de percepcao + forward neural (o ganho dominante)

Por que e bit-identico (e nao "quase"): cada agente le o mundo como CONST, escreve apenas a SUA
fatia de saida (`flatInputs[i*inputSize]` / `controls[i]` / o estado recorrente do PROPRIO
cerebro), nao ha RNG nesses sistemas e os unicos acumuladores compartilhados sao contadores
INTEIROS atomicos (soma comutativa). A ordem das operacoes de ponto flutuante DE CADA AGENTE nao
muda — so o paralelismo entre agentes. Logo qualquer numero de threads produz o mesmo resultado
do loop serial (validado: 2 execucoes -> digests identicos; e o digest pos-otimizacao == golden
pre-otimizacao serial).

- Implementacao: `std::for_each(std::execution::par)` sobre um indice [0..N) reutilizavel
  (pool de threads do Windows — sem criar threads por passo); limiar de 128 agentes (abaixo
  disso roda o mesmo lambda em serie); knob `use_parallel_systems` (Performance, default ON,
  rotulo PT-BR/EN) para A/B na Janela do Desenvolvedor.
- Pre-requisito resolvido: `SpatialHash::queryRadiusInto(..., QueryScratch&) const` — a query
  membro muta os stamps de dedup compartilhados; a nova variante const usa scratch do chamador
  (1 por thread), mesmos resultados. `SceneQuery` ganhou o overload const correspondente.
- Debug-ray do agente selecionado: so a thread dona daquele agente escreve no sink (1 escritor).

### 2. Divida 5 — alocacoes no hot loop (RESOLVIDA)

- `NeuralSystem::produceMovementControls`: o `std::vector<double> input` por agente/por passo
  virou buffer `thread_local` reutilizado (zero alocacao em regime; ~30k alocs/s eliminadas a
  1000 agentes). `syntheticInputForAgent` agora preenche buffer do chamador.
- `syncBrains`: eliminado o `std::unordered_set` reconstruido por passo — poda de cerebros
  mortos via `agents.contains` (indice O(1) que o AgentStore ja tem).
- Percepcao: buffers de candidatos + scratch de query sao `thread_local` persistentes; e o
  `vector<SpatialItem>` que a query alocava POR AGENTE/POR PASSO foi eliminado no caminho novo.

### 3. Decisoes guiadas por dados (o que NAO foi feito, e por que)

- **Parsing de config por passo**: overhead medido (SimStep − soma das secoes) era 0,33% — nao
  vale cache de config. Descartado.
- **SIMD**: mudaria a ordem de reducao de ponto flutuante ⇒ quebraria o bit-exato. Descartado
  nesta fase (a regra "zero regressao" tem prioridade sobre ganho extra).
- **Threading da colisao** (10–25% do passo): pares agente-agente fazem escrita cruzada — nao e
  paralelizavel por particao disjunta sem mudar a ordem de aplicacao (= mudar resultado).
  Fica para um algoritmo por celulas com ordem deterministica (candidato futuro, Fase 33+).

## Ganhos medidos (Release, mesma maquina, seed 1234, 3 reps ate 1000 / 2 reps 2000+)

| cenario | ANTES us/passo (passos/s) | DEPOIS us/passo (passos/s) | speedup |
|---|---:|---:|---:|
| scale_100 | 649 (1541) | 320 (3126) | 2.03x |
| scale_300 | 2100 (476) | 757 (1322) | 2.78x |
| scale_600 | 3806 (263) | 1310 (763) | 2.90x |
| scale_1000 | 6703 (149) | 2253 (444) | 2.97x |
| scale_2000 | 15799 (63) | 4824 (207) | 3.28x |
| scale_5000 | 65114 (15) | 22991 (43) | 2.83x |

- Meta do `BENCHMARK_PLAN` "2000 agentes com simulacao interativa": **superada** (207 passos/s).
- **5000 agentes acima de 30 passos/s** (43/s) — interativo tambem.
- O ganho aparece ao vivo na Janela do Desenvolvedor (Fase 30); o knob
  `use_parallel_systems` permite ver o delta on/off.

## Zero regressao (provas)

- Digest pos-otimizacao == golden pre-otimizacao (4 cenarios, `%.17g`, byte a byte).
- `--phase32-selftest` PASS: 2 execucoes -> digests identicos por cenario (threads nao afetam o
  resultado) + series de metricas identicas.
- Regressoes 7–32: todas PASS; Debug e Release limpos; smoke da UI ok.

## Divida 10 (prova de performance)

Avancou: escada ate 5000 agentes medida e reproduzivel, ~3x sobre a propria baseline C++.
O comparativo formal C++ vs Python (fechamento da divida) continua na Fase 33.

## Arquivos

Criados: `src/systems/Phase32Diagnostics.{hpp,cpp}`, este STATUS.
Modificados: `src/systems/NeuralSystem.{hpp,cpp}` (paralelo + Divida 5 + knob),
`src/perception/PerceptionSystem.{hpp,cpp}` (paralelo + scratch), `src/perception/SceneQuery.{hpp,cpp}`
(overload const + filtro fatorado), `src/simulation/SpatialHash.{hpp,cpp}` (`QueryScratch` +
query const), `src/config/ParameterDefaults.cpp` + `ParameterMetadata.cpp` (`use_parallel_systems`),
`src/bench/Benchmark.cpp` (escada 2000/5000), `src/main.cpp` (`--phase32-*`), `CMakeLists.txt`.

## Pendencias para a Fase 33

- Comparativo C++ vs Python (fecha a Divida 10) usando esta mesma suite.
- Candidatos de otimizacao restantes, se necessarios: colisao por celulas deterministicas
  (10–25% do passo), scratch interno do forward denso (alocacoes por camada dentro de
  `MLPBrain::forward`), compactacao do `.agentbiosim`.

Nao avancar para a Fase 33 sem autorizacao explicita do usuario.
