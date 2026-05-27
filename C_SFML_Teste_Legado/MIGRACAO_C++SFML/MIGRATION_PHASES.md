# Fases Propostas da Migracao

Cada fase deve ser pequena, testavel e revisavel. Nao migrar tudo de uma vez.

Estado atual: apenas a Fase 0 esta autorizada. A Fase 1 cria estrutura C++/SFML/CMake e so deve comecar depois de autorizacao explicita do usuario.

## Fase 0: Auditoria e Documentacao

- Objetivo: entender Python atual, inventariar parametros, UI, features, riscos e benchmarks.
- Entregaveis: estes arquivos Markdown.
- Criterio de conclusao: documentacao criada e revisada pelo usuario.
- Nao implementar codigo.

## Fase 1: Esqueleto C++/SFML/CMake

- Objetivo: criar projeto minimo que abre janela SFML e compila Debug/Release.
- Criterio: build limpo, executavel abre e fecha, sem simulacao ainda.
- Benchmark: tempo de startup e FPS vazio.

## Fase 2: Parametros/Configuracao

- Objetivo: criar schema de parametros e carregar defaults equivalentes ao Python.
- Criterio: dump JSON/CSV dos defaults bate com `PARAMETER_INVENTORY.md`.
- Benchmark: nao aplicavel, exceto custo de parse.

## Fase 3: Mundo/Camera/Tempo Fixo

- Objetivo: mundo retangular/circular, camera, pan/zoom e dt fixo.
- Criterio: camera e bounds funcionam; headless roda N steps.
- Benchmark: loop vazio headless.

## Fase 4: Entidades Basicas

- Objetivo: `AgentStore` e `FoodStore` com posicao, raio, energia e cor.
- Criterio: spawn deterministico por seed.
- Benchmark: criar/remover 100, 1000, 10000 entidades.

## Fase 5: Renderizacao Simples

- Objetivo: render SFML de agentes/comida/substrato.
- Criterio: FPS medido com 100/1000/5000 entidades sem IA.
- Benchmark: render simples vs Python Pygame.

## Fase 6: Spatial Hash

- Objetivo: grid espacial com queries por raio.
- Criterio: queries corretas contra brute force em dataset pequeno.
- Benchmark: rebuild/query para 100, 1000, 10000 agentes.

## Fase 7: Comida/Energia/Interacao

- Objetivo: comer comida instantanea, energia e morte por energia.
- Criterio: agente ganha energia ao tocar comida; morte ocorre abaixo do limite.
- Benchmark: custo de interacao com/sem spatial hash.

## Fase 8: Locomocao

- Objetivo: movimento forward e quatro direcoes, giro, arrasto/inercia opcional.
- Criterio: outputs sinteticos movem agentes corretamente.
- Benchmark: movement update em lote.

## Fase 9: MLP

- Objetivo: MLP padrao com pesos, mutacao e forward.
- Criterio: forward numericamente plausivel, mutacao reproduzivel por seed.
- Benchmark: forward batch por agentes e tamanhos de rede.

## Fase 10: Sensores/Visao Simples

- Objetivo: retina single equivalente basica.
- Criterio: inputs por retina/canal coerentes em cenarios controlados.
- Benchmark: custo por agente/retina/canal.

## Fase 11: Visao por Setores Otimizada

- Objetivo: sector/bin vision com subdivisoes de distancia.
- Criterio: output tem mesmo tamanho da retina configurada; visual debug funciona.
- Benchmark: sector vs raycast.

## Fase 12: Reproducao/Mutacao

- Objetivo: split por energia, filho com mutacao.
- Criterio: populacao cresce sob comida suficiente.
- Benchmark: custo de nascimento/remocao.

## Fase 13: Predadores

- Objetivo: especie predadora por dieta, preservando aliases legados `Bacteria`/`Predator` sem criar classe rigida obrigatoria.
- Criterio: predador come presa; presa e predador podem coexistir.
- Benchmark: cenarios com predadores.

## Fase 14: UI Inicial

- Objetivo: Dear ImGui ou UI escolhida com menus essenciais.
- Criterio: alterar parametros basicos, pause/play, spawn/reset.
- Benchmark: custo de UI ligada/desligada.

## Fase 15: Save/Load/Export/Import

- Objetivo: `.biosim` versionado e export/import de genoma/substrato.
- Criterio: roundtrip preserva estado e parametros.
- Benchmark: tempo de salvar/carregar cenarios grandes.

## Fase 16: Metricas/Profiler/Diagnostico

- Objetivo: profiler, logs, metricas e graficos basicos.
- Criterio: relatorio por sistema e CSV/JSON de benchmark.
- Benchmark: overhead profiler on/off.

## Fase 17: Paridade Completa de UI

- Objetivo: cobrir menus, abas, ferramentas e preferencias.
- Criterio: checklist `UI_INVENTORY.md` revisado item a item.
- Benchmark: UI completa com simulacao.

## Fase 18: Otimizacao Data-Oriented

- Objetivo: reduzir alocacoes, melhorar cache, batch por assinatura, threading seletivo.
- Criterio: ganho medido sem perda funcional.
- Benchmark: cenarios 600, 1000, 2000+ agentes.

## Fase 19: Testes de Paridade e Benchmarks

- Objetivo: comparar Python vs C++ sistematicamente.
- Criterio: relatorio com tabelas, commits, parametros e seeds.
- Benchmark: plano completo de `BENCHMARK_PLAN.md`.

## Regra de Avanco

Nao avancar para a fase seguinte sem:

- criterio de conclusao cumprido;
- benchmark minimo quando aplicavel;
- revisao do usuario;
- commit separado para a fase.
