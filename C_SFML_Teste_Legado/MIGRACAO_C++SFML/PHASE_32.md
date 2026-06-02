# Fase 32 — Otimizacao Data-Oriented e Escala

Atue como arquiteto senior de software C++, engenheiro de performance e especialista em design
data-oriented, hot loops, alocacao zero, SIMD, threading e cache, para uma simulacao evolutiva de
alta escala.

Esta fase OTIMIZA, guiada por dados: usa o profiler (Fase 27), o benchmark runner (Fase 29) e a
Janela do Desenvolvedor (Fase 30) para atacar os gargalos REAIS, sem alterar comportamento. Ataca a
Divida 5 (alocacoes no NeuralSystem) e avanca a Divida 10 (prova de performance).

## Regras invioláveis

1. **Nao otimizar sem benchmark**: cada mudanca precisa de numero antes/depois (us/step) que prove o
   ganho. Use os cenarios da Fase 29 e a Janela da Fase 30.
2. **Zero regressao funcional**: mesma seed -> mesmo comportamento/estado (regressao por seed
   obrigatoria). Otimizacao nao pode mudar resultados.
3. Engine headless; determinismo e dt fixo preservados. Nenhum Python alterado. Build Debug/Release.

## Pre-requisito tecnico obrigatorio: Divida 5

Resolver a Divida 5 de `TECHNICAL_DEBT_REGISTER.md`: substituir as alocacoes temporarias de
`std::vector<double>` por agente no `NeuralSystem` (`produceMovementControls`,
`temporaryInputForAgent`) por buffers persistentes pre-alocados por assinatura neural. Se a Fase 30
ja apontou outro gargalo maior, ataca-lo primeiro, sempre guiado pelo numero.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 31 concluidas e comitadas; profiler/benchmark/Janela do Desenvolvedor
disponiveis; baseline de performance coletado (Fase 29/30); selftests verdes.

## Referencia

`BENCHMARK_PLAN.md`, `PROPOSED_CPP_ARCHITECTURE.md`, status das Fases 29 e 30 (gargalos observados).

## Escopo obrigatorio (somente o que o benchmark justificar)

1. **Buffers persistentes** pre-alocados por assinatura neural (Divida 5); alocacao zero no hot loop.
2. **Batch por assinatura** para redes densas (MLP/Gated/Shortcut/Modulated/RNN), fallback individual
   para NEAT.
3. **Threading seletivo** onde o profiler mostrar ganho real e seguro (sem quebrar determinismo).
4. **SIMD** apenas onde medido e justificado.
5. **Cache neural por grupo** quando aplicavel.
6. Revisar outros gargalos apontados pela Fase 30 (percepcao sector, spatial hash, render).

## Fora de escopo

- Mudancas de comportamento/feature.
- Otimizacoes especulativas sem medicao.
- A campanha comparativa final C++ vs Python (Fase 33).

## Testes obrigatorios — `--phase32-selftest`

1. Regressao por seed: estado identico ao baseline para os mesmos parametros/seed.
2. Comparacao de metricas: series principais inalteradas.
3. Cada otimizacao tem numero antes/depois registrado (ganho comprovado).

## Diagnostics/benchmark — obrigatorio antes/depois

Rodar os cenarios da Fase 29 antes e depois de cada otimizacao: 600, 1000, 2000, 5000+ agentes.
Mostrar o ganho tambem na Janela do Desenvolvedor (Fase 30). Seed, commit, build.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 31 + `--phase32-selftest`.
- Crie `PHASE_32_OPTIMIZATION_SCALE_STATUS.md`: gargalos atacados; tecnica usada em cada um; ganho
  medido antes/depois por cenario; como a Divida 5 foi resolvida; o que avancou na Divida 10; o que
  fica para a campanha final (Fase 33).

## Saida final esperada

Resumo executivo; arquivos criados/modificados; tabela de ganhos antes/depois; confirmacao de zero
regressao por seed; decisao: pronto para commit ou correcao. Nao avance para a Fase 33 sem
autorizacao.
