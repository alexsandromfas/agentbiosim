# Guia Permanente para Proximas Sessoes do Codex

## Objetivo

Migrar o AgentBioSim Python para uma versao C++/SFML/CMake mais rapida, organizada e escalavel, preservando todas as funcionalidades atuais e melhorando a arquitetura. A versao Python atual e a referencia funcional.

## Regras Obrigatorias

1. Nao alterar codigo Python sem autorizacao explicita.
2. Nao remover parametros, menus, abas, botoes, ferramentas ou comportamentos sem autorizacao explicita.
3. Nao copiar literalmente a arquitetura Python para C++.
4. Usar arquitetura C++ data-oriented no hot loop.
5. Manter engine headless independente de render/UI.
6. Criar benchmarks antes de otimizar.
7. Medir antes/depois de qualquer otimizacao.
8. Preservar compatibilidade de saves/genomas sempre que viavel.
9. Consultar os inventarios antes de modificar parametros, UI ou comportamento.
10. Consultar `PLANNING_COVERAGE_AUDIT.md` antes de iniciar qualquer fase futura.
11. A partir da Fase 9, toda fase deve citar quais itens de `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md` e `UI_INVENTORY.md` ela cobre.
12. Nao considerar a migracao funcionalmente completa enquanto houver itens classificados como nao cobertos em `PLANNING_COVERAGE_AUDIT.md`.
13. A arquitetura neural deve ser extensivel desde a Fase 9.
14. Redes neurais avancadas nao podem ser encaixadas depois de forma improvisada.
15. Fases ja comitadas de 0 a 8 nao devem ser reescritas, salvo bugfix ou fase futura documentada.
16. Nao avance para a proxima etapa sem autorizacao explicita do usuario.

## O que e permitido nesta etapa atual

- Criar e revisar documentacao Markdown dentro de `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- Analisar Python atual.
- Planejar arquitetura, fases, riscos e benchmarks.
- Auditar cobertura de planejamento da Fase 9 em diante.

## O que e proibido nesta etapa atual

- Criar `.cpp`.
- Criar `.hpp`.
- Criar `CMakeLists.txt`.
- Criar pasta `src/`.
- Criar prototipos C++.
- Criar executaveis.
- Modificar arquivos Python.
- Mover arquivos existentes.
- Restaurar o TXT antigo apagado pelo usuario.
- Iniciar a Fase 9 sem autorizacao explicita.

## Estado Atual Oficial

- Fase 0 concluida.
- Fases 1, 2, 3, 4, 5, 6, 7 e 8 implementadas e comitadas.
- A proxima fase de implementacao e a Fase 9, mas ela ainda nao deve comecar sem autorizacao explicita.
- O planejamento futuro foi auditado em `PLANNING_COVERAGE_AUDIT.md`.
- As fases 0 a 8 devem ser tratadas como historico concluido, nao como pendencia.

## Fonte de Verdade Python

Arquivos principais:

- `README.md`
- `main.py`
- `sim/controllers.py`
- `sim/engine.py`
- `sim/entities.py`
- `sim/sensors.py`
- `sim/brain.py`
- `sim/actuators.py`
- `sim/systems.py`
- `sim/spatial.py`
- `sim/render.py`
- `sim/game.py`
- `sim/ui.py`
- `sim/neural_viewer.py`
- `sim/obstacles.py`
- `sim/world.py`
- `sim/fast_kernels.py`
- `sim/soa.py`
- `sim/profiler.py`
- `sim/diagnostics.py`
- `sim/intelligence.py`
- `sim/random_utils.py`

## Historicos/Legados

- `C_SFML_Teste_Legado` contem prototipos C++/SFML e builds de teste. Usar apenas como contexto secundario.
- `AgentBioSim_Cpp_Architecture_Guide.txt` foi apagado de proposito pelo usuario. Nao restaurar.

## Terminologia Operacional

- `Labels` e o termo ainda encontrado no codigo/UI atual para grupos de organismos.
- `Especies` e o destino conceitual recomendado para a nova arquitetura, mas nao deve substituir `Labels` sem uma fase explicita de compatibilidade.
- `Bacteria` e `Predator` sao tipos concretos/legados no Python atual. Na arquitetura C++ eles devem virar especies/dietas configuraveis, mantendo loader/aliases para saves antigos.
- `Agente` e o termo atual de menu/operacao em algumas partes da UI. `Genoma` e o destino conceitual para import/export de configuracao heredavel e cerebro.
- Qualquer renomeacao deve preservar compatibilidade de UI, save/load, parametros e documentacao.

## Arquitetura C++ Proposta

- Engine headless.
- Stores data-oriented/SoA.
- Sistemas separados.
- SFML para renderizacao.
- Dear ImGui recomendado para UI tecnica.
- Parametros via schema versionado.
- Persistencia `.biosim` versionada.
- Percepcao/visao como modulo critico.
- Redes neurais com executores por tipo.
- Benchmarks e profiler desde cedo.
- Arquitetura neural extensivel desde a Fase 9:
  - `BrainType`;
  - `BrainFactory`;
  - `BrainConfig`;
  - `BrainState`;
  - `BrainExecutor`;
  - `BrainHandle`;
  - batch para redes densas;
  - fallback individual/grupo por topologia para NEAT;
  - estado recorrente por agente;
  - `ActivationTrace` para visualizador neural;
  - serializacao neural versionada.

## Ordem Recomendada das Fases

- Fases 0 a 8: concluidas e comitadas.
- Fase 9: MLP inicial com arquitetura neural extensivel.
- Fase 10: Sensores, canais de retina e visao single.
- Fase 11: Visao fullbody/raycast e debug visual de visao.
- Fase 12: Visao sector/bins otimizada.
- Fase 13: Reproducao, mutacao base e genoma.
- Fase 14: Redes densas avancadas: Gated, Shortcut e Modulated MLP.
- Fase 15: RNN simples.
- Fase 16: Familia NEAT: comum, simplificada e recorrente.
- Fase 17: Especies, labels e genomas.
- Fase 18: Predacao e dieta generica.
- Fase 19: Comida chunk/pedacos completa.
- Fase 20: Obstaculos e oclusao.
- Fase 21: Colisoes e fisica opcional.
- Fase 22: UI base, menus e canvas.
- Fase 23: UI de parametros e preferencias.
- Fase 24: Editor genetico, especies e substrato.
- Fase 25: Agente selecionado e visualizador neural.
- Fase 26: Metricas, inteligencia, logs, diagnostico e profiler.
- Fase 27: Save/load/export/import/autosave.
- Fase 28: Benchmark runner e experimentos headless formais.
- Fase 29: Paridade completa de UI.
- Fase 30: Otimizacao data-oriented e escala.
- Fase 31: Campanha final de paridade Python vs C++.

## Documentos que Devem Ser Consultados

- Antes de mexer em parametros: `PARAMETER_INVENTORY.md`.
- Antes de mexer em UI: `UI_INVENTORY.md`.
- Antes de remover/substituir comportamento: `FEATURE_INVENTORY.md`.
- Antes de otimizar ou medir performance: `BENCHMARK_PLAN.md`.
- Antes de implementar modulo C++: `PROPOSED_CPP_ARCHITECTURE.md`.
- Antes de planejar fase: `MIGRATION_PHASES.md`.
- Antes de avaliar risco: `MIGRATION_RISKS.md`.
- Antes de iniciar fase futura: `PLANNING_COVERAGE_AUDIT.md`.

## Checklist Antes de Cada Nova Etapa

- A etapa foi autorizada explicitamente pelo usuario?
- O escopo esta pequeno e testavel?
- Existe criterio de conclusao?
- A fase esta vinculada aos itens do `FEATURE_INVENTORY.md` que cobre?
- A fase esta vinculada aos grupos do `PARAMETER_INVENTORY.md` que cadastra, usa, expoe ou valida?
- A fase esta vinculada aos itens do `UI_INVENTORY.md` que implementa ou valida?
- A fase resolve algum item nao coberto ou parcialmente coberto em `PLANNING_COVERAGE_AUDIT.md`?
- Existem parametros envolvidos? Consultar `PARAMETER_INVENTORY.md`.
- Existe UI envolvida? Consultar `UI_INVENTORY.md`.
- Existe comportamento que pode ser perdido? Consultar `FEATURE_INVENTORY.md`.
- Existe benchmark necessario? Consultar `BENCHMARK_PLAN.md`.
- A mudanca preserva engine headless?
- A mudanca tem commit separado planejado?
- Para Fase 9 ou neural: a mudanca preserva `BrainType`, factory, executor por tipo e suporte futuro a RNN/NEAT?

## Criterios de Conclusao de Uma Etapa

- Escopo implementado sem expandir para fase seguinte.
- Build/teste/benchmark aplicavel executado.
- Resultado documentado.
- Sem alteracoes fora do escopo.
- Sem perda funcional nao autorizada.
- Commit separado, quando o usuario pedir.

## Proximos Passos Permitidos

- Revisar estes documentos.
- Corrigir inventarios se o usuario encontrar ausencia.
- Planejar a Fase 9 em texto.
- Criar checklists de paridade mais detalhados.
- Auditar cobertura de planejamento.

## Proximos Passos Proibidos sem Autorizacao

- Criar C++.
- Criar CMake.
- Criar prototipo.
- Alterar Python.
- Migrar UI.
- Remover arquivos.
- Restaurar documentacao antiga apagada.
- Reabrir ou renumerar Fases 0 a 8 como se estivessem pendentes.
- Implementar Fase 9 sem autorizacao explicita.

## Frase de Seguranca

Nao avance para a proxima etapa sem autorizacao explicita do usuario.
