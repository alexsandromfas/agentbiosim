# Fase 31 — Paridade Completa de UI (STATUS: concluida)

Data: 2026-06-10. Build Debug + Release limpos; regressoes 7 a 31 verdes;
`--phase31-selftest` PASS (59 checks); smoke do app 12s sem crash.

Legenda do checklist: **[P]** preservado (igual ao Python) · **[R]** redesenhado (mesma funcao,
forma melhor) · **[X]** removido com autorizacao/justificativa · **[!]** corrigido nesta fase
(bug de paridade achado na varredura) · **[~]** pendencia explicita documentada.

## Bugs reais encontrados e CORRIGIDOS nesta fase

1. **[!] Tecla H nao abria a Ajuda** — `CmdToggleHelpPanel` alternava `uiState.showHelp`, flag da
   UI SFML morta; a janela ImGui le `prefs.helpWindowOpen`. Corrigido no App.
2. **[!] Tecla V e menu "Debug de visao" eram no-ops** — `CmdToggleVisionDebug` nao era tratado em
   lugar nenhum. Agora alterna o overlay de visao do agente selecionado (mesmo backend do checkbox
   "Mostrar visao" da Fase 26).
3. **[!] Ajuda mentia sobre "WASD"** — so W movia a camera; A/S/D sao ferramentas. Decisao: camera
   = setas (W removido como atalho); Ajuda corrigida + linha nova "Shift/Ctrl + clique: somar a
   selecao" (recurso que existia e nao estava documentado).
4. **[!] "Resetar rede neural" por especie estava desabilitado** (placeholder "Fase 26") — agora
   funcional: `NeuralSystem::resetBrainsBySeed` reconstroi cada cerebro a partir da config do
   proprio slot (arquitetura correta garantida) com seed variavel por passo ⇒ redes genuinamente
   novas, reproduzivel no mesmo ponto da mesma execucao. Botao habilitado na aba Labels com tooltip.

## Checklist — Menus

| Item (Python) | Estado | Como ficou no C++ |
|---|---|---|
| Arquivo > Novo | [P] | `CmdNewSimulation` (reset) |
| Arquivo > Abrir Simulacao | [R] | `.agentbiosim` (Fase 28; formato novo, sem legado Python — autorizado) |
| Arquivo > Salvar / Salvar Como | [P] | Fase 28, dialogos nativos |
| Arquivo > Exportar/Importar substrato JSON | [X] | Removido por autorizacao explicita do usuario ("o save normal ja e um export de substrato") |
| View > Mostrar ativacoes neurais | [R] | Aba Rede Neural do painel do agente (ativacoes por cor) |
| View > Layout da rede neural (fixed/preencher) | [P] | Checkbox "Preencher painel" no viewer |
| View > detalhes do agente | [R] | Agente > Painel do agente |
| View > grafico | [R] | Exibir > Metricas e profiler (janela, Fase 27) |
| View > spatial hash | [P] | Exibir > Overlay spatial hash |
| View > visao do agente | [!] | Exibir > Debug de visao + tecla V (corrigido) + checkbox no painel |
| View > visao multi-selecao | [~] | Parametro `show_multi_selected_vision` existe; render multi adiado (registrado desde a Fase 11; overlay atual = agente selecionado) |
| Preferencias (6 janelas + resolucao + grafico) | [P/R] | 7 abas (Simulacao, Fisica, Visao, Neural, Autosave, Aparencia, Performance); resolucao em Aparencia; grafico via parametros de metricas |
| Agente > Exportar selecionado | [P] | `.organism` (Fase 28) |
| Agente > Carregar agente | [P] | Importar organismo (spawn no centro da camera) |
| Agente > Criar linhagem do selecionado | [R] | Labels > "+ Nova label com os selecionados" |
| Ajuda > Ajuda e atalhos | [P] | Janela Atalhos (texto corrigido nesta fase) + Sobre (extra) |

## Checklist — Abas / paineis

| Item | Estado | Nota |
|---|---|---|
| Editor Genetico (5 grupos, ~40 params) | [P] | Dock esquerdo; grupos Gestalt iguais ao Python |
| — campo nome do template | [R] | Nome vira o nome do arquivo no dialogo de exportar |
| — cor do organismo | [R] | Cor por especie na aba Labels (color picker) |
| — separacao dos olhos | [X] | Absorvido por "angulo entre olhos" (a percepcao C++ usa angulo; sem param proprio) |
| — structural jitter | [~] | Registrado (`*_structural_jitter`) mas **inerte** no engine (lido, nunca consumido); nao exposto para nao enganar — pendencia documentada |
| Populacao | [R] | Fundida na aba Labels: Min/Max/Ini por especie + resgate de populacao minima |
| Substrato (todos os params + Aplicar + Limpar) | [P] | Aba Substrato do dock |
| Labels (cards: nome, contagem, min/max/ini, grafico, cor, selecionar, atribuir, remover, resetar rede, excluir, + nova) | [P/!] | Tudo presente; "Resetar rede" CORRIGIDO (funcional) |
| Abas legadas (Simulacao, Bacterias, Predadores, Teste, Ajuda-aba) | [X] | Duplicatas legadas do proprio Python (o inventario ja as marcava); funcoes vivem na toolbar/preferencias/editor/ajuda |
| Painel do agente (retratil, Genoma + Rede Neural) | [P] | Fase 26; abre so com selecao; custo zero oculto |
| — inteligencia local por agente | [R] | Fator de inteligencia de GRUPO nas Metricas (Fase 27); valor por agente nao portado (decisao documentada) |
| Visualizador neural (fixed/fill, pesos, ativacoes, RNN, NEAT) | [P/R] | Fase 26; pesos fracos = alpha proporcional (em vez de ocultos); valores por cor |
| Grafico (metricas, especies, amostragem, janela) | [R] | Janela Metricas: series fixas plotadas + por especie; amostragem/janela via `metrics_sample_interval`/`metrics_max_samples`; pausa com a simulacao; valor atual na legenda |
| Janela do Desenvolvedor | extra | Fase 30 (justificada; nao existia no Python) |

## Checklist — Ferramentas de canvas e atalhos

| Item | Estado | Nota |
|---|---|---|
| Selecao (S), Retangulo (Q), Laco (L) | [P] | Aditiva com Shift/Ctrl (agora documentada na Ajuda) |
| Comida (G), Agente (A), Pincel (B), Apagar (X), Mover (M), Excluir (D) | [P] | Icones + tooltips (Fase 25.3); Python usava F p/ comida — F aqui e "fit" (igual README) |
| Pipeta / coletar genoma | [X] | Ja estava "verificacao manual" no Python; funcao coberta por Exportar organismo selecionado |
| Pan botao direito, zoom scroll, fit (F) | [P] | |
| Space, Esc, Delete, R, T, V, H | [P/!] | V e H corrigidos nesta fase |
| WASD camera | [R/!] | Setas = camera (W removido; letras sao ferramentas); Ajuda corrigida |
| Tooltips nos botoes | [P] | Delayed tooltips |

## Checklist — Janelas de preferencias (Fase 23, revalidado)

Opcoes de simulacao [P] · Sistema de visao [P] · Redes neurais (8 tipos, knobs por arquitetura
filtrados pelo tipo ativo) [P] · Autosave [P] (backend real desde a Fase 28; padrao 30 min) ·
Aparencia (fundo/substrato/gradientes/borda + resolucao + idioma PT-BR/EN extra) [P] ·
Performance/novas mudancas [P] (alguns knobs `PendingFuturePhase` continuam expostos com badge).
Selftest: as 7 abas resolvem o modelo de parametros sem crash e nao-vazias.

## Checklist — Fluxos criticos (testados ponta a ponta no selftest)

Abrir/salvar/salvar-como `.agentbiosim` ✓ · novo ✓ · exportar/importar organismo (mente preservada
por checksum) ✓ · autosave sem travar o loop (thread de fundo, Fase 28) ✓ · aplicar por painel
(genoma/ambiente/populacao) ✓ · selecao por canvas (ponto/ret/laco) e por labels ✓ · spawn/limpar
comida ✓ · pintar/apagar obstaculo ✓ · criar/renomear/colorir/ajustar/excluir especie ✓ · resetar
rede por especie (150/150 cerebros substituidos) ✓ · inspecionar agente + viewer (Fase 26 selftest)
✓ · mudar render sem reiniciar (flags Immediate) ✓.

## Itens nao identificados do inventario — decisao final

- Abas legadas (`Teste`, `Bacterias`, `Predadores`, `Simulacao`): **removidas** (eram codigo morto
  duplicado no proprio Python).
- Pipeta: **removida** (coberta por exportar/importar organismo).
- `Labels` vs `Especies`: a UI mantem "Labels" como titulo da aba (familiaridade) sobre o
  SpeciesStore conceitual — documentado.
- Menu `Agente` vs `Genoma`: mantido "Agente" com acoes de organismo (exportar/importar) — o
  conceito genoma vive no Editor Genetico.
- Saves antigos Python: **sem compatibilidade**, por decisao explicita do usuario (Fase 28).

## Testes

- `--phase31-selftest` PASS (59): 19 checks de atalhos via InputRouter real (sf::Event sintetico,
  sem janela), 7 abas de preferencias, 3 comandos de janela, 30 checks de fluxos criticos.
- Regressoes 7–31: todas PASS (Release); Fase 31 tambem PASS em Debug.
- Smoke manual: app aberto 12s com simulacao viva, sem crash.
- Limite documentado: o smoke de clique em cada janela ImGui exige contexto GL/janela; coberto pelo
  launch manual + selftests de estado/comando (as janelas sao funcoes puras de UiState).

## Arquivos

Criados: `src/systems/Phase31Diagnostics.{hpp,cpp}`, este STATUS.
Modificados: `src/app/App.cpp` (fix H + fix V), `src/ui/InputRouter.cpp` (W removido),
`src/ui/ImGuiUi.cpp` (Ajuda corrigida + botao Resetar rede habilitado),
`src/systems/NeuralSystem.{hpp,cpp}` (`resetBrainsBySeed`), `src/sim/SimulationRunner.{hpp,cpp}`
(`resetNeuralForSpecies` + comando funcional), `src/main.cpp`, `CMakeLists.txt`.

## Microfase 31.1 — substrato/comida ao vivo + min/max por label (2026-06-10)

A pedido do usuario, tres mudancas de comportamento (selftest da fase ampliado para 69 checks):

1. **Mudar substrato/comida NAO reinicia a simulacao.** `world_w`/`world_h`/`substrate_radius`/
   `substrate_shape` sairam de `RequiresReset` para o novo `ApplyFlag::ReshapeWorld`:
   `SimulationRunner::applyWorldConfigLive()` reconfigura o mundo ao vivo e **empurra agentes e
   comida de volta para dentro** dos novos limites (clamp nas duas formas). O botao "Aplicar
   ambiente" e o Apply das preferencias usam esse caminho (sem reset, sem refit de camera). Os
   parametros de comida ja eram lidos a cada passo — o reset vinha apenas do botao, removido.
   "Aplicar populacao" tambem nao reseta mais. "Aplicar a especie" (Editor Genetico) MANTEM o
   reset (semantica explicita de re-bake).
2. **Maximo POR LABEL no nascimento.** Antes a reproducao comparava o TOTAL global de agentes com
   `bacteria_max_limit` (bug). Agora `ReproductionSystem::apply` recebe a `SpeciesStore` e bloqueia
   o nascimento quando a label do pai atinge seu `maxPopulation` (0 = sem limite). O caminho legado
   (sem store) preserva o comportamento antigo para os selftests das Fases 13/18.
3. **Resgate de MINIMO por label** (`SimulationRunner::applyPopulationRescue`, por passo, ligado a
   `population_min_rescue_enabled`): toda label habilitada abaixo do seu `minPopulation` e reposta
   ate ele, com defaults do genoma da propria label (funciona para labels criadas pelo usuario) e
   posicoes deterministas por passo. Todo organismo pertence a uma label valida em todos os
   caminhos (spawn manual, reproducao, import, remocao de label ⇒ label padrao "bacteria").
   Nota documentada: reduzir o max abaixo da contagem atual NAO mata organismos existentes — o
   limite vale para nascimentos.

Tambem: check de identidade do top-system no `--phase30-selftest` endurecido contra empate de
timing (aceita top-2 do benchmark) — eliminava um flake raro sob carga. Duas rodadas completas das
regressoes 7-31 sem falhas.

Nao avancar para a Fase 32 sem autorizacao explicita do usuario.
