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
10. Nao avance para a proxima etapa sem autorizacao explicita do usuario.

## O que e permitido nesta etapa atual

- Criar e revisar documentacao Markdown dentro de `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- Analisar Python atual.
- Planejar arquitetura, fases, riscos e benchmarks.

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

## Ordem Recomendada das Fases

- Fase 0: Auditoria/documentacao.
- Fase 1: Esqueleto C++/SFML/CMake.
- Fase 2: Parametros/configuracao.
- Fase 3: Mundo/camera/tempo fixo.
- Fase 4: Entidades basicas.
- Fase 5: Render simples.
- Fase 6: Spatial hash.
- Fase 7: Comida/energia/interacao.
- Fase 8: Locomocao.
- Fase 9: MLP.
- Fase 10: Sensores/visao simples.
- Fase 11: Visao por setores otimizada.
- Fase 12: Reproducao/mutacao.
- Fase 13: Predadores/especies por dieta.
- Fase 14: UI inicial.
- Fase 15: Save/load/export/import.
- Fase 16: Metricas/profiler/diagnostico.
- Fase 17: Paridade completa de UI.
- Fase 18: Otimizacao data-oriented.
- Fase 19: Testes de paridade e benchmarks.

## Documentos que Devem Ser Consultados

- Antes de mexer em parametros: `PARAMETER_INVENTORY.md`.
- Antes de mexer em UI: `UI_INVENTORY.md`.
- Antes de remover/substituir comportamento: `FEATURE_INVENTORY.md`.
- Antes de otimizar ou medir performance: `BENCHMARK_PLAN.md`.
- Antes de implementar modulo C++: `PROPOSED_CPP_ARCHITECTURE.md`.
- Antes de planejar fase: `MIGRATION_PHASES.md`.
- Antes de avaliar risco: `MIGRATION_RISKS.md`.

## Checklist Antes de Cada Nova Etapa

- A etapa foi autorizada explicitamente pelo usuario?
- O escopo esta pequeno e testavel?
- Existe criterio de conclusao?
- Existem parametros envolvidos? Consultar `PARAMETER_INVENTORY.md`.
- Existe UI envolvida? Consultar `UI_INVENTORY.md`.
- Existe comportamento que pode ser perdido? Consultar `FEATURE_INVENTORY.md`.
- Existe benchmark necessario? Consultar `BENCHMARK_PLAN.md`.
- A mudanca preserva engine headless?
- A mudanca tem commit separado planejado?

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
- Planejar Fase 1 em texto.
- Criar checklists de paridade mais detalhados.

## Proximos Passos Proibidos sem Autorizacao

- Criar C++.
- Criar CMake.
- Criar prototipo.
- Alterar Python.
- Migrar UI.
- Remover arquivos.
- Restaurar documentacao antiga apagada.

## Frase de Seguranca

Nao avance para a proxima etapa sem autorizacao explicita do usuario.
