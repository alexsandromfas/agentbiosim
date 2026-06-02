# Fase 25 — Migracao Total da UI para Dear ImGui

Atue como arquiteto senior de software C++, designer senior de UI/UX e especialista em Dear ImGui /
ImGui-SFML, sistemas de design, principios de Gestalt, acessibilidade visual, input routing e
arquitetura desacoplada engine/UI para uma simulacao evolutiva 2D de alto desempenho.

Esta e a fase de **refundacao da camada de UI**. Toda a interface atual foi construida a mao em SFML
imediato (`UiPanel`, `UiPreferencesPanel`, `UiLeftDock`), com geometria de hit-test recalculada e
duplicada entre o desenho e o clique. Isso e a Divida 9 e e a area mais fragil do projeto. A Fase 25
substitui **100% dessa UI** por Dear ImGui, com qualidade de nivel especialista em UI/UX, sem perder
nenhuma funcionalidade nem usabilidade ja entregue, e **sem tocar na arquitetura boa** (engine
headless, stores SoA, UI desacoplada por comandos).

## Principio que governa esta fase

- **Usabilidade, funcionalidades e parametros vem do Python** (disposicao de botoes, fluxo de menus,
  abas, o que cada controle faz). A base de usabilidade e o programa Python.
- **Arquitetura de codigo NAO vem do Python.** Vem da arquitetura nova C++ ja construida.
- Parametros internos relevantes que a arquitetura nova expoe e que nao existiam na UI Python podem
  ser adicionados, desde que no lugar logico/adequado, seguindo o esqueleto de UI ja existente e com
  rotulo amigavel em PT-BR.
- Nao copie a estrutura de codigo da UI Python. Copie a EXPERIENCIA.

## Regras invioláveis (arquitetura)

1. O nucleo da simulacao continua headless. `sim/`, `simulation/`, `systems/`, `neural/`,
   `perception/` NAO podem incluir Dear ImGui, ImGui-SFML nem `ui/`.
2. A UI nao vira dona da simulacao. UI emite comandos; o `SimulationRunner` aplica.
3. O `Renderer` continua desenhando mundo/agentes/comida/obstaculos/overlays; ele NAO processa input
   nem decide UI.
4. Determinismo por seed preservado. dt fixo preservado.
5. Nenhum arquivo Python alterado.
6. Build Debug e Release limpos.
7. Nada de funcionalidade perdida em relacao ao que existia ao fim da Fase 24.2.

## Estado esperado antes de iniciar

Confirme e reporte (rode `git status` e `git log --oneline -10`):

- Fases 0 a 24 concluidas e comitadas (inclui microfases 24.1 e 24.2).
- Working tree limpo. Se nao estiver, pare e reporte.
- Os selftests abaixo passam ANTES de comecar (baseline):
  `--phase22-selftest`, `--phase22_1-selftest` (ou equivalente), `--phase23-selftest`,
  `--phase23_1-selftest`, `--phase23_2-selftest`, `--phase24-selftest`, e regressoes 7 a 21.

## Pre-requisito tecnico obrigatorio: resolver a Divida 8

Antes de construir a UI nova, corrija a inversao de camada (Divida 8 em
`TECHNICAL_DEBT_REGISTER.md`):

- `sim/SimulationRunner.hpp` inclui `ui/Command.hpp` e tem `applyCommand(const ui::Command&)`.
- Mova `Command` e `CommandQueue` para uma camada neutra (sugestao: `src/core/Command.hpp` no
  namespace `agentbiosim::core` ou `agentbiosim::sim`). O engine passa a depender dessa camada
  neutra; a UI tambem. O engine deixa de incluir `ui/`.
- Refator puro, sem mudanca de comportamento. Os selftests existentes devem continuar passando antes
  de prosseguir para a UI. Registre no status que a Divida 8 foi resolvida.

So depois disso, comece a UI ImGui.

## Documentos obrigatorios a ler

- `MIGRATION_PHASES.md` (nota de re-sequenciamento e regra de usabilidade)
- `PROPOSED_CPP_ARCHITECTURE.md`
- `UI_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `FEATURE_INVENTORY.md`
- `TECHNICAL_DEBT_REGISTER.md` (Dividas 8 e 9)
- Status das Fases 22, 22.1, 23, 23.1, 23.2, 24 (o que cada painel faz hoje, em SFML manual)

## Referencia de usabilidade (Python)

Consulte como referencia de usabilidade (NAO de arquitetura): `sim/ui.py` principalmente, e
`sim/game.py`, `sim/render.py`, `sim/neural_viewer.py` quando precisar confirmar disposicao/fluxo.
Reproduza a disposicao (painel lateral esquerdo fixo com abas, menus, preferencias, toolbar) com a
linguagem visual elevada para um padrao moderno.

## Integracao Dear ImGui + ImGui-SFML

1. Adicione Dear ImGui e o backend ImGui-SFML ao projeto e ao `CMakeLists.txt` (Debug/Release).
   Documente a versao e a origem (vcpkg, submodulo, third_party, etc.). Prefira o caminho mais
   simples e reproduzivel para o build MSVC atual.
2. Inicializacao/encerramento limpos: `ImGui::SFML::Init(window)`, `ProcessEvent`, `Update`,
   `Render`, `Shutdown`, integrados ao loop do `App` (AppController), sem vazar para o engine.
3. Carregue uma fonte com suporte a acentos PT-BR (glyph ranges Latin-1/Latin Extended). A UI atual
   ja usa ASCII para contornar isso; com ImGui podemos usar acentuacao correta — prefira PT-BR
   correto ("Genetico", "Substrato", "Populacao") se a fonte suportar; senao mantenha ASCII e
   documente.
4. ImGui consome o mouse/teclado quando esta sobre um widget: o roteamento de eventos deve respeitar
   `ImGui::GetIO().WantCaptureMouse` / `WantCaptureKeyboard` ANTES de repassar o evento ao canvas
   (substitui o `pointInside*` manual de hoje).

## Objetivo

Substituir toda a UI manual por Dear ImGui, com:

- Paridade total de usabilidade e funcionalidade com o estado ao fim da Fase 24.2.
- Qualidade de UI/UX de nivel especialista (ver secao de principios).
- Codigo de UI declarativo (sem hit-test manual): Divida 9 resolvida.
- Engine intocado e headless: Divida 8 resolvida.

## Principios de UI/UX exigidos (nivel especialista)

A UI nova nao e so "ImGui default". Aplique:

- **Hierarquia visual clara**: titulos, grupos, separadores; o que e primario tem mais peso.
- **Gestalt**: proximidade (controles relacionados juntos), similaridade (mesmo estilo para mesma
  funcao), regiao comum (cards/painados com fundo proprio), continuidade e alinhamento em grade.
- **Espacamento consistente**: defina um espacamento base (ex.: 8px) e multiplos; padding e gaps
  uniformes via `ImGuiStyle`.
- **Tipografia**: pelo menos 2 pesos/tamanhos (titulo vs corpo); rotulos legiveis.
- **Tema proprio**: paleta coesa (escura, moderna), cores de acento para acoes primarias, estados
  claros de hover/active/disabled; raio de borda arredondado; sem o cinza default cru do ImGui.
- **Affordância**: botoes parecem clicaveis, sliders parecem arrastaveis, campos editaveis sao
  obvios. Estados desabilitados explicados ("(Fase 26)" etc.).
- **Iconografia**: use os icones da pasta `Assets/` (carregados como `sf::Texture`/`ImTextureID`)
  na toolbar e onde fizer sentido. Se algum icone faltar, documente.
- **Feedback**: tooltips com a descricao do parametro; confirmacao visual de acoes destrutivas.
- **Densidade adequada**: nem amontoado nem espalhado; o painel lateral deve caber sem rolagem
  excessiva, com scroll quando necessario.

Defina o tema em um unico lugar (ex.: `src/ui/imgui/Theme.cpp`) para ser reaproveitado pelas Fases
26, 30 e 31.

## Escopo obrigatorio (mapeie cada elemento atual para ImGui)

Reimplemente em Dear ImGui, com paridade funcional:

1. **Menu superior** (menu bar): Arquivo, Exibir, Preferencias, Agente, Ajuda — com os mesmos itens
   e estados (habilitado/desabilitado/placeholder de fase futura) que existem hoje. Mantenha o
   toggle "Painel lateral" em Exibir e os placeholders de Agente marcados (Fase 26/28).
2. **Toolbar de ferramentas de canvas**: Play/Pause, Stop/Reset, Step, Selecionar, Selecao
   retangular, Lasso, Comida, Agente, Pincel de obstaculo, Apagar obstaculo, Mover, Deletar, Fit
   world — com indicador de ferramenta ativa e icones. Indicadores de agentes/comida/obstaculos/FPS.
3. **Painel lateral esquerdo fixo** com 3 abas, reproduzindo a Fase 24.2:
   - **Editor Genetico**: linhas de parametros do genoma (reusa a fonte de dados atual,
     `editorParameters()`), com sliders/inputs/combos/color picker conforme o tipo; rodape Aplicar a
     especie / Aplicar selecionados / Reverter / Defaults.
   - **Substrato**: grupos Comida e Substrato; rodape Aplicar ambiente / Limpar Comida.
   - **Labels**: um card por especie com swatch+nome editavel, "{n} individuos", checkbox Grafico,
     Min/Max/Inicial, botoes Selecionar / Atribuir selecionados / Remover selecionados / Cor /
     Resetar rede (desabilitado, "(Fase 26)") / Excluir; rodape com checkbox de resgate-minimo e
     "+ Nova label com selecionados".
4. **Janela de Preferencias** (Fase 23): todos os grupos (simulacao, visao, redes neurais por tipo,
   autosave, aparencia, performance) com rotulos amigaveis PT-BR, combos corretos, color pickers,
   edicao por texto, defaults funcionais, scroll, e parametros internos ocultados conforme a Fase
   23.x. Agora janelas ImGui movediцeis/redimensionaveis nativamente (resolve as queixas antigas de
   "nao consigo mover a janela").
5. **Overlays**: ajuda (lista de atalhos), overlay de selecao/retangulo/lasso continuam pelo
   Renderer (sao mundo, nao UI) — mas o texto/ajuda passa para ImGui.
6. **Color picker** e **combos**: usar os widgets nativos do ImGui (`ColorEdit`/`ColorPicker`,
   `Combo`/`BeginCombo`), eliminando os popups manuais.

Reaproveite a logica de dados ja existente (ParameterRegistry, friendly labels PT-BR, enum values,
hide rules, os metodos do `SimulationRunner` para especies/labels). O que muda e a CAMADA DE
APRESENTACAO, nao o backend.

## O que preservar (nada disso pode regredir)

- Todos os comandos e seu roteamento ate o `SimulationRunner`.
- Conversao tela->mundo e o fix da microfase 22.1 (clique correto inclusive maximizado).
- Brush/eraser de obstaculo, selecao unica/retangular/lasso, spawn de comida/agente.
- Substrato circular/retangular, Aplicar ambiente sem reescalar organismos (fix 24.1).
- Atribuir label muda especie + recolore de fato (fix 24.2).
- Rotulos amigaveis PT-BR, combos corretos, ocultacao de aliases internos (Fases 23.x).
- Engine headless e todos os selftests anteriores.

## Fora de escopo (nao implementar agora)

- Visualizador neural / painel completo do agente selecionado (Fase 26).
- Metricas/graficos/profiler (Fase 27).
- Save/Load/Export/Import/Autosave reais (Fase 28).
- Janela do Desenvolvedor / performance in-app (Fase 30).
- Paridade exaustiva item-a-item do `UI_INVENTORY.md` (Fase 31).
- Otimizacao de performance do engine (Fase 32).

Itens marcados como placeholders devem aparecer desabilitados com a fase de destino indicada.

## Determinismo e performance

- A troca de UI nao pode alterar a sequencia de comandos aplicados nem o determinismo por seed.
- Meca o custo da UI ImGui aberta vs fechada e compare com o baseline SFML manual (deve ser
  comparavel ou melhor; ImGui e barato). Registre em `--phase25-diagnostics`.

## Testes obrigatorios — `--phase25-selftest`

Crie o selftest headless cobrindo pelo menos:

1. Engine continua headless: nenhum header de `sim/`, `simulation/`, `systems/`, `neural/`,
   `perception/` inclui ImGui/ImGui-SFML/`ui/` (checagem por grep no proprio teste ou nota
   documentada + assert de compilacao headless).
2. Divida 8 resolvida: `SimulationRunner` nao inclui `ui/`; `Command` esta na camada neutra.
3. Comando emitido pela UI (ex.: CmdSetCanvasTool, CmdAssignSelectedToSpecies, CmdApplyEnvironment)
   chega ao `SimulationRunner` e produz o efeito esperado (assign muda speciesId+cor; create cresce
   SpeciesStore; aplicar ambiente reconfigura o mundo) — reaproveitar/portar os checks da Fase 24.
4. ParameterRegistry: setValue com clamp/enum continua valido; friendly labels PT-BR resolvem; enum
   values corretos; aliases internos ocultados.
5. Selecao unica/retangular/lasso e conversao tela->mundo continuam corretas (portar checks 22/22.1).
6. Regressoes: os checks essenciais das Fases 22, 23, 23.1, 23.2, 24 continuam verdes (porte o que
   for de backend; o que era hit-test manual vira teste de comando).

Smoke manual (documente): app abre; menu/toolbar/painel lateral/preferencias renderizam; arrastar
janela de preferencias funciona; clique sobre painel ImGui NAO pinta no canvas; clique no canvas
livre funciona; combos e color picker abrem; atribuir label recolore.

## Diagnostics/microbenchmark — `--phase25-diagnostics`

Meca: FPS e us/step com UI fechada, com painel lateral aberto, com preferencias aberta, com tudo
aberto; overhead UI ImGui on/off; 100/300/600/1000 agentes. Compare com o baseline SFML manual da
Fase 24. Registre seed, commit, build.

## Build obrigatorio

```
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Debug
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Release
```

(Pode ser necessario reconfigurar o CMake apos adicionar ImGui/ImGui-SFML.)

## Regressoes obrigatorias

Rode os selftests das Fases 7 a 24 (e o novo `--phase25-selftest`). Todos devem passar. Se algum
quebrar por causa da reescrita de UI, conserte na propria fase (a logica de backend nao deveria
mudar; o que muda e apresentacao). Se a quebra for estrutural grande, pare e reporte.

## Documentacao obrigatoria

Crie `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_25_IMGUI_UI_MIGRATION_STATUS.md` com: escopo
executado; como a Divida 8 foi resolvida (onde Command foi parar); como a Divida 9 foi resolvida
(o que de UI manual foi removido); integracao ImGui/ImGui-SFML (versao, CMake); decisao de fonte/
acentos; tema/estilo definido e onde; mapeamento elemento-a-elemento (SFML manual -> ImGui); icones
usados/faltando; o que foi preservado e como; itens deixados para Fases 26/27/28/30/31; resultado de
build Debug/Release; resultado de `--phase25-selftest`, `--phase25-diagnostics` e regressoes 7-24;
comparacao de performance vs baseline; confirmacao de engine headless; confirmacao de que nenhum
Python foi alterado; pendencias.

Atualize `TECHNICAL_DEBT_REGISTER.md` marcando Dividas 8 e 9 como RESOLVIDAS na Fase 25 (com commit/
data). Atualize `MIGRATION_PHASES.md` apenas com ajuste factual de status, se necessario.

## Saida final esperada

Resumo executivo; arquivos criados/modificados; como ficou a fronteira UI/engine apos a Divida 8;
como ficou o tema/estilo; prints/descricao de cada painel; resultados de build/selftests/benchmark;
divergencias contra o estado anterior; pendencias para as proximas fases; decisao: pronto para
commit da Fase 25 ou precisa correcao. Se tudo certo, commit da Fase 25.

Nao avance para a Fase 26 sem autorizacao explicita.
