# Fase 34.2 — Genoma completo (traços hereditários por indivíduo) — STATUS

## Resumo executivo

Migrados para o `GenomeRecord` (por indivíduo) os traços **escalares** que eram globais e que
**não alteram a arquitetura da rede neural**: locomoção (velocidade/virada/marcha-ré), custos
metabólicos (v0/vmax) e energia de morte. Os sistemas Movement/Energy/Death passam a ler esses
valores **do genoma de cada agente**. **Golden `--phase32-checksum` BYTE-IDÊNTICO**; determinismo,
regressão 7–34 e performance preservados.

**PENDENTE (grupo 2, decisão de risco do usuário):** a **geometria de visão** (nº de retinas, olhos,
canais, modo de entrada, FOV, ângulo dos olhos) e o **modo de locomoção** — os traços que mudam o
**tamanho da entrada/saída** da rede e exigem reescrever o layout do `PerceptionResult` + o batching
neural para arquiteturas heterogêneas. Ficam globais (marcados "global" no editor) até o usuário
decidir tomar esse risco (ver "Decisão pendente" no fim).

## Por que a divisão

`PerceptionResult` é um buffer **flat com stride uniforme** (`inputForAgent(i) = data + i*inputSize`)
e o `NeuralSystem` agrupa cérebros por **assinatura de arquitetura** assumindo I/O uniforme.
`outputSizeForMovementMode` mostra que `movement_mode` muda o nº de saídas. Logo:

- **Grupo 1 (feito):** escalares puros — `maxSpeed`, `maxTurn`, `allowReverse` (Movement),
  `moveCostV0`/`moveCostVmax` (Energy), `deathEnergy` (Death) + `energyCap` lido por-agente (corrige
  o cap por-espécie da 34.1). São leituras por-agente de poucos `double` → **byte-idêntico** (defaults
  = globais), determinismo intacto, **sem tocar percepção nem batching neural**.
- **Grupo 2 (pendente):** `retina_count`/`eye_count`/canais/`input_mode` (tamanho da entrada),
  `movement_mode` (tamanho da saída), `fov`/`eye_angle` (direções dos raios → RayCache). Tornar
  por-agente exige máquina de arquiteturas heterogêneas (offsets por-agente no PerceptionResult,
  assinatura de lote incluindo a geometria, cérebros dimensionados pelo genoma, repro/save/load/
  editor coerentes) — exatamente onde mora o risco a determinismo/performance.

`death_by_age_enabled`/`death_age` **não foram migrados**: nenhum sistema os consome hoje (seriam
placeholders — o prompt proíbe). Seguem como parâmetros globais inertes, marcados "global" no editor.

## O que mudou (grupo 1)

- **`GenomeRecord`** (+6 campos): `maxSpeed=300`, `maxTurn=π`, `allowReverse=false`,
  `moveCostV0=0.5`, `moveCostVmax=8.0`, `deathEnergy=50` (defaults = defaults globais antigos).
- **`overwriteGenomeScalarsFromRegistry`**: semeia os 6 do registry por `prefix` (bacteria/predator).
  Os `bacteria_*`/`predator_*` viram **defaults de fábrica**; o loop não os lê mais.
- **MovementSystem::apply(..., genomes)**: por-agente, `MovementConfig cfg = config` com
  `maxSpeed/maxTurn/allowReverse` do genoma; resto (mode, inércia, smooth) segue global.
- **EnergySystem::apply(..., genomes)**: por-agente `v0Cost/vmaxCost/vmaxRef(=maxSpeed)/energyCap`.
- **DeathSystem::apply(..., species, genomes)**: limiar de inanição por-agente (`deathEnergy`).
- `runOneStep` passa `&genomes_` aos três. `nullptr` = fallback global (selftests standalone).
- **`GenomeFields`**: os 6 entram no bridge (`max_speed`, `max_turn`, `allow_reverse_locomotion`,
  `metab_v0_cost`, `metab_vmax_cost`, `death_energy`) → o editor por-espécie da 34.1 os edita **ao
  vivo** (deixam de ser "global"). Corrigido o bug latente: o editor usava `v0_cost`/`vmax_cost`
  (inexistentes no registry) — agora `metab_v0_cost`/`metab_vmax_cost` (as chaves reais), com rótulos.
- **Save/load**: `genomeToJson`/`genomeFrom` serializam os 6 (leitura com defaults → saves antigos
  carregam sãos). Reprodução herda automaticamente (cloneFrom copia o record inteiro).

## Determinismo, golden e performance

- **Golden `--phase32-checksum` BYTE-IDÊNTICO** ao baseline (cada bacteria lê do genoma o mesmo valor
  que lia do global; cenários do golden não têm predadores, então a única diferença por-prefixo —
  custo metabólico do predador — não os afeta). `--phase32-selftest` (determinismo) PASS.
- **Regressão 7–34 PASS** Debug+Release, incluindo os selftests com predador (mudar o custo
  metabólico do predador para o dele próprio — antes ele usava, por bug histórico, o do bacteria —
  não quebrou nenhum teste). `--phase34-selftest` = **26 checks** (15 da 34.1 + 11 novos:
  Movement/Death leem por-genoma comprovado por comportamento — max_speed=0 congela, death_energy
  alto extingue —, save/load preserva os campos, determinismo com espécies de traços distintos).
- **Performance:** leituras por-genoma = 1 lookup indexado + cópia de config na pilha por agente
  nos sistemas Movement/Energy/Death (que somam <8% do passo). **Zero alocação por passo.** Benchmark
  `--phase29-bench` (depois): Movement 1.3–5.2%, Energy ~1%, Death ~1%; **Perception 22–37% e Neural
  7.5–42% inalterados** (não foram tocados) — coerente com o baseline. Sem regressão.

## Como habilita o roadmap (sem implementar)

- **Mutação:** os 6 traços novos já passam pelo `setGenomeField`; uma futura `mutate(genome,rng)`
  toca qualquer um sem mexer nos sistemas.
- **Promover indivíduo a espécie:** trivial (o genoma já carrega 100% dos traços escalares).
- **Custo por neurônio / ecossistema:** brain config + dieta + visão-flags já por-genoma.

## Decisão pendente (grupo 2)

Para o `GenomeRecord` descrever **100%** do organismo falta a **geometria de visão** + `movement_mode`.
Isso muda o tamanho da entrada/saída da rede → exige reescrever o `PerceptionResult` (offsets
por-agente) e o batching neural (assinatura incluindo a geometria), mantendo o caso uniforme
byte-idêntico e sem regressão de performance. É a parte de maior risco a determinismo/FPS. Aguarda a
decisão do usuário: fazer agora (sub-fase focada, com benchmark antes/depois) ou deixar para depois.
