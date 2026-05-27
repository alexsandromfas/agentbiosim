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

## Regra de Seguranca

Nenhuma funcionalidade deve ser removida apenas porque parece antiga. Primeiro deve ser classificada como:

- preservada;
- redesenhada;
- substituida com comportamento equivalente;
- legado mantido para compatibilidade;
- removida apenas com autorizacao explicita do usuario.
