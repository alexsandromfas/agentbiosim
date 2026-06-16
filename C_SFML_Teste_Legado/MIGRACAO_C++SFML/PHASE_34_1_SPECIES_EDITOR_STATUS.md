# Fase 34.1 — Editor de Genoma por Espécie + apply granular ao vivo — STATUS (CONCLUÍDA)

## O paradigma novo (a dor resolvida)

Antes: o editor de genoma era um buffer GLOBAL (`bacteria_*`). Mexer num campo (ex.: dieta) e
clicar "Aplicar à espécie" jogava **todos** os campos do editor na espécie, sobrescrevendo o que o
usuário não queria mudar. Confuso e perigoso.

Agora: **cada espécie tem o seu próprio editor, vinculado ao genoma daquela espécie**. Editar um
campo e apertar **Enter** aplica **apenas aquele campo**, ao vivo, à espécie (template + todos os
membros vivos). Sem botão "aplicar tudo". A "label" virou **espécie**; o dock é uma aba por espécie.

## Arquitetura — o núcleo (apply granular)

- **`simulation/GenomeFields.{hpp,cpp}` (novo, engine puro):** a ponte única campo↔genoma, indexada
  pela chave-sufixo do registry (`body_size`, `diet_food`, `retina_see_food`, ...).
  - `setGenomeField(GenomeRecord&, field, value)` — escreve UM campo (com os mesmos clamps do
    bootstrap); retorna false para campo global/desconhecido (no-op seguro).
  - `genomeFieldValue(genome, field)` — lê o valor atual como `ParameterValue` (a UI semeia os
    widgets a partir do genoma da espécie).
  - `isGenomeField(field)` — true para os traços por-indivíduo.
  - É o mesmo ponto que a **Fase 34.2** estende (mover os globais para o genoma) e que uma futura
    **mutação** vai reusar para tocar qualquer campo.
- **`core::CmdSetSpeciesGenomeField {speciesId, field, value}`** (novo) + **`CmdCreateSpeciesDefault`**
  (novo) no variant de comandos.
- **`SimulationRunner::setSpeciesGenomeField(speciesId, field, value)`** (novo): resolve o
  genoma-template da espécie (clone-on-write se compartilhado), escreve só aquele campo nele e no
  genoma pessoal de cada membro vivo (clone-on-write), atualiza estado derivado (raio se `body_size`,
  forma se `body_shape`, clamp de energia se `energy_cap`) e o `dietSnapshot` (se dieta).
  O(membros da espécie); `rebuildSpatial()` só quando `body_size` muda; sem reset; cérebros
  preservados. Tratado direto em `applyCommand` (não precisa da seleção).
- **`SimulationRunner::createSpeciesDefault()`** (novo): cria espécie a partir do template bacteria
  (nome "Especie N", cor da paleta, min 5 / max 150 / inicial 5), clona o genoma e **spawna 5
  organismos** (`spawnAgentsOfSpecies`, RNG local — ação de UI, fora do golden/determinismo).

## Campos por-espécie (ao vivo) × globais (marcados) NESTA fase

- **Por-espécie (no `GenomeRecord`, editáveis ao vivo):** corpo (tamanho/forma), energia
  (inicial/split/cap), reprodução (idade mín./cooldown), dieta (come comida/organismos/mesma
  espécie, eficiências, corpo→comida), flags de visão (o que enxerga), mutação (taxa/força).
- **Globais (mostrados read-only com selo "(global)" + tooltip):** velocidade, virada, locomoção,
  energia/idade de morte, custos de locomoção, **geometria** da visão (raio/retinas/FOV/olhos/
  canais/modo) e a arquitetura neural (`hidden_layers`). Editados em Preferências; **migram para o
  genoma na Fase 34.2** (reusando este mesmo bridge).
- Correção de bug latente: o `editorParameters()` antigo listava `bacteria_food_efficiency`/
  `_agent_efficiency` (não existem no registry; as chaves reais são `_diet_food_efficiency`/
  `_diet_agent_efficiency`). O editor por-espécie usa as chaves corretas e essas eficiências agora
  aparecem e aplicam.

## UI/UX (ImGuiUi.cpp)

- **Dock = abas de espécie.** Uma aba por espécie; a "orelha" tinge com a cor da espécie
  (`pushSpeciesTabColors`, mais escura inativa / clara ativa). Nome editável **≤10 caracteres**
  (buffer de 11). Abas com rolagem; **aba "+"** ao final (`TabItemButton`, trailing). Dock retrátil
  (Exibir > Painel lateral, `CmdToggleLeftDock`).
- **Aba "+":** com seleção → `CmdCreateSpeciesFromSelected` (os selecionados viram a espécie, sem
  spawn); sem seleção → `CmdCreateSpeciesDefault` (genoma padrão + 5 organismos). Tooltip explica o
  caso atual.
- **Topo do editor:** cor, nome (≤10), contagem de vivos, gráfico, população (Min/Max/Ini),
  Selecionar/Atribuir/Remover seleção, **Resetar rede neural** e **Excluir espécie** (desabilitado
  para bacteria). Um **mini-visor** desenha um exemplar (corpo elíptico/circular na cor da espécie)
  no canto superior direito (draw-list, sem custo de layout).
- **Campos:** numérico = caixa de texto que fica **laranja** enquanto o valor digitado diverge do
  genoma e aplica só aquele campo no **Enter**; bool = checkbox (aplica na hora); enum = dropdown
  (aplica na hora). O "✓ Aplicado" verde reaparece a cada aplicação. (Comparação do float contra o
  genoma já convertido a float — senão campos como `mutation_rate`=0.05 ficariam laranja para sempre
  por imprecisão float↔double.)
- **Modal de confirmação:** "Resetar rede neural" abre uma janela modal ("isto vai reconstruir os
  cérebros desta espécie; o aprendizado será perdido — confirmar?") antes de disparar
  `CmdResetNeuralForSpecies`. É o gancho que a Fase 34.2 reusa para edições de geometria de visão
  (que reconstroem a rede).
- **Substrato** saiu do dock e virou **item no menu superior** (entre Agente e Ajuda) que abre uma
  **janela** com os parâmetros de substrato/comida + "Aplicar ambiente"/"Limpar comida"
  (`UiState.showSubstrateWindow`).
- Botões "Aplicar à espécie"/"Aplicar aos selecionados" **removidos**. O toggle global "Resgate de
  população mínima" ficou no rodapé do dock e agora aplica **imediato** (o dock não tem botão
  Aplicar) — adicionado à lista de knobs imediatos do App.

## Testes — `--phase34-selftest` (novo, 15 checks, PASS)

`systems/Phase34Diagnostics.{hpp,cpp}`: (A) editar um campo muda **só** ele (corpo/dieta/mutação
intactos); (B) aplica ao template e a todos os 150 membros; (C) `body_size` atualiza o raio dos
membros; (D) campo de dieta sincroniza o `dietSnapshot`; (E) editar uma espécie não afeta outra
(independência); (F) "+" sem seleção spawna 5; (G) "+" com seleção reatribui (sem spawn) e tira da
antiga; (H) determinismo (2 execuções idênticas).

## Build, regressão, golden

- Build **Debug E Release** limpos.
- **Golden `--phase32-checksum` BYTE-IDÊNTICO** ao baseline (a camada engine é inerte até ser
  chamada por um comando da UI; a UI não toca no caminho determinístico). `--phase32-selftest`
  (determinismo) PASS.
- Regressão completa **7–34 PASS** (inclui hotfixes 22.1/23.1/23.2). `--phase31-selftest` = 112
  checks.
- CMakeLists: adicionados `src/simulation/GenomeFields.cpp` e `src/systems/Phase34Diagnostics.cpp`
  (lista explícita).

## Limites / o que segue para a 34.2

- Velocidade/morte/custos/geometria-de-visão/arquitetura-neural continuam **globais** (marcados).
  A 34.2 move esses traços para o `GenomeRecord` (por indivíduo) e reescreve os sistemas para ler
  por-genoma, reusando `GenomeFields` e o modal de confirmação desta fase.
- A aba "+" a partir da seleção é a semente do futuro "promover um mutante a nova espécie".
- Sem mutação, sem custo por neurônio, sem placeholders — só a fundação arquitetural.

## Validação visual (manual)

A correção do backend está provada pelo `--phase34-selftest` + golden. A aparência do dock (abas
coloridas, laranja ao editar, modal, janela de Substrato) é verificada abrindo o app (a UI ImGui
precisa de contexto GL; não há selftest de render).
