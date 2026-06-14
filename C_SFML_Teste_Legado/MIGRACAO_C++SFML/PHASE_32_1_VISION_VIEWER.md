# Fase 32.1 — Visualizador de Visão do Agente + Visão por Tipo no Editor + Buckets

> Nota de numeracao: o rotulo "32.1" foi escolhido pelo usuario para ESTA fase. Ja
> existe uma *microfase 32.1* interna (defaults de min/max de populacao) citada no
> CLAUDE.md; sao coisas distintas. Este arquivo e o prompt formal desta fase.

## Objetivo

Tres entregas, todas na area de VISAO, feitas de forma arquiteturada (sem jogar
codigo em qualquer lugar), organizada, bonita e otimizada:

A. **Visao por tipo no editor genetico** (ver comida / ver organismos / ver
   predadores / ver obstaculos / ver tudo / ver atraves de paredes), POR LABEL.
B. **Visualizador da visao do agente selecionado**: ao selecionar UM organismo, a
   visao dele aparece desenhada na frente dele, FUNCIONANDO e bonita; o desenho e
   DIFERENTE conforme o modo (bin/setor vs raycast). Nao aparece com 0 ou >1
   selecionados (custo controlado).
C. **Otimizacao**: o spatial hash guarda comida e organismos em buckets separados,
   para que uma consulta "ver so comida" nao toque nas celulas de organismos.

## Estado atual (verificado antes de comecar)

- **A (editor):** a microfase 32.5 ja moveu os see-flags para o genoma
  (`simulation::VisionConfig` no `GenomeRecord`), corrigiu os nomes em
  `UiLeftDock::editorParameters()` (`bacteria_retina_see_*`) e adicionou rotulos
  PT-BR. **Verificar** que a secao "Visao" do editor (ImGuiUi) realmente lista e
  edita os 6 flags, e que "Aplicar a especie/selecionados" grava no genoma da label
  (caminho 32.2). Se faltar agrupamento na secao Visao, corrigir.
- **B (visualizador):** a infra da Fase 26 existe e esta PARCIALMENTE morta para o
  usuario:
  - `perception::VisionDebugData` (DTO read-only de raios) e preenchido pela
    percepcao SO para o agente-alvo (`PerceptionDebugRequest`, trace-on-demand,
    custo zero sem alvo).
  - `App::update()` so define o alvo (`runner_.setVisionDebugTarget`) quando
    `uiState_.selectedVisionOverlay` (toggle da tecla V) esta LIGADO; pega o 1o
    selecionado. `App::render()` so passa o overlay quando o toggle esta on.
  - `Renderer::drawVisionDebug()` desenha cada raio como uma LINHA fina colorida —
    **igual para todos os modos** (nao distingue setor de raycast) e visualmente
    pobre.
  - Logo: selecionar um organismo NAO mostra a visao (precisa apertar V). O usuario
    quer que mostre automaticamente ao selecionar UM. (No Python, render.py
    `_draw_vision_rays`/`_draw_vision_bins` desenha a visao quando o agente esta
    selecionado; setor = cunhas angulares + arcos de distancia, raycast = raios com
    intensidade.)
- **C (buckets):** `SpatialHash::queryRadiusInto` devolve TODA a vizinhanca
  (comida+agentes+obstaculos misturados); `SceneQuery::filterSpatialItems` filtra
  por tipo depois. Logo "ver so comida" ainda itera as celulas com agentes.

## Arquitetura (onde o codigo vai)

- **Engine permanece sagrado e read-only para a UI.** A percepcao continua dona do
  `VisionDebugData`. Se o modo setor precisar de mais dados para desenhar cunhas
  (largura angular do bin, ativacao por bin, raio do anel mais distante atingido),
  estender `VisionRayDebug`/`VisionDebugData` com campos novos preenchidos por
  `sectorBinsVisionForAgent` — sem dependencia de SFML/ui.
- **Novo modulo de desenho** `render/VisionOverlay.{hpp,cpp}` (camada render, pode
  usar SFML) com a logica bonita e mode-aware. O `Renderer` delega para ele
  (`drawVisionDebug` vira um wrapper fino ou e substituido). Mantem `Renderer.cpp`
  enxuto. O overlay le so o `VisionDebugData` + camera + posicao/angulo do alvo.
- **Gatilho automatico:** `App::update()` define o alvo de visao quando ha
  EXATAMENTE 1 agente selecionado (independe da tecla V); limpa com 0 ou >1.
  Manter `selectedVisionOverlay` como preferencia mestre (default ON) para quem
  quiser desligar; a tecla V/menu Debug alterna essa preferencia.
- **Buckets:** dentro do `SpatialHash`, manter por celula listas separadas por tipo
  (comida / agente / obstaculo) OU hashes paralelos. `queryRadiusInto` ganha uma
  mascara de tipos (ou overloads) para varrer so os buckets pedidos. CRITICO para o
  golden: a ORDEM dos candidatos retornados para uma consulta "so comida" (cenarios
  do golden = bacteria default) deve ser IDENTICA a de hoje. So muda quando o filtro
  exclui um tipo inteiro (ai pula o bucket). Provar com `--phase32-checksum`.

## Detalhe do desenho (Parte B)

- **Raycast (single / fullbody):** raios do olho ate o hit; cor = cor do objeto
  visto; intensidade/alpha e espessura proporcionais a ativacao; raios sem hit
  desenhados esmaecidos ate o alcance. Marcador no ponto de hit. Cone de FOV de
  fundo (leve) para dar contexto.
- **Bin/Setor:** `retinaCount` cunhas angulares cobrindo o FOV a partir do olho,
  cada uma preenchida com cor/alpha pela ativacao do bin; aneis de distancia
  (arcos) subdividindo o raio de visao (como o `_draw_bin_grid` do Python), porem
  mais bonito (gradiente, bordas suaves). Multiplos olhos (`eyeCount`) desenhados
  cada um a partir do seu offset angular.
- **Comum:** desenhar SOMENTE para o alvo unico; custo zero quando nao ha alvo
  (percepcao nem preenche o debug). Usar `sf::VertexArray` (Triangles/TriangleFan/
  Lines) em batch, sem alocacao por frame no caminho quente.

## Selftests / Verificacao

- `--phase26-selftest` (viewer neural + vision debug) deve continuar PASS; estender
  com checagens do novo modo-setor do `VisionDebugData` se aplicavel.
- Novo bloco no `--phase31-selftest` (ou phase26-diagnostics): (1) selecionar 1
  agente preenche o `VisionDebugData` do alvo; (2) selecionar 0 ou 2+ NAO preenche
  (alvo limpo); (3) modo setor preenche os dados de bin; modo single/fullbody
  preenche raios. Headless: setar o alvo via runner e checar `runner_.visionDebug()`.
- Editor (A): garantir `editorParameters()` lista os 6 `bacteria_retina_see_*` e que
  aplicar no genoma da label muda a percepcao por-label (ja coberto pelo bloco J da
  32.5; estender se necessario para "ver obstaculos").
- Buckets (C): `--phase32-checksum` BYTE-IDENTICO ao golden atual (cenarios usam
  bacteria default = ver so comida); `--phase32-selftest` (determinismo) PASS;
  rodar `--phase29-bench` e `--vision-bench` para mostrar o ganho do "ver so comida".
- Regressao completa 7–32 + hotfixes, Debug E Release. Golden preservado.

## Ordem de execucao sugerida (incremental, buildar/testar a cada passo)

1. Parte A: verificar/garantir os see-flags na secao Visao do editor (rapido).
2. Parte B: gatilho automatico (single-selection) + modulo `render/VisionOverlay`
   bonito e mode-aware; estender `VisionDebugData` se o setor precisar. Selftests.
3. Parte C: buckets no SpatialHash preservando a ordem da consulta de comida; provar
   golden byte-identico; bench do ganho.
4. Docs (CLAUDE.md + memoria) + commit por parte ou um commit coeso da fase.
