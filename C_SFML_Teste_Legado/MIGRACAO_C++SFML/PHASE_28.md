# Fase 28 — Save/Load/Export/Import/Autosave

Atue como arquiteto senior de software C++, especialista em serializacao versionada, compatibilidade
de formatos legados e persistencia determinista de simulacoes evolutivas.

Esta fase implementa a persistencia completa: salvar/abrir simulacao (`.biosim`), export/import de
substrato (JSON) e de genoma/agente (formato legado Python), autosave e recovery, preservando RNG,
camera, labels/especies e todos os tipos de cerebro.

## Regras invioláveis

1. Engine headless; a serializacao vive em camada neutra/engine; a UI so dispara comandos.
2. Determinismo: roundtrip (salvar -> carregar) reproduz exatamente o estado, incluindo RNG state.
3. Compatibilidade: o loader aceita aliases legados (Numba/Python) ja mapeados na migracao.
4. Nenhum Python alterado. Build Debug/Release limpos.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 27 concluidas e comitadas; selftests anteriores verdes. Os hooks de
crash/recovery foram preparados na Fase 27 (sem persistencia real) — agora ganham implementacao.

## Referencia de usabilidade (Python)

`sim/ui.py` (menus Arquivo: Salvar/Abrir/Salvar Como/Exportar/Importar), `sim/random_utils.py` (RNG
state), `PARAMETER_INVENTORY.md`. Usabilidade e formato sim; arquitetura nao.

## Escopo obrigatorio

1. **Formato `.biosim`** versionado: agentes (SoA), comida, obstaculos, especies/labels, genomas,
   cerebros (MLP/Gated/Shortcut/Modulated/RNN/NEAT, incluindo estado recorrente), camera, RNG state,
   parametros, metadados (versao, commit, data).
2. **Salvar / Abrir / Salvar Como** ligados aos itens de menu Arquivo (ja existentes como placeholder
   desde a Fase 22).
3. **Export/Import de substrato JSON** (paridade Python).
4. **Export/Import de genoma/agente** no formato legado, aceitando saves antigos do Python quando
   viavel (documentar limites).
5. **Autosave** periodico configuravel + **recovery on close** (oferecer recuperar ultimo estado).
6. **Versionamento e migracao de schema**: ao abrir um save mais antigo, migrar com seguranca ou
   recusar com mensagem clara.

## Fora de escopo

- Novas features fora do Python.
- Otimizacao de performance do save/load alem do razoavel (medir; otimizacao profunda e Fase 32).

## Testes obrigatorios — `--phase28-selftest`

1. Roundtrip `.biosim`: salvar -> carregar -> estado identico (agentes, energia, posicoes, RNG,
   camera, especies, cerebros) por seed fixa.
2. RNG state restaurado: continuar a simulacao apos load produz a mesma sequencia que sem save.
3. Cada tipo neural salva/carrega corretamente (inclui estado recorrente de RNN/NEAT recorrente).
4. Loader aceita aliases legados.
5. Export/import de substrato e de genoma roundtrip.
6. Autosave gera arquivo; recovery restaura.

## Diagnostics — `--phase28-diagnostics`

Tempo de save/load para mundos pequeno/medio/grande; tamanho do arquivo; 100/600/1000+ agentes;
seed, commit, build.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 27 + `--phase28-selftest`.
- Crie `PHASE_28_SAVE_LOAD_AUTOSAVE_STATUS.md`: escopo; layout do formato `.biosim` e versao;
  compatibilidade legada e limites; estrategia de migracao de schema; resultados; pendencias.

## Saida final esperada

Resumo executivo; arquivos criados/modificados; formato `.biosim` documentado; resultados de
build/selftests/diagnostics; decisao: pronto para commit ou correcao. Nao avance para a Fase 29 sem
autorizacao.
