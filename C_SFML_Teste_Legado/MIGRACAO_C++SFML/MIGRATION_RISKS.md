# Riscos da Migracao

## Tabela de Riscos

| Risco | Impacto | Probabilidade | Mitigacao | Como testar | Documento/checklist protetor |
|---|---|---:|---|---|---|
| Perda de parametros | Alto | Alta | Usar `PARAMETER_INVENTORY.md` e schema com aliases | Comparar dumps de config Python/C++ | `PARAMETER_INVENTORY.md` |
| Perda de funcionalidades da UI | Alto | Alta | Inventario completo e checklist por menu/aba | Revisao manual item a item | `UI_INVENTORY.md` |
| Divergencia comportamental Python vs C++ | Alto | Alta | Benchmarks de paridade com seed fixa | Comparar series de populacao/energia/eventos | `BENCHMARK_PLAN.md` |
| Regressao de performance | Alto | Media | Medir antes/depois em Release | Benchmark progressivo | `BENCHMARK_PLAN.md` |
| Reescrita excessiva sem validacao | Alto | Media | Fases pequenas e testaveis | Criterio de conclusao por fase | `MIGRATION_PHASES.md` |
| Acoplamento indevido UI-engine | Alto | Media | Engine headless sem dependencia de SFML/ImGui | Rodar benchmark sem janela | `PROPOSED_CPP_ARCHITECTURE.md` |
| Design C++ com vicios de Python | Alto | Media | Data-oriented para hot loop | Perfil de cache/wall time | `PROPOSED_CPP_ARCHITECTURE.md` |
| Perda de reprodutibilidade | Alto | Media | RandomService com seed e estado | Mesmo seed, contadores equivalentes | `BENCHMARK_PLAN.md` |
| Perda de save/load | Alto | Media | Schema versionado e roundtrip test | Salvar, abrir, comparar estado | `FEATURE_INVENTORY.md` |
| Falhas de CMake/build | Medio | Media | Fase inicial minima, CI/manual release | Build Debug/Release limpo | `MIGRATION_PHASES.md` |
| Complexidade excessiva inicial | Alto | Alta | MVP por fases, UI parcial primeiro | Fase conclui so com escopo fechado | `MIGRATION_PHASES.md` |
| Benchmarks mal definidos | Alto | Alta | Plano formal, seed, warm-up, repeticoes | Conferir JSON/CSV completos | `BENCHMARK_PLAN.md` |
| Otimizacao sem metrica objetiva | Alto | Alta | Sempre perfil antes/depois | Relatorio antes/depois | `BENCHMARK_PLAN.md` |
| Copiar hierarquia Bacteria/Predator rigidamente | Medio | Alta | Modelar especies/dietas | Criar especie presa/predador por config | `PROPOSED_CPP_ARCHITECTURE.md` |
| NEAT quebrar batch das MLPs | Alto | Media | Executors separados e fallback por tipo | Benchmark MLP com NEAT desligada | `PROPOSED_CPP_ARCHITECTURE.md` |
| Visualizacao debug pesar simulacao | Medio | Media | Debug data so quando solicitado | Benchmark com/sem debug | `BENCHMARK_PLAN.md` |
| Oclusao/obstaculo mudar aprendizado | Medio | Media | Preservar flags e modos | Teste com obstaculo conhecido | `FEATURE_INVENTORY.md` |
| Comida chunk ficar cara demais | Alto | Media | Sistemas opcionais e benchmark isolado | Cenarios chunk on/off | `BENCHMARK_PLAN.md` |
| UI incompleta atrasar uso real | Alto | Media | Priorizar paineis essenciais e paridade incremental | Checklist UI por fase | `UI_INVENTORY.md` |
| Incompatibilidade de arquivos antigos | Medio | Media | Loaders com aliases e migradores | Abrir saves reais copiados | `PARAMETER_INVENTORY.md` |
| Confusao terminologica entre Labels/Especies/Agente/Genoma/Bacteria/Predator | Alto | Alta | Manter glossario e aliases versionados | Abrir saves antigos e conferir UI | `CODEX_MIGRATION_GUIDE.md`, `UI_INVENTORY.md` |
| Funcionalidade inventariada sem fase executavel | Alto | Alta | Consultar `PLANNING_COVERAGE_AUDIT.md` antes de cada fase | Verificar se cada item tem fase de implementacao/validacao | `PLANNING_COVERAGE_AUDIT.md`, `MIGRATION_PHASES.md` |
| Redes neurais avancadas sem fase clara | Alto | Alta | Fases 14, 15 e 16 dedicadas | Testar Gated/Shortcut/Modulated/RNN/NEAT separadamente | `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md` |
| Redes neurais avancadas tratadas como detalhe tardio | Alto | Media | Fase 9 deve criar arquitetura neural extensivel | Revisar se MLP nao foi hardcoded no App/AgentStore | `PLANNING_COVERAGE_AUDIT.md` |
| MLP inicial implementada com arquitetura fechada | Alto | Media | `BrainType`, `BrainFactory`, `BrainConfig`, `BrainState` e `BrainExecutor` desde a Fase 9 | Adicionar outro tipo neural sem reescrever App/MovementSystem | `PROPOSED_CPP_ARCHITECTURE.md` |
| UI inventariada sem paridade por menu/ferramenta | Alto | Alta | Fases 22-25 e 29 quebradas por area | Checklist item a item de `UI_INVENTORY.md` | `UI_INVENTORY.md`, `MIGRATION_PHASES.md` |
| Parametro cadastrado mas nunca usado funcionalmente | Alto | Alta | Tabela de cobertura por parametros deve indicar fase de uso, UI e validacao | Para cada parametro comportamental, teste efeito no sistema | `PARAMETER_INVENTORY.md`, `PLANNING_COVERAGE_AUDIT.md` |
| Save/load/export/import deixados para o fim sem schema | Alto | Media | Fase 27 explicita com schema versionado e aliases | Roundtrip e abertura de saves/genomas legados | `PARAMETER_INVENTORY.md`, `MIGRATION_PHASES.md` |
| Benchmarks planejados sem fase real de execucao | Alto | Media | Fase 28 para runner formal e Fase 31 para campanha final | CSV/JSON/MD gerados com commit/build/seed | `BENCHMARK_PLAN.md`, `MIGRATION_PHASES.md` |

## Riscos Criticos Detalhados

### Perder parametros acumulados

O projeto tem muitos parametros com nomes historicos, aliases e ranges especificos. Perder um parametro pode mudar completamente o comportamento evolutivo.

Mitigacao:

- Criar `ParameterSchema` antes de implementar UI completa.
- Importar todos os defaults atuais.
- Manter aliases antigos.
- Testar roundtrip de config.

### Mudar a dinamica cientifica sem perceber

Tempo fixo, energia, reproducao, visao e colisao definem a pressao evolutiva. Pequenas mudancas podem fazer organismos aprenderem outro comportamento.

Mitigacao:

- Registrar ordem dos sistemas.
- Rodar paridade por seed.
- Aceitar tolerancias numericas, mas investigar divergencias grandes.

### Reescrever UI grande de uma vez

A UI atual e extensa e contem muito conhecimento operacional. Recriar tudo de uma vez aumenta risco de perda.

Mitigacao:

- Criar UI por paineis.
- Cada painel tem checklist.
- Primeiro suportar salvar/carregar parametros e rodar simulacao.

### Criar C++ orientado a objetos pesado

Copiar `Agent` como classe polimorfica por organismo pode nao entregar performance suficiente.

Mitigacao:

- Hot loop em SoA.
- Objetos apenas em bordas: UI, serializacao, editores.
- Benchmarks de `AgentStore` vs objeto por agente.

### Otimizar visao errada

A visao e gargalo central, mas tambem carrega significado biologico. Uma otimizacao que perde direcao, cor ou distancia pode acelerar e piorar aprendizado.

Mitigacao:

- Manter varios modos.
- Comparar desempenho e comportamento.
- Visualizacao debug por agente.

### Cobertura de planejamento insuficiente

Um item pode estar citado em inventario ou arquitetura e ainda assim nao estar coberto. Para ser considerado coberto, precisa ter fase executavel de implementacao, validacao ou pendencia explicita com fase futura.

Mitigacao:

- Consultar `PLANNING_COVERAGE_AUDIT.md` antes de iniciar qualquer fase.
- Toda fase futura deve listar quais funcionalidades, parametros e itens de UI cobre.
- Itens parcialmente cobertos devem permanecer visiveis ate a fase de validacao.

### Arquitetura neural fechada cedo demais

Se a Fase 9 implementar MLP diretamente no `App`, `AgentStore` ou `MovementSystem`, a migracao futura de Gated/Shortcut/Modulated/RNN/NEAT exigira reescrita ou quebrara performance.

Mitigacao:

- `BrainType`, `BrainFactory`, `BrainConfig`, `BrainState`, `BrainHandle` e `BrainExecutor` desde a Fase 9.
- Densas avancadas em Fase 14, RNN em Fase 15 e NEAT em Fase 16.
- Benchmarks devem provar que NEAT desligado nao adiciona custo ao caminho MLP.

### Parametro apenas cadastrado

Cadastrar parametro no `ParameterRegistry` nao preserva comportamento. Parametros de comportamento precisam ser usados por um sistema, expostos na UI quando aplicavel e validados.

Mitigacao:

- A tabela de cobertura por parametros em `PLANNING_COVERAGE_AUDIT.md` deve ser revisada antes de cada fase.
- Cada fase deve declarar parametros usados funcionalmente e parametros ainda pendentes.

## Regra de Seguranca

Nenhuma funcionalidade deve ser removida apenas porque parece antiga. Primeiro deve ser classificada como:

- preservada;
- redesenhada;
- substituida com comportamento equivalente;
- legado mantido para compatibilidade;
- removida apenas com autorizacao explicita do usuario.
