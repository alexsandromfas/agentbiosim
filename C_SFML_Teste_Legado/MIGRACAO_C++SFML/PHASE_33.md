# Fase 33 — Campanha Final de Paridade + Prova de Performance Python vs C++

Atue como arquiteto senior de software C++, engenheiro de performance e responsavel pelo gate final
da migracao. Esta e a fase de FECHAMENTO: valida a migracao completa contra o Python e PROVA, com
numeros reproduziveis, que a versao C++ e mais rapida em escala. Fecha a Divida 10.

## Objetivo duplo

1. **Paridade**: nenhum item dos inventarios (`FEATURE_INVENTORY`, `PARAMETER_INVENTORY`,
   `UI_INVENTORY`) fica sem decisao; diferencas conhecidas documentadas.
2. **Prova de performance**: comparativo C++ vs Python lado a lado, em 1000+, 2000+, 5000+ agentes,
   mostrando o speedup do C++ com relatorio reproduzivel. Esta e a promessa central da migracao.

## Regras invioláveis

1. Sem novas features. So validacao, medicao e relatorio.
2. Comparacao justa: mesmos cenarios, mesma seed conceitual, mesma carga; documentar diferencas
   metodologicas (ex.: Python com/sem Numba).
3. Nenhum Python alterado (pode-se EXECUTAR o Python para medir, nunca modifica-lo). Build
   Debug/Release; medir em Release.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 32 concluidas e comitadas; benchmark runner (Fase 29) e otimizacoes
(Fase 32) prontos; selftests verdes.

## Referencia

`BENCHMARK_PLAN.md` (plano completo), `PLANNING_COVERAGE_AUDIT.md`, todos os inventarios, e o
programa Python como alvo de comparacao (executado, nao modificado).

## Escopo obrigatorio

1. **Paridade por seed**: cenarios controlados comparando comportamento C++ vs Python (dentro das
   diferencas conhecidas/documentadas).
2. **Saves/genomas reais**: roundtrip com saves de verdade; compatibilidade legada confirmada.
3. **UI completa**: checklist da Fase 31 revisado uma ultima vez.
4. **Benchmarks comparativos C++ vs Python**: rodar o plano completo de `BENCHMARK_PLAN.md` nas duas
   implementacoes; medir us/step e throughput por cenario (100 a 5000+ agentes; visao single/raycast/
   sector; MLP/RNN/NEAT).
5. **Relatorio final de performance**: tabelas de speedup por cenario, gargalos restantes,
   recomendacoes; metadados de commit/build/maquina.

## Fora de escopo

- Implementar features novas.
- Otimizacoes adicionais alem de ajustes triviais (otimizacao foi a Fase 32).

## Testes obrigatorios — `--phase33-selftest`

1. Suite final por seed passa.
2. Roundtrip de saves passa.
3. Checklist de UI revisado.
4. Comparativo C++ vs Python gera numeros por cenario; o speedup do C++ e calculado e reproduzivel.

## Build, regressoes, documentacao

- Build Debug/Release (medir em Release); regressoes Fases 7 a 32 + `--phase33-selftest`.
- Crie `PHASE_33_FINAL_PARITY_PERFORMANCE_STATUS.md`: estado de paridade (item a item, sem pendencia
  sem decisao); diferencas conhecidas; **relatorio comparativo C++ vs Python** com tabelas de speedup
  por cenario; confirmacao de que a Divida 10 esta fechada (ou, se algum gargalo persistir, registrar
  honestamente com plano). Atualize `TECHNICAL_DEBT_REGISTER.md` fechando a Divida 10.

## Saida final esperada

Resumo executivo; estado de paridade; **tabela de speedup C++ vs Python**; gargalos restantes;
veredito do gate de migracao (migracao funcional comprovada ou pendencias); decisao: pronto para
commit final ou correcao. Esta e a fase de conclusao da migracao.
