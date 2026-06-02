# Fase 31 — Paridade Completa de UI

Atue como arquiteto senior de software C++, designer senior de UI/UX e revisor de paridade de
features, fechando o checklist completo de `UI_INVENTORY.md` sobre a UI Dear ImGui.

Esta fase NAO inventa features novas: ela varre o `UI_INVENTORY.md` item a item e garante que a UI
ImGui (Fases 25, 26, 30) cobre tudo que o Python tinha, classificando cada item como preservado,
redesenhado ou removido com autorizacao.

## Regras invioláveis

1. Engine headless; UI desacoplada por comandos; tema da Fase 25.
2. Nenhuma feature do Python "esquecida" sem decisao explicita e documentada.
3. Nenhum Python alterado. Build Debug/Release limpos. Determinismo preservado.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 30 concluidas e comitadas; UI em ImGui completa; selftests verdes.

## Referencia

`UI_INVENTORY.md`, `FEATURE_INVENTORY.md`, `sim/ui.py`. Usabilidade do Python como gabarito.

## Escopo obrigatorio

1. **Varredura item a item de `UI_INVENTORY.md`**: para cada menu, aba, botao, slider, spinbox,
   checkbox, combo, atalho, ferramenta de canvas e fluxo critico, confirmar presenca e funcionamento
   na UI ImGui.
2. **Classificacao** de cada item: preservado (igual ao Python), redesenhado (melhor, mas mesma
   funcao), ou removido com autorizacao (justificado e aprovado).
3. **Atalhos**: revisar todos os atalhos de teclado e garantir consistencia e presenca na Ajuda.
4. **Fluxos criticos**: testar de ponta a ponta os fluxos principais (criar/editar especie, aplicar
   genoma, pintar/limpar substrato e comida, salvar/abrir, inspecionar agente, etc.).
5. **Checklist final** num documento, marcando cada item como OK/redesenhado/removido.

## Fora de escopo

- Features inexistentes no Python (exceto as ja justificadas em fases anteriores, ex.: Janela do
  Desenvolvedor).
- Otimizacao de performance (Fase 32).

## Testes obrigatorios — `--phase31-selftest`

1. Smoke de cada janela/painel (abre/fecha sem crash).
2. Cada comando do `UI_INVENTORY` mapeado a um comando/efeito verificavel (headless onde possivel).
3. Atalhos essenciais disparam os comandos certos.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 30 + `--phase31-selftest`.
- Crie `PHASE_31_UI_PARITY_STATUS.md` contendo o **checklist completo** de `UI_INVENTORY.md` com a
  classificacao de cada item, divergencias contra o Python, e a lista de itens removidos com a
  justificativa/autorizacao.

## Saida final esperada

Resumo executivo; checklist de paridade preenchido; itens redesenhados/removidos; resultados de
build/selftests; decisao: pronto para commit ou correcao. Nao avance para a Fase 32 sem autorizacao.
