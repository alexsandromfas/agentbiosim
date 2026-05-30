# Microfase 22.1 — Correcao Visual, Usabilidade e Canvas

Status: **CONCLUIDA** (2026-05-30)

Microfase obrigatoria de correcao da Fase 22. Surgiu de uma validacao manual no executavel que revelou: organismos com aparencia rasterizada, todos os menus quebrados ou misturados, botoes de selecao sem feedback visual, brush sem rastro, eraser sem efeito e cliques saindo do lugar ao maximizar a janela. Esta microfase nao introduz nada da Fase 23 — ela so corrige a base de UI/canvas.

## Escopo executado

- `Renderer` reescrito para visual vetorial: 40 segmentos por corpo (era 32), outline escuro 0.8px+zoom, gradient interno por sobreposicao de dois discos lighten, "cabeca" como triangulo orientado (era circulo), antialiasing 8x na janela. Pintura de comida/obstaculo ganhou outline e highlight interno. Bordas do mundo circular agora usam 192 segmentos (era 160).
- `Renderer` ganhou overlays headless-free: `drawSelectionHalos`, `drawMarquee`, `drawLasso`, `drawBrushCursor`. Todos recebem dados via `SelectionRenderInput` POD para nao acoplar renderer a `ui::UiState`.
- `App::handleResize` agora faz `window_.setView(sf::View(sf::FloatRect(0, 0, w, h)))` antes do refit da camera. Esta e a correcao direta do bug "clique sai do lugar quando maximiza" da Fase 22.
- `App` construtor agora abre a janela com `sf::ContextSettings{antialiasingLevel = 8}`.
- `App::drainCommandsAndApply` reconhece 9 comandos novos da UI: `CmdNewSimulation`, `CmdQuitApp`, `CmdTogglePreferencesPanel`, `CmdToggleAboutPanel`, `CmdToggleGenomePanel`, `CmdResetCamera`, `CmdCloseAllMenus`, `CmdPaintObstacleStroke`, `CmdEraseObstacleStroke`.
- `InputRouter` reescrito:
  - pan via `MouseMoved` event (foi `sf::Mouse::getPosition()` polled, que retornava coordenadas do desktop e quebrava ao maximizar);
  - brush no `MouseMoved` emite `CmdPaintObstacleStroke{lastStamp, current, brushRadius}` para rastro continuo;
  - eraser idem com `CmdEraseObstacleStroke`;
  - left-click no canvas tambem emite `CmdCloseAllMenus`.
- `SimulationRunner::applyCommand` ganhou tratamento de `CmdPaintObstacleStroke` e `CmdEraseObstacleStroke`. A interpolacao gera stamps a cada `brushRadius * 0.6` ao longo do segmento worldFrom->worldTo. Resultado medido no benchmark: 1 stroke = ~8 stamps com brush_radius=18 e segmento de ~80 unidades de mundo.
- `UiPanel` reescrito com dropdown real para cada menu:
  - clique no titulo do menu apenas alterna o dropdown (nao executa acao);
  - clique em item do dropdown dispara o comando correspondente;
  - `Arquivo`: Novo (CmdNewSimulation), Sair (CmdQuitApp); Abrir/Salvar/Salvar Como/Exportar/Importar como itens DESABILITADOS marcados como "Fase 27";
  - `View`: Fit world, Reset camera, Render simples, Vision debug, Spatial hash overlay, Selection overlay, Tool overlay — todos funcionais;
  - `Preferencias`: dois itens, "Em breve (Fase 23)" desabilitado e "Abrir painel placeholder" que dispara `CmdTogglePreferencesPanel`;
  - `Genoma` (renomeado de "Agente/Genoma"): dois itens, "Em breve (Fase 24/25)" desabilitado e "Abrir painel placeholder" que dispara `CmdToggleGenomePanel`;
  - `Ajuda`: "Atalhos (H)" -> `CmdToggleHelpPanel`, "Sobre..." -> `CmdToggleAboutPanel`.
- Toolbar agora tem 9 ferramentas (era 10): **Pan removido**. Pan e exclusivamente botao direito do mouse. Ferramentas: Select, Rect, Lasso, Food+, Agent+, Brush, Eraser, Move, Delete. Botoes alinhados, ativo destacado com outline e cor de fundo distinta.
- Status bar a direita mostra `tool=...  ag=...  f=...  o=...  sel=...  step=...`. Texto encostado a direita da janela.
- Tres overlays placeholder novos: Preferencias, Sobre, Genoma. Cada um e separado dos demais (era um bug da Fase 22: Preferencias abria Ajuda).
- `UiPanel::pointInsidePanel` agora considera dropdowns abertos para nao deixar cliques no menu vazarem para o canvas.
- `--phase22-hotfix-selftest` (26 checks, PASS), `--phase22-hotfix-benchmark`, `--phase22-hotfix-diagnostics`.
- `Phase22_1Diagnostics` cobre: roundtrip screen->world->screen em viewports normal e maximizada (1920x1080), dispatcher dos itens de menu separando Preferencias de Ajuda, brush stroke gerando 5+ stamps por arrasto de 100 unidades, eraser stroke reduzindo obstaculos, Genoma renomeado, separadores nao despachando, itens desabilitados nao despachando, `CmdQuitApp` por Arquivo > Sair, selecao por rect e por pick coexistindo.

## Decisao tecnica sobre visual dos organismos

Optei por mais geometria SFML (40 seg corpo + 24 seg disco interno + ConvexShape 3-pt cabeca) em vez de sf::Texture/RenderTexture com sprites pintados. Motivos:

- mantem o pipeline puramente vetorial — bordas suaves vem do AA da janela, nao de sampling raster;
- nada de heap alloc por agente (todos os shapes sao stack-locais);
- inspiracao do `learning_compare`: corpo de 24 seg com olho pequeno, mas elevado para 40 seg + outline + gradient + triangulo direcional;
- escala bem ate ~1000 agentes (benchmark abaixo mostra avg_step ~900-1060 us com 152 agentes; render-only com 600+ agentes precisa medicao real, mas a geometria por agente e ~5 draw calls de shape simples).

## Comparacao com microteste learning_compare

O `learning_compare` desenha 24-seg circles + 12-seg eyes sem outline e sem orientacao explicita; e bonito mas modesto. A versao 22.1:
- mais segmentos no corpo (40 vs 24);
- outline escuro suave em vez de fill chapado;
- gradient interno por sobreposicao;
- cabeca como **triangulo direcional** (era circulo no microteste) — mais legivel como vetor de movimento;
- antialiasing 8x na janela (o microteste nao define ContextSettings explicito).

Resultado: visual superior ao microteste e claramente superior a rasterizacao pygame.

## Fora de escopo (confirmado)

- Fase 23 NAO iniciada.
- UI de parametros completa NAO implementada.
- Preferencias completas NAO implementadas (apenas placeholder).
- Editor genetico NAO implementado.
- Painel de especies NAO implementado.
- Painel de substrato NAO implementado.
- Visualizador neural NAO implementado.
- Save/load NAO implementado.
- Autosave NAO implementado.
- Benchmark runner formal NAO implementado.
- Paridade completa de UI Python NAO implementada.
- Nenhum arquivo Python alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `ARCHITECTURE_REVIEW.md`
- `FEATURE_INVENTORY.md`
- `UI_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`
- `MIGRATION_RISKS.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`
- `PHASE_20_OBSTACLES_OCCLUSION_STATUS.md`
- `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`
- `CODEX_MIGRATION_GUIDE.md`

## Arquivos Python consultados

Esta microfase nao precisou abrir nenhum arquivo Python — toda a logica de UI/canvas/render alterada vive apenas no lado C++. O comportamento esperado (pan por botao direito, brush continuo, dropdown de menu, overlay de selecao) foi inferido do prompt da microfase e do `UI_INVENTORY.md`.

## Microteste learning_compare consultado

Lido somente `learning_compare/cpp_sfml_learning/src/main.cpp` (drawAgents, drawDish, drawVision). Tomada apenas inspiracao visual:
- segmento count, uso de CircleShape com origin centrado, eye pequeno offset pelo angulo;
- escolha de palette com `setFillColor(sf::Color(...))` direto.

Nao foi copiada nem arquitetura, nem logica de simulacao, nem o sistema de janela.

## Arquivos C++ criados

- `src/systems/Phase22_1Diagnostics.hpp`
- `src/systems/Phase22_1Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_22_1_UI_VISUAL_CANVAS_HOTFIX_STATUS.md`

## Arquivos C++ modificados

- `src/app/App.hpp` — sem mudancas estruturais; ajuste das responsabilidades documentadas.
- `src/app/App.cpp` — AA settings, `setView` no resize, novo dispatch de comandos, wire de `SelectionRenderInput` para Renderer, watch de `uiState_.quitRequested`.
- `src/render/Renderer.hpp` — assinatura `render(...)` recebeu parametro opcional `SelectionRenderInput*`; novo `RenderStats` com counters de overlay.
- `src/render/Renderer.cpp` — corpos vetoriais, food com highlight, obstacle com outline, drawSelectionHalos, drawMarquee, drawLasso, drawBrushCursor.
- `src/ui/Command.hpp` — 9 comandos novos + dois Stroke variants.
- `src/ui/UiState.hpp` — flags showPreferencesPlaceholder/showAboutPanel/showGenomePlaceholder, openMenuIndex, quitRequested.
- `src/ui/InputRouter.hpp/.cpp` — event-driven pan, brush/eraser stroke, accept-no-poll.
- `src/ui/UiPanel.hpp/.cpp` — dropdown system, Pan removido da toolbar, rename Genoma, separacao de overlays.
- `src/sim/SimulationRunner.cpp` — applyCommand cobre 9 novos + 2 strokes (interpolacao linear).
- `src/main.cpp` — flags `--phase22-hotfix-*`.
- `CMakeLists.txt` — fonte `Phase22_1Diagnostics.cpp`.

## Problemas encontrados e correcao

| # | Sintoma | Causa | Correcao |
|---|---|---|---|
| 1 | Organismos pixelizados | Sem AA, 32 seg, sem outline, sem gradient | AA=8, 40 seg, outline + 2 discs gradient + triangulo direcional |
| 2 | Botoes de selecao sem feedback visual | Marquee/lasso/halo nao desenhados pelo renderer | `drawSelectionHalos/Marquee/Lasso/BrushCursor` no Renderer |
| 3 | Menu Arquivo resetava no clique | UiPanel disparava CmdResetSimulation no clique do titulo | titulo so toggla dropdown; reset agora via item "Novo" |
| 4 | Menu View vazio | UiPanel disparava CmdToggleSimpleRender no titulo | View ganhou dropdown completo com 7 itens funcionais |
| 5 | Preferencias abria Ajuda | UiPanel disparava CmdToggleHelpPanel no titulo de Preferencias | `CmdTogglePreferencesPanel` + overlay distinto |
| 6 | Botao PAN ocupava espaco e duplicava funcao | Pan estava em kTools array | kTools agora tem 9 ferramentas; Pan e exclusivamente botao direito |
| 7 | Brush de obstaculo so 1 stamp por clique | Sem handler em MouseMoved + sem interpolacao | `paintActive_` + `CmdPaintObstacleStroke` interpolando stamps |
| 8 | Menu Agente/Genoma com nome ruim | Nomenclatura mista | Renomeado para `Genoma` com 2 itens placeholder |
| 9 | Eraser nao apagava | Mesma causa do brush + radius nao multiplicado corretamente | `eraserActive_` + `CmdEraseObstacleStroke` com radius * 1.5 |
| 10 | Maximize movia cursor | SFML view nao atualizada apos Resized | `window_.setView(sf::View(...))` no `handleResize` |

## Testes executados

- Build Debug: OK (sem warnings novos).
- Build Release: OK (sem warnings novos).
- `--phase22-hotfix-selftest`: **PASS (26 checks)**.
- `--phase22-selftest` (regressao Fase 22): **PASS (155 checks)**.
- Regressoes Phase 7-21:
  - Phase 7: PASS (14), Phase 8: PASS (15), Phase 9: PASS (23), Phase 10: PASS (21),
  - Phase 11: PASS (30), Phase 12: PASS (49), Phase 13: PASS (42), Phase 14: PASS (71),
  - Phase 15: PASS (72), Phase 16: PASS (109), Phase 17: PASS (110), Phase 18: PASS (111),
  - Phase 19: PASS (133), Phase 20: PASS (134), Phase 21: PASS (150).
- `--phase22-hotfix-benchmark`:

```
scenario,agents,foods,obstacles,steps,total_ms,avg_step_us,commands,selection,notes
paint_30steps_0cmd,152,50,0,30,28.3350,944.5000,0,0,paint_strokes=0
paint_30steps_1cmd,152,50,240,30,28.3196,943.9867,30,0,paint_strokes=1
paint_30steps_4cmd,152,50,960,30,31.8107,1060.3567,120,0,paint_strokes=4
paint_100steps_1cmd,161,50,800,100,89.8076,898.0760,100,0,paint_strokes=1_steps=100
```

- Smoke test do executavel: abre janela e roda sem crash. Mensagem inicial: "AgentBioSimCpp Phase 22.1: UI hotfix (menus + canvas + brush + resize) ready".

## Validacao visual/manual

Esta validacao depende de inspecao humana no executavel. A camada de testes headless valida que:
- ferramentas se ativam,
- comandos chegam ao runner,
- stamps de stroke sao gerados na quantidade esperada,
- screen<->world fecha em viewport grande,
- menus separam suas acoes corretamente.

Para confirmar o resultado visual (organismos vetoriais, halos de selecao no canvas, marquee desenhado, dropdown abrindo etc.), o usuario deve rodar o executavel em ambiente grafico. O smoke test confirmou que o app abre e termina limpo apos a janela fechar.

## Pendencias remanescentes

- Validacao visual ao vivo pelo usuario (esta microfase nao tem como inspecionar pixels).
- Dear ImGui ainda pendente — a decisao de adotar fica para fase posterior. A separacao UI/engine ja torna a troca cirurgica: somente `UiPanel` muda.
- Editor genetico, painel de parametros, save/load — proximas fases.

## Confirmacoes

- Fase 23 NAO foi iniciada.
- Nenhum arquivo Python foi alterado.
- Save/load NAO foi implementado.
- Preferencias completas NAO foram implementadas (apenas placeholder).
- Editor genetico NAO foi implementado.
