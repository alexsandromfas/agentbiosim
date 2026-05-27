# Auditoria de Cobertura do Planejamento

## 1. Estado atual da migracao

Estado oficial em 2026-05-27:

- Fase 0 concluida.
- Fase 1 concluida.
- Fase 2 concluida.
- Fase 3 concluida.
- Fase 4 concluida.
- Fase 5 concluida.
- Fase 6 concluida.
- Fase 7 concluida.
- Fase 8 concluida e comitada.
- A revisao do planejamento deve comecar da Fase 9 em diante.
- As fases ja concluidas nao devem ser renumeradas nem reabertas, salvo bug critico tratado em fase futura de correcao.
- Nenhuma nova fase de implementacao foi iniciada nesta auditoria.

## 2. Resumo executivo

O planejamento conceitual nao esqueceu as redes neurais avancadas. Elas aparecem em `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`, `PROPOSED_CPP_ARCHITECTURE.md` e `BENCHMARK_PLAN.md`.

A lacuna era executavel: `MIGRATION_PHASES.md` tratava a Fase 9 apenas como "MLP" e depois pulava para sensores, visao, reproducao, predadores, UI e otimizacao. Isso nao deixava claro quando seriam implementadas, validadas e benchmarkadas:

- Gated MLP.
- Shortcut MLP.
- Modulated MLP.
- Simple RNN.
- NEAT comum.
- NEAT simplificada.
- NEAT recorrente.
- Visualizacao neural especifica para RNN/NEAT.
- Serializacao neural por tipo.
- Benchmarks comparativos por tipo neural.

Tambem havia lacunas de cronograma para comida chunk completa, obstaculos, oclusao, labels/especies/genomas, UI por areas, save/load/export/import, autosave e benchmarks formais. Esses itens estavam documentados conceitualmente, mas sem fase executavel suficientemente clara.

Conclusao: a documentacao conceitual era boa, mas o cronograma futuro precisava ser reestruturado da Fase 9 em diante. Esta auditoria corrige isso sem alterar codigo.

## 3. Tabela de cobertura por funcionalidade

| Funcionalidade | Existe no Python? | Inventario | Arquitetura | Fase antes da auditoria | Fase responsavel revisada | Status | Acao recomendada |
|---|---|---|---|---|---|---|---|
| Simulacao | Sim | Sim | Sim | Fases 1, 3, 7, 8, 18, 19 | 0-8 concluidas; 28, 31 para validacao final | Coberto | Manter fases historicas e validar por paridade. |
| Mundo/camera | Sim | Sim | Sim | Fase 3 | Fase 3 concluida; 22 e 29 para UI/canvas | Coberto | Nao reabrir; validar em UI e save/load. |
| Agentes | Sim | Sim | Sim | Fase 4 | Fase 4 concluida; 17 para especies/genomas | Parcialmente coberto | Adicionar especies/genomas e compatibilidade de labels. |
| Comida instantanea | Sim | Sim | Sim | Fase 7 | Fase 7 concluida; 31 para paridade | Coberto | Validar contra Python. |
| Comida chunk/pedacos | Sim | Sim | Sim | Sem fase clara alem de nota | Fase 19 | Parcialmente coberto | Implementar consumo por pedacos, reposicao e benchmark. |
| Predadores | Sim | Sim | Sim | Fase 13 generica | Fase 18 | Parcialmente coberto | Modelar como especie/dieta, nao classe rigida. |
| Obstaculos | Sim | Sim | Sim | Nao claro | Fase 20 | Nao coberto antes | Implementar mapa, canvas, colisao, spawn blocking e oclusao. |
| Sensores/visao | Sim | Sim | Sim | Fases 10, 11 | Fases 10, 11, 12 | Parcialmente coberto | Separar contrato de inputs, raycast/fullbody e sector/bins. |
| Visao single | Sim | Sim | Sim | Fase 10 | Fase 10 | Coberto futuro | Integrar visao -> cerebro -> locomocao. |
| Visao fullbody/raycast | Sim | Sim | Sim | Citada em benchmark, sem fase propria | Fase 11 | Nao coberto antes | Criar fase explicita com paridade e microbenchmark. |
| Visao sector/bins | Sim | Sim | Sim | Fase 11 | Fase 12 | Coberto futuro | Implementar bins, subdivisoes, projection, candidate limit. |
| Canais de retina | Sim | Sim | Sim | Fase 10 generica | Fases 10-12 | Parcialmente coberto | Validar input size, DRGB, D dedicado e modos ponderados. |
| Redes neurais MLP | Sim | Sim | Sim | Fase 9 | Fase 9 | Coberto futuro | Fase 9 deve criar arquitetura extensivel, nao MLP isolada. |
| Redes neurais avancadas | Sim | Sim | Sim | Sem fase executavel clara | Fases 14-16 | Nao coberto antes | Dividir densas, RNN e NEAT. |
| Gated MLP | Sim | Sim | Sim | Sem fase | Fase 14 | Nao coberto antes | Implementar gates, mutacao, batch e benchmark. |
| Shortcut MLP | Sim | Sim | Sim | Sem fase | Fase 14 | Nao coberto antes | Implementar atalho entrada -> saida e benchmark. |
| Modulated MLP | Sim | Sim | Sim | Sem fase | Fase 14 | Nao coberto antes | Combinar gates + shortcut em tipo proprio. |
| RNN simples | Sim | Sim | Sim | Sem fase | Fase 15 | Nao coberto antes | Implementar estado recorrente por agente e reset/copy. |
| NEAT comum | Sim | Sim | Sim | Sem fase | Fase 16 | Nao coberto antes | Implementar topologia, mutacoes estruturais e executor individual/grupos. |
| NEAT simplificada | Sim | Sim | Sim | Sem fase | Fase 16 | Nao coberto antes | Preservar parametros proto NEAT e benchmark proprio. |
| NEAT recorrente | Sim | Sim | Sim | Sem fase | Fase 16 | Nao coberto antes | Implementar recorrencia e estado por agente/topologia. |
| Evolucao/reproducao/mutacao | Sim | Sim | Sim | Fase 12 | Fase 13, 14-16 para mutacoes neurais | Parcialmente coberto | Separar reproducao base de mutacoes por tipo neural. |
| Energia/metabolismo | Sim | Sim | Sim | Fase 7 | Fase 7 concluida; 31 paridade | Coberto | Validar com locomocao/visao/cerebro. |
| Colisao/interacao | Sim | Sim | Sim | Fase 7 parcial | Fases 7, 18, 19, 21 | Parcialmente coberto | Completar predacao, chunk movel e colisao fisica. |
| UI | Sim | Sim | Sim | Fases 14, 17 genericas | Fases 22-25, 29 | Parcialmente coberto | Quebrar por menus, preferencias, paineis e viewer. |
| Menus | Sim | Sim | Sim | UI generica | Fase 22 e 29 | Parcialmente coberto | Checklist por menu superior. |
| Abas | Sim | Sim | Sim | UI generica | Fases 23-24 e 29 | Parcialmente coberto | Confirmar abas legadas manualmente. |
| Ferramentas de canvas | Sim | Sim | Sim | UI generica | Fases 22, 24, 20 | Parcialmente coberto | Canvas tools precisam fase propria. |
| Graficos/metricas | Sim | Sim | Sim | Fase 16 | Fase 26 | Coberto futuro | Incluir intelligence e series por especie. |
| Agente selecionado | Sim | Sim | Sim | UI generica | Fase 25 | Nao claro antes | Implementar inspector retratil e modo rede neural. |
| Visualizador neural | Sim | Sim | Sim | Citado, sem fase propria | Fase 25 | Nao claro antes | Adaptar MLP, RNN e NEAT. |
| Labels/grupos/especies | Sim | Sim | Sim | Fase 13/17 genericas | Fase 17 | Parcialmente coberto | SpeciesStore/GenomeStore e aliases `labels`. |
| Exportacao/importacao | Sim | Sim | Sim | Fase 15 | Fase 27 | Coberto futuro | Incluir genoma, substrato, `.biosim` e ativacoes. |
| Save/load | Sim | Sim | Sim | Fase 15 | Fase 27 | Coberto futuro | Schema versionado e roundtrip. |
| Autosave | Sim | Sim | Sim | Fase 15 generica | Fase 27 | Parcialmente coberto | Tratar recovery, intervalos e ativacoes. |
| Logs/diagnostico | Sim | Sim | Sim | Fase 16 | Fase 26 | Coberto futuro | Logger, heartbeat, crash/recovery. |
| Profiling | Sim | Sim | Sim | Fase 16 | Fase 26 e 28 | Coberto futuro | Profiler por sistema e reports. |
| Execucao headless | Sim | Sim | Sim | Fases 1/19 | Fase 28 | Parcialmente coberto | Formalizar CLI de experimentos. |
| Benchmarks | Sim/conceito | Sim | Sim | Fase 19 | Fases 28 e 31, microbench em cada fase | Parcialmente coberto | Criar fase formal de runner e campanha final. |
| Performance/otimizacoes | Sim | Sim | Sim | Fase 18 | Fase 30 | Coberto futuro | Otimizar depois de paridade funcional. |
| Persistencia de parametros | Sim | Sim | Sim | Fase 2 e 15 | Fase 2 concluida; 23 e 27 futuras | Parcialmente coberto | UI e save/load ainda faltam. |
| Compatibilidade de saves/genomas | Sim | Sim | Sim | Fase 15 generica | Fase 27 | Parcialmente coberto | Loader versionado e aliases legados. |

## 4. Tabela de cobertura por parametros

| Grupo de parametros | Fase que cadastra | Fase que usa funcionalmente | Fase que expoe na UI | Fase que valida/paridade | Status | Observacoes |
|---|---|---|---|---|---|---|
| Gerais, mundo e tempo | Fase 2 | Fases 3, 7, 8, 28 | Fases 22-23 | Fase 31 | Parcialmente coberto | Cadastro e parte funcional ja existem; UI/save final faltam. |
| Performance, render e percepcao global | Fase 2 | Fases 5, 10-12, 28, 30 | Fase 23 | Fases 28, 31 | Parcialmente coberto | Parametros Python/Numba viram aliases ou opcoes C++ equivalentes. |
| Visao por bins/setores | Fase 2 | Fase 12 | Fase 23 | Fases 12, 28, 31 | Parcialmente coberto | Antes nao havia fase suficiente para todos os modos/projecoes. |
| Redes neurais globais | Fase 2 | Fases 9, 14, 15 | Fase 23 | Fases 14, 15, 28, 31 | Parcialmente coberto | Fase 9 deve criar `BrainType`; densas avancadas e RNN tem fases proprias. |
| NEAT | Fase 2 | Fase 16 | Fase 23 | Fases 16, 28, 31 | Nao coberto antes | Agora ganha fase propria para comum, simplificada e recorrente. |
| Comida e substrato | Fase 2 | Fases 7 e 19 | Fases 23-24 | Fases 19, 28, 31 | Parcialmente coberto | Instantanea feita; chunk completa pendente. |
| Bacterias/organismo base | Fase 2 | Fases 4, 7, 8, 9-18 | Fase 24 | Fase 31 | Parcialmente coberto | Deve migrar para especie/genoma preservando aliases. |
| Predadores | Fase 2 | Fase 18 | Fase 24 | Fases 18, 28, 31 | Parcialmente coberto | Deve virar especie/dieta configuravel. |
| Fisica | Fase 2 | Fase 8 parcial; Fase 21 completa | Fase 23 | Fases 21, 28, 31 | Parcialmente coberto | Locomocao existe; colisao/viscosidade/Brownian ainda faltam. |
| UI, debug, save e aparencia | Fase 2 | Fases 22-27 | Fases 22-25 | Fases 27, 29, 31 | Parcialmente coberto | Cadastro nao basta; precisa UI e persistencia. |
| Estados fora de defaults | Fase 2 parcial | Fases 17, 23, 27 | Fases 22-25 | Fases 27, 31 | Parcialmente coberto | Camera, labels, current path, ui_params e ativacoes precisam schema/loader. |
| Metadados labels/especies | Fase 2 parcial | Fase 17 | Fase 24 | Fases 27, 31 | Parcialmente coberto | `label_genome_ref` ainda e conceitual e precisa implementacao. |

## 5. Tabela de cobertura da UI

| Item de UI | Fase de implementacao | Fase de validacao de paridade | Status | Pendencia |
|---|---|---|---|---|
| Menus superiores | Fase 22 | Fase 29 | Parcialmente coberto | Checar `Arquivo`, `View`, `Preferencias`, `Agente/Genoma`, `Ajuda`. |
| Acoes de arquivo | Fase 27 | Fase 29/31 | Parcialmente coberto | Novo/abrir/salvar/salvar como/export/import. |
| Preferencias | Fase 23 | Fase 29 | Parcialmente coberto | Simulacao, visao, redes neurais, autosave, aparencia, performance. |
| Editor genetico | Fase 24 | Fase 29 | Parcialmente coberto | Genoma, corpo, dieta, energia, visao, MLP/RNN/NEAT conforme tipo. |
| Populacao | Fase 17/24 | Fase 29 | Parcialmente coberto | Migrar para especies/labels sem perder min/max/inicial. |
| Substrato | Fase 24 | Fase 29 | Parcialmente coberto | Instant/chunk, clear food, cores, formato e export/import. |
| Labels/grupos/especies | Fase 17/24 | Fase 29/31 | Parcialmente coberto | Nome, cor, min/max/inicial, grafico, reset brain, genoma associado. |
| Painel do agente selecionado | Fase 25 | Fase 29 | Nao claro antes | Inspector retratil, abas Genoma/Rede Neural, custo zero quando oculto. |
| Visualizador neural | Fase 25 | Fase 29/31 | Nao claro antes | MLP completa, RNN simplificada, NEAT apropriada, ActivationTrace. |
| Grafico | Fase 26 | Fase 29 | Parcialmente coberto | Series, janela, taxa, labels/especies, inteligencia. |
| Ferramentas de canvas | Fase 22/24/20 | Fase 29 | Parcialmente coberto | Selecao, lasso, comida, agente, obstaculo, mover, deletar, pipeta. |
| Atalhos | Fase 22 | Fase 29 | Parcialmente coberto | Delete, Space, Esc, R, F, T, V, WASD/setas, scroll. |
| Autosave | Fase 27 | Fase 29/31 | Parcialmente coberto | Intervalo, recovery, ativacoes, pretty JSON. |
| Aparencia | Fase 23 | Fase 29 | Parcialmente coberto | Fundo, gradientes, borda, resolution scale. |
| Redes neurais | Fase 23/24/25 | Fase 29/31 | Nao claro antes | Dropdown e paineis por MLP/Gated/Shortcut/Modulated/RNN/NEAT. |
| Sistema de visao | Fase 23/25 | Fase 29/31 | Parcialmente coberto | Modos, bins, debug visual, multi-selecao. |
| Export/import | Fase 27 | Fase 29/31 | Parcialmente coberto | Substrato, genoma, agente legado, ativacoes. |
| Save/load | Fase 27 | Fase 31 | Parcialmente coberto | `.biosim`, schema, aliases, camera, labels, RNG. |
| Agente/genoma selecionado | Fase 24/25/27 | Fase 29/31 | Parcialmente coberto | Pipeta, importar/exportar e aplicar genoma por especie/selecionados. |

## 6. Auditoria arquitetural neural obrigatoria

As redes neurais avancadas estao reconhecidas conceitualmente. O ajuste necessario e tornar essa cobertura executavel desde a Fase 9.

A Fase 9 deve implementar somente MLP inicial, mas tambem deve criar a base arquitetural neural extensivel:

- `BrainType` ou equivalente com pelo menos `Mlp`, `GatedMlp`, `ShortcutMlp`, `ModulatedMlp`, `SimpleRnn`, `NeatCommon`, `NeatSimplified`, `NeatRecurrent`.
- `BrainFactory` ou equivalente para criar cerebro por `BrainConfig`.
- Separacao entre `BrainConfig`/genoma, `BrainState`/pesos/estado/topologia e `BrainExecutor`.
- Separacao entre cerebro e `AgentStore`: agentes guardam handle/indice, nao objeto neural pesado no hot path.
- Dense executors preparados para batch por assinatura.
- Fallback individual ou por topologia para NEAT.
- Estado recorrente por agente para RNN e NEAT recorrente.
- `ActivationTrace` opcional para agente selecionado e neural viewer.
- Serializacao neural versionada futura.
- Diferentes tipos neurais por especie/genoma.
- `input_size` dependente das fases 10-12 de sensores/visao.
- `output_size` dependente da Fase 8 de locomocao.

Regras executaveis:

- A Fase 9 nao pode hardcodar MLP no `App`, `Renderer`, `MovementSystem` ou `AgentStore`.
- A Fase 9 nao deve implementar redes avancadas, mas deve reservar a interface para elas.
- Redes avancadas devem ser implementadas em fases futuras explicitas: Fase 14 para densas avancadas, Fase 15 para RNN, Fase 16 para NEAT.
- Cada rede avancada precisa de testes funcionais, serializacao planejada e benchmark proprio.
- Se uma rede avancada for adiada por custo, isso deve aparecer como pendencia documentada da fase correspondente, nao como nota solta.

## 7. Lacunas encontradas

| Item | Onde foi encontrado | Ja aparece na arquitetura? | Ja aparece nos parametros? | Ja aparece na UI? | Ja aparece nos benchmarks? | Por que ainda era lacuna | Risco | Fase sugerida | Docs atualizados |
|---|---|---|---|---|---|---|---|---|---|
| Gated/Shortcut/Modulated MLP | Feature, UI, params, brain.py | Sim | Sim | Sim | Sim | Sem fase executavel | MLP fechada e perda de comportamento atual | 14 | Fases, guia, riscos, benchmark |
| Simple RNN | Feature, UI, params, brain.py | Sim | Sim | Sim | Sim | Sem fase executavel | Perda de memoria temporal | 15 | Fases, guia, riscos, benchmark |
| NEAT comum/simplificada/recorrente | Feature, UI, params, brain.py | Sim | Sim | Sim | Sim | Sem fase executavel | Topologia neural atual some ou quebra batch | 16 | Fases, guia, riscos, benchmark |
| Brain architecture extensivel desde MLP | Architecture | Sim | Sim | Indireto | Sim | Fase 9 era apenas "MLP" | Hardcode no App/AgentStore | 9 | Fases, guia, arquitetura |
| Fullbody/raycast | sensors.py, benchmark | Sim | Sim | Sim | Sim | Fase 10/11 nao separava modo | Perder modo de visao atual | 11 | Fases, audit |
| Chunk food completa | systems.py, controllers.py | Sim | Sim | Sim | Sim | Fase 7 so cobre instantanea | Perder dinamica de alimentacao lenta | 19 | Fases, audit |
| Obstaculos e oclusao | obstacles.py, sensors.py, UI | Sim | Sim | Sim | Sim | Sem fase propria | Visao/spawn/movimento divergem | 20 | Fases, audit |
| Especies/labels/genomas | ui.py, engine.py, params | Sim | Sim | Sim | Sim | Fases genericas | Perder labels, min/max, genoma associado | 17 | Fases, guide |
| UI por areas | UI inventory | Sim | Sim | Sim | Sim | UI inicial/paridade eram grandes demais | Esquecer menus/ferramentas | 22-25, 29 | Fases, riscos |
| Save/load/export/import/autosave | ui.py, parameter inventory | Sim | Sim | Sim | Sim | Fase generica demais | Incompatibilidade de saves/genomas | 27 | Fases, riscos |
| Benchmarks formais | benchmark plan | Sim | N/A | N/A | Sim | Fase 19 final tardia | Otimizar sem metrica | 28, 31 | Fases, benchmark |

## 8. Reescrita das fases futuras a partir da Fase 9

Esta secao e a fonte de decisao para atualizar `MIGRATION_PHASES.md`.

### Fase 9: MLP inicial com arquitetura neural extensivel

- Objetivo: implementar MLP baseline e a fundacao neural extensivel.
- Escopo: `BrainType`, `BrainConfig`, `BrainState`, `BrainHandle`, `BrainFactory`, `BrainExecutor`, MLP, mutacao de pesos, clone/copy e `ActivationTrace` minimo.
- Fora de escopo: sensores reais, redes avancadas, reproducao.
- Referencias: `sim/brain.py`, `sim/actuators.py`, `PARAMETER_INVENTORY.md`, `PROPOSED_CPP_ARCHITECTURE.md`.
- Criterios: forward deterministico, mutacao testada, output compatel com Fase 8.
- Testes: forward, mutation_rate 0, mutation_rate > 0, copy, output -> MovementSystem.
- Benchmark: forward individual e batch para 100/300/600/1000 agentes.
- Cobre: MLP e base para redes avancadas.

### Fase 10: Sensores, canais de retina e visao single

- Objetivo: gerar input neural real para MLP usando retina single.
- Escopo: input size por retinas/olhos/canais, D/R/G/B e modos ponderados, SceneQuery basico.
- Fora de escopo: fullbody/raycast estrito, sector/bins, obstaculos.
- Testes: objetos em posicoes controladas, canais normalizados, input_size correto.
- Benchmark: custo por agente/retina/canal.

### Fase 11: Visao fullbody/raycast e debug visual de visao

- Objetivo: preservar modo fullbody/raycast e visualizacao equivalente ao Python.
- Escopo: raycast/fullbody, see_food/agents/predators/obstacles flags preparadas, debug para agente selecionado.
- Fora de escopo: sector/bins otimizado e oclusao completa.
- Testes: ray hits, distancia/cor, multi-olhos, retina_skip.
- Benchmark: single vs fullbody/raycast.

### Fase 12: Visao sector/bins otimizada

- Objetivo: implementar bins/setores com subdivisoes de distancia sem aumentar inputs.
- Escopo: `retina_bins_*`, candidate limit, projection modes, falloff, high scale auto sector.
- Fora de escopo: oclusao final por obstaculo se ObstacleStore ainda nao existir.
- Testes: mesmo tamanho de output, intensidade por distancia, candidato em setor vizinho conforme projection.
- Benchmark: sector vs raycast, subdivisoes 1/5/20/99, candidatos.

### Fase 13: Reproducao, mutacao base e genoma

- Objetivo: split por energia, idade/cooldown, mutacao base de genoma e pesos MLP.
- Escopo: `GenomeStore` inicial, heranca, mutation rate/strength, min/max rescue preparado.
- Fora de escopo: redes avancadas e especies completas.
- Testes: filho herda e muta, energia dividida, cooldown/idade.
- Benchmark: nascimentos/remocoes por step.

### Fase 14: Redes densas avancadas

- Objetivo: implementar Gated MLP, Shortcut MLP e Modulated MLP.
- Escopo: gates, atalhos input->output, modulacao combinada, mutacoes especificas, batch por assinatura.
- Fora de escopo: RNN e NEAT.
- Testes: parametros de gate/shortcut, mutation overrides, equivalencia de output shape.
- Benchmark: MLP vs Gated vs Shortcut vs Modulated.

### Fase 15: RNN simples

- Objetivo: implementar SimpleRNNBrain com memoria curta.
- Escopo: estado recorrente por agente, memory decay, clip, reset_state_on_copy, ActivationTrace simplificado.
- Fora de escopo: NEAT recorrente.
- Testes: estado persiste entre steps, decay 0/0.6/0.9, reset no filho.
- Benchmark: RNN vs MLP em 100/300/600/1000 agentes.

### Fase 16: Familia NEAT

- Objetivo: implementar NEAT comum, NEAT simplificada e NEAT recorrente.
- Escopo: topologia, nodes/conexoes, enable/toggle/remove/add/reset, limites, recorrencia e executor individual/grupo por assinatura.
- Fora de escopo: otimizacao global pesada.
- Testes: mutacoes estruturais, limites, recurrent state, serializacao conceitual.
- Benchmark: NEAT comum vs simplificada vs recorrente; custo quando desligada deve ser zero para MLP.

### Fase 17: Especies, labels e genomas

- Objetivo: substituir tipos rigidos por especies/genomas mantendo aliases `bacteria`, `predator` e `labels`.
- Escopo: `SpeciesStore`, `GenomeStore`, min/max/inicial, cor, grafico, reset neural por especie, label_genome_ref.
- Fora de escopo: UI completa e predacao final.
- Testes: reset por especie, label save metadata, limites populacionais.
- Benchmark: custo de agrupamento por especie.

### Fase 18: Predacao e dieta generica

- Objetivo: implementar predadores como especies/dietas configuraveis.
- Escopo: diet_food, diet_agents, diet_same_label, predacao, eficiencia, corpse_to_food inicial.
- Fora de escopo: UI completa de especies.
- Testes: predador come presa, canibalismo respeitado, energia ganha.
- Benchmark: cenarios presa/predador.

### Fase 19: Comida chunk/pedacos completa

- Objetivo: preservar comida em pedacos e reposicao por clusters/particulas.
- Escopo: bite_seconds, particle radius, cluster radius, spacing, spawn_cluster, grow_existing, grow_particles, clear food.
- Fora de escopo: fisica avancada de chunks se exigir CollisionSystem completo.
- Testes: consumo parcial, reposicao, trim, target.
- Benchmark: instant vs chunk, modos de reposicao.

### Fase 20: Obstaculos e oclusao

- Objetivo: implementar obstaculos, desenho/canvas e bloqueios.
- Escopo: ObstacleStore/Map, pincel, apagar, bloqueio de movimento/spawn/comida e oclusao de visao.
- Fora de escopo: UI final polida.
- Testes: agente nao atravessa, comida nao nasce em obstaculo, visao bloqueada quando flag desligar ver atraves.
- Benchmark: visao com/sem oclusao.

### Fase 21: Colisoes e fisica opcional

- Objetivo: agente-agente, comida movel, elasticidade, transferencia, viscosidade e Brownian.
- Escopo: CollisionSystem, parametros de colisao, chunk mobility/collision/adhesion reavaliada.
- Fora de escopo: comportamento nao documentado.
- Testes: separacao, impulso maximo, energia preservada dentro de tolerancia.
- Benchmark: fisica on/off.

### Fase 22: UI base, menus e canvas

- Objetivo: criar UI tecnica inicial com menu, toolbar, viewport e canvas tools essenciais.
- Escopo: Dear ImGui/SFML, play/pause/stop, pan/zoom, selecao, lasso/retangulo, atalhos.
- Fora de escopo: editor completo.
- Testes: fluxos de input e janela.
- Benchmark: UI ligada/desligada.

### Fase 23: UI de parametros e preferencias

- Objetivo: expor parametros por schema.
- Escopo: simulacao, fisica, sistema de visao, redes neurais, autosave, aparencia, performance.
- Fora de escopo: save/load completo.
- Testes: cada controle altera ParameterRegistry e nao quebra runtime.
- Benchmark: custo UI aberta/fechada.

### Fase 24: Editor genetico, especies e substrato

- Objetivo: reconstruir paineis operacionais principais.
- Escopo: editor de genoma, especies/labels, populacao, substrato, limpar comida, aplicar por especie/selecionados.
- Fora de escopo: neural viewer completo.
- Testes: aplicar parametros, importar genoma para editor, atribuir especie.
- Benchmark: custo do painel aberto.

### Fase 25: Agente selecionado e visualizador neural

- Objetivo: inspector retratil e neural viewer conectado.
- Escopo: abas Genoma/Rede Neural, ActivationTrace, MLP, RNN simplificada, NEAT apropriada, visao do agente selecionado.
- Fora de escopo: paridade completa de UI.
- Testes: oculto nao calcula caro, selecionado mostra valores corretos.
- Benchmark: viewer ligado/desligado.

### Fase 26: Metricas, inteligencia, logs, diagnostico e profiler

- Objetivo: preservar metricas, graficos, profiler e diagnosticos.
- Escopo: MetricsSystem, intelligence metrics, chart data, Logger, heartbeat, ScopedTimer.
- Fora de escopo: save/load completo.
- Testes: series por especie, profiler on/off, logs.
- Benchmark: overhead metrics/profiler.

### Fase 27: Save/load/export/import/autosave

- Objetivo: persistencia versionada e compatibilidade.
- Escopo: `.biosim`, JSON substrato, genoma/agente, autosave, recovery, rng state, camera, labels/species, brains por tipo.
- Fora de escopo: otimizacoes finais.
- Testes: roundtrip, abrir save legado, export/import genoma com MLP/RNN/NEAT.
- Benchmark: salvar/carregar cenarios grandes.

### Fase 28: Benchmark runner e experimentos headless formais

- Objetivo: implementar infraestrutura formal de benchmark.
- Escopo: CLI headless, scenarios, CSV/JSON/MD reports, commit/build metadata, percentuais por sistema.
- Fora de escopo: otimizar sem medicao.
- Testes: reproducibilidade por seed.
- Benchmark: executar suite minima.

### Fase 29: Paridade completa de UI

- Objetivo: fechar checklist `UI_INVENTORY.md`.
- Escopo: menus, abas, botoes, atalhos, preferencias, fluxos criticos e verificacao manual.
- Fora de escopo: novas features nao existentes.
- Testes: checklist item a item.
- Benchmark: UI completa com simulacao.

### Fase 30: Otimizacao data-oriented e escala

- Objetivo: otimizar gargalos medidos.
- Escopo: SoA refinado, batch por assinatura, buffers persistentes, alocacao zero no loop, threading seletivo/SIMD quando medido.
- Fora de escopo: otimizar comportamento nao validado.
- Testes: regressao funcional e performance antes/depois.
- Benchmark: 600/1000/2000/5000+ agentes.

### Fase 31: Campanha final de paridade Python vs C++

- Objetivo: validar migracao completa.
- Escopo: paridade por seed, saves/genomas reais, UI, benchmarks, relatorio final.
- Fora de escopo: novas features.
- Testes: suite completa.
- Benchmark: plano completo de `BENCHMARK_PLAN.md`.

## 9. Atualizacao do MIGRATION_PHASES.md

`MIGRATION_PHASES.md` deve preservar as Fases 0 a 8 como historicas/concluidas e substituir a sequencia futura por Fases 9 a 31 descritas acima.

## 10. Atualizacao do CODEX_MIGRATION_GUIDE.md

O guia deve exigir:

- Consultar `PLANNING_COVERAGE_AUDIT.md` antes de iniciar qualquer fase futura.
- Toda fase futura deve citar os itens de `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md` e `UI_INVENTORY.md` que cobre.
- Nao considerar a migracao completa enquanto houver item `nao coberto` sem fase futura.
- Arquitetura neural extensivel desde a Fase 9.
- Fases 0 a 8 nao devem ser reescritas.

## 11. Atualizacao do PROPOSED_CPP_ARCHITECTURE.md

A arquitetura deve reforcar:

- `BrainType`, `BrainFactory`, `BrainHandle`.
- `BrainConfig`, `BrainState`, `BrainExecutor`.
- Dense batch para MLP/Gated/Shortcut/Modulated/RNN.
- Fallback individual/grupo por assinatura para NEAT.
- `ActivationTrace` e `BrainSerializer` como contratos planejados.

## 12. Atualizacao do MIGRATION_RISKS.md

Riscos adicionados/reforcados:

- Item inventariado sem fase executavel.
- Redes avancadas sem fase clara.
- MLP hardcoded impedindo extensao.
- UI inventariada sem paridade por menu/ferramenta.
- Parametro cadastrado mas nunca usado.
- Save/load/export/import deixado tarde demais.
- Benchmarks planejados sem execucao real.

## 13. Atualizacao do BENCHMARK_PLAN.md

O benchmark plan deve ligar explicitamente suites futuras as fases:

- Fase 9: MLP.
- Fase 14: Gated/Shortcut/Modulated.
- Fase 15: RNN.
- Fase 16: NEAT.
- Fases 10-12: visao single/fullbody/sector.
- Fase 19: comida chunk.
- Fase 20: obstaculos/oclusao.
- Fase 27: save/load.
- Fase 28: runner formal.
- Fase 31: paridade final Python vs C++.

## 14. Criterios obrigatorios

- Nenhum codigo C++ foi alterado por esta auditoria.
- Nenhum arquivo Python foi alterado por esta auditoria.
- Nenhuma fase de implementacao foi iniciada.
- Somente Markdown de planejamento dentro de `C_SFML_Teste_Legado/MIGRACAO_C++SFML` deve ser alterado.
- Fases 0 a 8 ficam preservadas como concluidas.
