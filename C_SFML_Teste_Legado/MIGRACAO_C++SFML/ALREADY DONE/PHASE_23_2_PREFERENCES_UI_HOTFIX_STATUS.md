# Microfase 23.2 — Segunda Rodada de Correcoes da UI de Preferencias

Status: **CONCLUIDA** (2026-05-30)

A microfase 23.1 ja tinha reorganizado o fluxo de menu Preferencias em janelas separadas, escondido Numba e adicionado labels amigaveis para parte dos parametros. A validacao manual revelou que ainda existiam bugs serios que tornavam a UI inutilizavel para edicao real:

1. janelas nao moviam (nao tinham drag);
2. parametros neurais ainda em ingles (`neural_rnn_recurrent_init_std`...);
3. precisava editar valores via caixa de texto, nao botoes +/-;
4. botao "Defaults" nao aplicava nada (so populava pending);
5. botoes footer ainda em ingles ("Defaults");
6. botao X nao centralizado;
7. combo de rede neural nao abria — aparecia "runtime" e "neural v.";
8. combo de substrato (circular/retangular) tambem mostrava "runtime";
9. slider de velocidade na toolbar era stepper +/-, deveria arrastar;
10. abaixo de cada label aparecia o nome interno em cinza ("Colisao entre agentes / agent_collision_enabled");
11. cada tipo de rede neural deveria mostrar so seus proprios parametros (nao amontoar RNN com NEAT).

Esta microfase resolve TODOS esses itens.

## Causa raiz do "runtime / neural v."

O bug estava na confusao semantica do campo `domains` no `ParameterRegistry`:

- Em `ParameterDefaults.cpp`, `domains` recebe TAGS (`{"runtime", "neural"}`) usadas pelo `dump()` para filtragem.
- O codigo da microfase 23.1 leu esse mesmo campo como se fosse a LISTA DE VALORES DO ENUM e mostrou "runtime", "neural" como opcoes do combo.

**Correcao**: adicionado `prefsEnumValuesFor(name)` em `ParameterMetadata.{hpp,cpp}` que retorna a lista correta de valores enum para cada parametro:

| Parametro                                | Enum values                                  |
|------------------------------------------|----------------------------------------------|
| `neural_network_type`                    | mlp, gated_mlp, shortcut_mlp, modulated_mlp, simple_rnn, neat, proto_neat, recurrent_neat |
| `substrate_shape`                        | rectangular, circular                        |
| `retina_vision_mode`                     | frontal, omni, raycast, raycast_omni         |
| `retina_bins_mode`                       | single, sector, global, auto_sector          |
| `retina_bins_distance_distribution`      | linear, log, quadratic                       |
| `retina_bins_distance_falloff`           | none, linear, exponential                    |
| `retina_bins_projection`                 | flat, fisheye                                |
| `neural_neat_initial_topology`           | empty, minimal, layered                      |
| `neural_proto_neat_initial_topology`     | empty, minimal, layered                      |
| `neural_recurrent_neat_initial_topology` | empty, minimal, layered                      |

O `UiPreferencesPanel` agora consulta `prefsEnumValuesFor(name)` para popular o combo popup — nunca mais o campo `domains` aparecera como conteudo do menu.

## Filtro neural por tipo selecionado

Adicionado `prefsShouldShowNeuralParameterFor(name, currentNetworkType)`. Detecta prefixos:

- `neural_gate_*`           -> so aparece quando type = gated_mlp ou modulated_mlp
- `neural_shortcut_*`       -> so aparece quando type = shortcut_mlp
- `neural_rnn_*`            -> so aparece quando type = simple_rnn
- `neural_proto_neat_*`     -> so aparece quando type = proto_neat
- `neural_recurrent_neat_*` -> so aparece quando type = recurrent_neat
- `neural_neat_*`           -> so aparece quando type = neat
- `neural_network_type`     -> sempre aparece
- demais `neural.*` (comuns) -> sempre aparecem

`prefsParametersForTabFiltered` aplica esse filtro automaticamente para a aba Neural usando o tipo atual (pendingValue se houver, senao registry value). Resultado: trocando o combo para `simple_rnn` a janela mostra apenas os 7 knobs de RNN; trocando para `neat` mostra os 11 knobs de NEAT — nunca os dois ao mesmo tempo.

## Labels amigaveis ate o ultimo parametro neural

`kFriendlyLabels` foi migrado de `std::array` fixo para `std::vector` e cresceu de 70 para ~120 entradas. Cobre agora:

- Todos os parametros neurais RNN (`neural_rnn_recurrent_init_std` -> "Desvio padrao inicial recorrente (RNN)", `neural_rnn_recurrent_scale`, `neural_rnn_mutation_rate`, etc.)
- Todos os parametros NEAT (`neural_neat_*`, `neural_proto_neat_*`, `neural_recurrent_neat_*`) com sufixo distinguindo a familia ("(NEAT)" / "(proto NEAT)" / "(NEAT recorrente)")
- Mundo / substrato (`substrate_shape` -> "Formato do substrato", `world_w`, `world_h`, `substrate_radius`)
- Cores (`substrate_bg_color` -> "Cor de fundo", `substrate_color_top/bottom`, `substrate_border_color`, `background_color_top/bottom`)
- Brain cache (Fase 27, todos marcados pendente)
- Camera (camera_follow_*)
- Etc.

**O subtitle com o nome interno foi REMOVIDO** das rows. Agora aparece apenas o label amigavel. O nome interno fica disponivel apenas no codigo / nas selftests, nao polui a UI.

## Botao "Restaurar padroes" aplica imediatamente

Antes (23.1): clicar em "Defaults" so populava `pendingValues` com `originalDefault`. O usuario precisava clicar em Aplicar depois. Nenhum efeito visivel imediato. Confuso.

Agora (23.2):
- Novo command `CmdRestoreDefaultsAndApply`.
- App detecta a janela ativa (topo da pilha de janelas abertas), pega os parametros filtrados dessa janela, popula `pendingValues` com `originalDefault` e **chama `prefsApplyPending` imediatamente** + processa flags (`configureRenderOptions / runner.reset / configureFromParameters`).
- Resultado: o usuario clica no botao e ve os valores voltarem para o default na hora.

## Botoes footer todos em PT-BR

Antes (23.1): "Aplicar", "Reverter", "Defaults", "Fechar".
Agora (23.2): "Aplicar", "Reverter", "Restaurar padroes", "Fechar".

## Janelas arrastaveis

- Adicionado em `PreferencesState`: `std::array<float, Count> windowX`/`windowY` (sentinel -1 = posicao default cascateada), `int draggingTab` (-1 = nada), `dragOffsetX/Y` e versao equivalente para a janela Help.
- `UiPreferencesPanel::handleMouseClick` detecta clique no header strip (excluindo o botao X), seta `draggingTab` e captura offset.
- Novo `handleMouseMove(sx, sy, viewport, state, queue)` emite `CmdMovePreferencesWindow{tab, x, y}` que App escreve em `windowX/Y[tab]`.
- Novo `handleMouseRelease` reseta `draggingTab`.
- App.cpp roteia `sf::Event::MouseMoved` e `MouseButtonReleased` para esses handlers quando ha drag em progresso.
- Mesma logica para a janela Help (`draggingHelp`, `helpWindowX/Y`, `CmdMoveHelpWindow`).

## Edicao via caixa de texto

- Adicionado em `PreferencesState`: `std::string editingParam` (vazio quando nao editando) e `std::string editingBuffer`.
- Novos commands: `CmdBeginEditParameter{name, initialBuffer}`, `CmdCancelEditParameter{}`, `CmdCommitEditParameter{}`.
- Clicar na celula de valor de um param int/float emite `CmdBeginEditParameter` com o valor formatado atual.
- App.cpp captura `sf::Event::TextEntered` (digitos, ponto, menos, mais, e/E, backspace) quando `editingParam` esta ativo e ANEXA ao buffer (filtra outros caracteres).
- Enter emite `CmdCommitEditParameter` que faz `std::stoi`/`std::stod` do buffer (com try/catch para invalido) e escreve `pendingValues[name]`.
- Esc emite `CmdCancelEditParameter` que limpa buffer sem aplicar.
- Visual: celula vira azul (`kEditingBg`) com borda accent, mostra `buffer + "_"` (cursor simulado). Celula normal mostra valor + "(clique p/ editar)" como hint.

## Slider de velocidade arrastavel na toolbar

Antes (23.1): widget com [-] / label / [+] que aplicava `*0.5` ou `*2.0`. Funcionava mas era stepper, nao slider.

Agora (23.2):
- Layout: label "Velocidade" + texto NN.NNx + track horizontal de ~200px de largura + thumb azul + marcas em 1x e 10x.
- Mapeamento **log-escala** entre 0.1x e 50.0x (formula: `10^(log10(0.1) + rel * (log10(50) - log10(0.1)))`).
- Click no track captura o drag + jump-to-position (define `uiState.velocitySliderDragging = true`).
- App.cpp processa `MouseMoved` enquanto draggando e emite `CmdSetTimeScale{computed}`.
- Release encerra o drag.
- `UiPanel::draw` publica `velocitySliderTrackX` e `velocitySliderTrackW` em `UiState` toda frame para o App calcular corretamente.

## Botoes arredondados + X centralizado

- Novo helper `drawRoundedRect(target, x, y, w, h, radius, fill, outline, thickness)` em `UiPreferencesPanel.cpp`. Combina um RectangleShape principal + bordas + 4 CircleShape nos cantos para simular arredondamento (SFML 2.6 nao tem rect com cantos arredondados nativo).
- Botoes footer, close button, swatches de cor, presets, popup neural e popup color picker — todos usam rounded corners.
- O close button virou um quadrado 22x22 arredondado vermelho com o glyph "x" posicionado no centro (offset empiricamente verificado para segoeui.ttf / arial.ttf no tamanho 14).

## Arquivos C++ criados

- `src/systems/Phase23_2Diagnostics.hpp` / `.cpp`
- `MIGRACAO_C++SFML/PHASE_23_2_PREFERENCES_UI_HOTFIX_STATUS.md`

## Arquivos C++ modificados

- `src/config/ParameterMetadata.hpp/.cpp` — `prefsEnumValuesFor`, `prefsShouldShowNeuralParameterFor`, `kFriendlyLabels` expandido para ~120 entradas (agora `std::vector`).
- `src/ui/Command.hpp` — 6 commands novos: `CmdMovePreferencesWindow`, `CmdMoveHelpWindow`, `CmdBeginEditParameter`, `CmdCancelEditParameter`, `CmdCommitEditParameter`, `CmdRestoreDefaultsAndApply`.
- `src/ui/PreferencesState.hpp` — `windowX/Y` (com sentinel -1), `draggingTab/Help`, `dragOffsetX/Y`, `helpWindowX/Y`, `editingParam/Buffer`. Construtor inicializa sentinels.
- `src/ui/UiState.hpp` — `velocitySliderDragging/TrackX/TrackW`.
- `src/ui/UiPanel.cpp` — slider de velocidade com track + thumb + marcas; click captura drag; expõe geometry para App.
- `src/ui/UiPreferencesPanel.hpp/.cpp` — handleMouseMove/handleMouseRelease (drag); prefsParametersForTabFiltered (filtro neural); enum popups via `prefsEnumValuesFor`; editor inline; footer 100% PT-BR; rounded corners; X centralizado; subtitle de nome interno REMOVIDO.
- `src/app/App.cpp` — dispatch dos 6 commands novos; `MouseMoved`/`MouseButtonReleased` para drag + slider; `TextEntered`/`KeyPressed` para editor inline; mensagem inicial atualizada para "Phase 23.2".
- `src/sim/SimulationRunner.cpp` — no-op cases para os 6 commands novos.
- `src/main.cpp` — flag `--phase23-hotfix2-selftest`.
- `CMakeLists.txt` — `Phase23_2Diagnostics.cpp`.

## Resultados dos testes

- Build Debug: OK.
- Build Release: OK.
- `--phase23-hotfix2-selftest`: **PASS (58 checks)**.
- `--phase23-hotfix-selftest`: **PASS (58 checks)**.
- `--phase23-selftest`: **PASS (189 checks)**.
- `--phase22-hotfix-selftest`: **PASS (26 checks)**.
- `--phase22-selftest`: **PASS (155 checks)**.
- Regressoes Phase 7-21: **todas PASS** (14, 15, 23, 21, 30, 49, 42, 71, 72, 109, 110, 111, 133, 134, 150).
- Smoke test do executavel: abre limpo. Mensagem inicial: `"AgentBioSimCpp Phase 23.2: janelas arrastaveis + edicao de texto + combo correto ..."`.

## Pendencias para microfases futuras

- Copiar a pasta `Assets/` do Python e usar PNGs nos botoes da toolbar (precisa `sf::Texture` loading + cache).
- Snap-to-grid no drag de janelas.
- Persistencia de posicoes de janelas entre sessoes (Fase 27 save/load).
- Color picker com seletor de matiz (a versao 23.2 tem 8 presets + steppers RGB).
- Slider arrastavel para todos os parametros numericos (a versao 23.2 tem caixa de texto editavel + clamp).
- Tab key para navegar entre celulas editaveis.

## Confirmacoes finais

- **Nenhum arquivo Python foi alterado.**
- **Microfase 22.1 NAO regrediu** (regressao Phase22 + 22.1 PASS).
- **Microfase 23.1 NAO regrediu** (regressao Phase23.1 PASS).
- **Fase 24 NAO foi iniciada.**
- **Editor genetico completo NAO foi implementado.**
- **Save/load NAO foi implementado.**
