# Fase 34.1 — Editor por-espécie + aplicação granular ao vivo + Substrato como janela

Atue como arquiteto sênior de software C++, com foco igual em **usabilidade (UX)**, **performance
data-oriented** e **coesão arquitetural**. Esta fase NÃO é uma gambiarra pendurada na UI antiga: é
uma **mudança de paradigma** no fluxo de edição genética, projetada como fundação do roadmap
(mutação por indivíduo, promover indivíduo a espécie, ecossistema). Implemente pensando que tudo o
que vier depois deve encaixar naturalmente nisso — sem placeholders agora.

## Visão (por que esta fase existe)

Hoje o editor de genoma é um buffer GLOBAL: o usuário muda um campo (ex.: dieta) e, ao "Aplicar à
espécie", **todos** os campos do editor são jogados na espécie, sobrescrevendo o que ele não queria
mexer. Isso é confuso e perigoso. O novo paradigma:

> **Cada espécie tem o seu próprio editor de genoma, vinculado ao genoma daquela espécie.** Editar
> um campo e apertar Enter aplica **apenas aquele campo**, ao vivo, naquela espécie. Sem botão
> "aplicar tudo".

A "label" passa a se chamar **espécie**. O dock da esquerda deixa de ter abas de função (Editor /
Substrato / Labels) e passa a ter **uma aba por espécie**, cada aba sendo o editor daquela espécie.

## Regras invioláveis

1. **Engine headless é sagrado** (`src/sim`, `src/simulation`, `src/systems`, `src/neural`,
   `src/perception`): nada de SFML/ImGui/`ui::`. Comandos em `core::`. A UI depende do engine.
2. **Zero regressão e determinismo**: mesma seed → mesmo comportamento/estado. O golden
   (`--phase32-checksum`) e o determinismo (`--phase32-selftest`) continuam passando. Esta fase é
   UI/UX + um comando de apply granular; **não muda a física nem o modelo de dados do genoma** (isso
   é a 34.2), então o golden deve permanecer **byte-idêntico**.
3. **Performance**: o editor só lê o genoma da espécie SELECIONADA (custo zero por-frame para as
   outras). O apply por-campo é O(indivíduos vivos da espécie), sem reset global, sem varredura de
   toda a população. Nenhuma alocação por-frame no caminho do dock.
4. **Nada de Python alterado.** Build Debug E Release. Rodar a bateria de selftests.
5. **Não "remendar"**: planeje a estrutura (estado da UI, comandos, métodos do runner) antes de
   escrever. Coeso, não colcha de retalhos.

## Estado esperado antes de iniciar

`git status` limpo na branch de migração; Fases 0–32 + microfases concluídas/comitadas; tema visual
e sistema de comida por sites já no lugar; selftests 7–32 verdes; golden byte-idêntico.

## Escopo — campos por-espécie NESTA fase

Tornam-se por-espécie e **ao vivo** os campos que **já vivem no `GenomeRecord`**: corpo
(tamanho/forma), energia (inicial/split/cap), reprodução (idade mín./cooldown), **dieta** (come
comida/organismos/mesma espécie, eficiências, corpo→comida), **flags de visão** ("o que enxerga":
comida/organismos/predadores/obstáculos/tudo/através de paredes), mutação (taxa/força).

Os campos hoje **globais** (velocidade, virada, locomoção, energia/idade de morte, custos de
locomoção, **geometria** da visão — raio/nº de retinas/FOV/olhos/canais/modo — e a arquitetura
neural) **continuam globais nesta fase**, mas devem ficar **claramente marcados como "global"** no
editor (ex.: um rótulo/ícone e um tooltip "vale para todas as espécies; vira por-espécie na Fase
34.2"). Em **34.2** eles migram para o genoma. NÃO os mova agora.

## Arquitetura — o núcleo técnico (apply granular)

1. **Genoma da espécie = fonte de verdade** dos seus campos por-espécie. O agente já carrega o
   `genomeId`; cada espécie tem um genoma-template (`defaultGenomeId`). Esta fase NÃO cria campos
   novos no genoma — só passa a EDITAR o genoma da espécie de forma granular.
2. **Editor vinculado à espécie selecionada.** Ao selecionar a aba de uma espécie, o editor é
   preenchido com os valores DAQUELE genoma (criar um carregador genoma→buffer-de-edição; pode ser
   um buffer próprio do editor por-espécie em `ui::`, NÃO os parâmetros globais `bacteria_*`). Cada
   caixa mostra o valor atual da espécie.
3. **Estado "sujo" por-campo (laranja).** Ao digitar, a caixa fica **laranja** indicando que o valor
   digitado diverge do genoma da espécie. Ao apertar **Enter** (ou perder o foco confirmando), só
   aquele campo é aplicado.
4. **Comando granular novo:** `core::CmdSetSpeciesGenomeField { simulation::SpeciesId speciesId;
   std::string field; config::ParameterValue value; }`. Handler no engine:
   `SimulationRunner::setSpeciesGenomeField(speciesId, field, value)` que:
   - resolve o genoma-template da espécie (clone-on-write se for compartilhado);
   - escreve **apenas aquele campo** no template (um `switch` field→membro do `GenomeRecord`, ou um
     setter por chave bem encapsulado em `simulation::`);
   - para cada indivíduo VIVO da espécie: escreve o mesmo campo no genoma pessoal (clone-on-write se
     compartilhado) e atualiza o estado derivado do agente quando fizer sentido (raio se body_size,
     forma se body_shape, clamp de energia se energy_cap);
   - O(indivíduos da espécie); sem reset; `rebuildSpatial()` só se o raio mudou.
   - Mantém `dietSnapshot` da espécie em sincronia quando um campo de dieta muda.
5. **Campos da rede neural / geometria de visão (os globais que reconfiguram a rede):** ao editar +
   Enter, **abrir uma janela MODAL de confirmação** ("Isto vai reconstruir os cérebros desta
   espécie — o aprendizado será perdido. Confirmar?"). Só ao confirmar é que aplica + reconstrói
   (reutilize o caminho de `resetNeuralForSpecies`/`syncBrains`). Cancelar reverte o campo. (Nesta
   fase, como a geometria ainda é global, esse modal vale para a arquitetura neural global; a parte
   por-espécie da geometria chega na 34.2 reusando exatamente este modal.)
6. **Remover** os botões "Aplicar à espécie" e "Aplicar aos selecionados" — não fazem mais sentido
   (você já está dentro da espécie). Os comandos `CmdApplyGenomeToSpecies/ToSelected` podem ser
   aposentados ou mantidos só como atalho interno, conforme o que ficar mais limpo.

## Escopo — UI/UX do novo dock de espécies

1. **Dock = abas de espécie.** Uma aba por espécie. A "orelha" da aba (onde aparece o nome) tem a
   **cor da espécie**. Nome **editável, máx. 10 caracteres**. Cabem ~5 abas visíveis; com mais,
   rolagem. Sempre existe uma **aba "+"** ao final: clicar nela cria uma espécie nova.
2. **Aba "+":** cria uma espécie nova, em dois casos (feature real desta fase, não futura):
   - **Com indivíduos selecionados:** os selecionados **viram a nova espécie** e **deixam de
     pertencer à anterior** (a antiga continua existindo com o resto dos seus membros). Reutilize
     `CmdCreateSpeciesFromSelected`/`CmdAssignSelectedToSpecies`. **Não** spawna ninguém — a
     população da nova espécie são exatamente os selecionados.
   - **Sem nada selecionado:** cria a espécie com **genoma default** e **spawna 5 organismos**
     daquele genoma no substrato (5 = a população inicial padrão da espécie; alinhe com o default do
     registry, ex.: `bacteria` min/inicial = 5, em vez de um número mágico solto).
   Ao criar, a nova aba já aparece selecionada com o editor vinculado ao genoma dela. (O caso "com
   seleção" é, de propósito, a semente do futuro "promover um mutante a nova espécie".)
3. **Topo do editor (controles da espécie):** o que hoje está na aba "Labels" sobe para o topo do
   editor — nome (≤10), cor, população (min/max/inicial), mostrar no gráfico, **resetar rede neural**
   desta espécie, **excluir espécie**. À direita desses controles (ao lado dos botões de
   min/max/inicial), um **mini-visor** que desenha **um exemplar da espécie** (corpo + cor), pequeno,
   só para identificar visualmente qual espécie é. (Desenho leve, sem custo relevante.)
4. **Corpo do editor:** os grupos de genoma (Corpo/locomoção, Energia/reprodução, Visão, Dieta, Rede
   neural), agora lendo/gravando o genoma DESTA espécie (campos por-espécie) e marcando os campos
   globais como "global".
5. **Dock retrátil:** botão para recolher/expandir o dock (ganhar espaço de tela). Reaproveite o
   `CmdToggleLeftDock` se existir; a câmera/visão deve reaproveitar o espaço corretamente.
6. **Estética:** caprichado e limpo (o usuário pediu "de forma bonita"). Cores da espécie nas
   orelhas, espaçamento agradável, agrupamento Gestalt mantido, feedback visual coerente com o resto
   (ex.: o "✓ aplicado" que já existe pode reaparecer ao aplicar um campo).

## Escopo — Substrato vira janela do menu superior

1. **Remover a aba "Substrato" do dock.** O dock é só de espécies agora.
2. Adicionar um item **"Substrato"** na barra de menu superior, **entre "Agente" e "Ajuda"**, que
   alterna uma **janela** de Substrato (mesmo padrão das janelas de Preferências). A janela contém os
   parâmetros do substrato/comida que estavam na aba (tamanho do substrato, comida, etc.).
3. Persistência/estado: um flag de janela aberta no `UiState`/`PreferencesState`, como as outras
   janelas.

## Fora de escopo (NÃO fazer agora)

- Mover campos globais (velocidade/morte/geometria de visão) para o genoma — isso é a **Fase 34.2**.
- Qualquer sistema de **mutação** (de escalares ou de neurônios) — projetado mentalmente, não
  implementado, sem placeholder.
- Custo de energia por neurônio, tipos de célula, ecossistema (scavenger/plantas/etc.).
- Otimização de colisão/percepção (alavanca de FPS) — fase própria.

## Coerência com o roadmap (projete pensando nisto, sem implementar)

- O genoma por-indivíduo é a fonte de verdade → a **mutação futura** mexerá nesses mesmos campos.
- A aba "+" a partir da seleção é a base de **promover um indivíduo mutante a nova espécie**.
- O editor por-espécie + mini-visor é a base do futuro **inspetor por-indivíduo** (visão por agente).
- O comando granular `setSpeciesGenomeField` é o mesmo mecanismo que a 34.2 estende para os campos
  que migrarem ao genoma.

## Testes obrigatórios — `--phase34-selftest` (headless)

Criar o selftest da fase (engine + fluxo, sem render), cobrindo no mínimo:
1. `setSpeciesGenomeField` aplica **só** o campo pedido (os demais campos do genoma da espécie ficam
   inalterados) — prova direta da correção da dor.
2. Aplica ao template E aos indivíduos vivos da espécie; não toca em outras espécies.
3. Clone-on-write: editar uma espécie cujo template é compartilhado não afeta as outras.
4. Campo de dieta atualiza o `dietSnapshot` da espécie.
5. Aba "+": sem seleção, cria a espécie e **spawna 5 organismos** do genoma default (população da
   nova espécie = 5); com seleção, os selecionados trocam para a nova espécie e a antiga mantém o
   resto (sem spawn).
6. Determinismo: aplicar o mesmo conjunto de campos por seed dá estado idêntico em duas execuções.
7. (Roteamento de UI headless, se aplicável ao padrão da Fase 31) atalhos/flags do dock e da janela
   de Substrato.

## Build, regressões, golden, documentação

- Build **Debug E Release**.
- Regressões **Fases 7–32** + `--phase34-selftest` verdes.
- **Golden byte-idêntico** (`--phase32-checksum`) e determinismo (`--phase32-selftest`) — esta fase
  não muda física/modelo, então não pode mexer no digest.
- Criar `PHASE_34_1_SPECIES_EDITOR_STATUS.md`: o paradigma novo; a arquitetura do apply granular; o
  que virou por-espécie e o que segue global (marcado); a UI do dock/abas/visor; a janela de
  Substrato; selftests adicionados; confirmação de golden/regressão.

## Saída final esperada

Dock de espécies funcional e bonito: cada espécie com seu editor vinculado, edição por-campo ao vivo
(laranja → Enter aplica só aquele campo), modal de confirmação para campos de rede, Substrato como
janela do menu superior, sem os botões "aplicar". A dor original (mexer num campo e bagunçar os
outros) resolvida. Tudo validado, determinístico, golden intacto — e arquitetado como fundação da
Fase 34.2.
