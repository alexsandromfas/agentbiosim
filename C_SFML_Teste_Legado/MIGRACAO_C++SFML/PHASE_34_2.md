# Fase 34.2 — Genoma completo: todos os traços hereditários por indivíduo

Atue como arquiteto sênior de software C++, com foco igual em **performance data-oriented**,
**determinismo** e **coesão arquitetural**. Esta fase fecha o paradigma iniciado na 34.1: o
**genoma passa a ser a fonte única de TODOS os traços hereditários** de um organismo — não sobra
nenhum traço "global" escondido nos `bacteria_*`. É a fundação técnica do roadmap (mutação por
indivíduo de qualquer traço, promover indivíduo a espécie, custo por neurônio, ecossistema).
**Sem implementar mutação, custo por neurônio ou placeholders agora** — apenas a estrutura.

## Visão (por que esta fase existe)

Depois da 34.1, alguns traços ainda são **globais** (lidos dos parâmetros `bacteria_*` pelos
sistemas): velocidade/virada/locomoção, energia/idade de morte, custos de locomoção e a
**geometria de visão** (raio, nº de retinas, FOV, olhos, canais, modo). Enquanto eles forem
globais, duas espécies **não podem** ter velocidades/visões diferentes, e a mutação futura não tem
onde escrever. Esta fase move esses traços para dentro do `GenomeRecord` (por indivíduo) e faz os
sistemas lerem **do genoma do agente**, não de um parâmetro global.

> Princípio: **o genoma = todo o DNA herdável, por indivíduo. A espécie = um rótulo + um
> genoma-template.** Os parâmetros globais `bacteria_*` viram apenas **defaults de fábrica** usados
> ao criar um genoma novo — nunca mais lidos no loop de simulação.

## Regras invioláveis

1. **Engine headless é sagrado** (`src/sim`, `src/simulation`, `src/systems`, `src/neural`,
   `src/perception`): nada de SFML/ImGui/`ui::`. A UI depende do engine.
2. **Determinismo absoluto**: mesma seed → mesmo estado. As leituras por-agente são independentes
   entre agentes (sem dependência cruzada) → ordem de ponto-flutuante preservada. `--phase32-selftest`
   continua verde.
3. **Golden — mudança INTENCIONAL e justificada.** A meta é: se os **defaults do genoma forem
   exatamente os valores globais antigos**, cada agente lê do seu genoma o MESMO valor que lia do
   global → o comportamento é idêntico e o golden permanece **byte-idêntico**. Persiga isso. Só
   se, por um motivo técnico inevitável (ex.: ordem de leitura), o digest mudar, regenere o golden
   como mudança **documentada e revisada**, provando equivalência comportamental. **Não** aceite
   divergência silenciosa.
4. **Performance — não pode piorar.** O agente já tem `genomeId`; ler campos do genoma é um lookup
   indexado, cache-friendly. Os caminhos quentes (Movement/Energy/Death) passam a ler alguns
   `double` a mais do genoma — custo desprezível, **zero alocação por passo**. Ver a seção
   "Performance e a otimização por assinatura" — é o ponto crítico desta fase.
5. **Nada de Python alterado.** Build Debug E Release. Bateria de selftests completa.
6. **Incremental e planejado**: migre um traço/sistema de cada vez, buildando e rodando o
   `--phase32-checksum` a cada passo para flagrar na hora qualquer divergência de golden. Não
   "remendar" — planejar o `GenomeRecord` final, o save/load e o bootstrap antes de cortar.

## Estado esperado antes de iniciar

Fase 34.1 concluída e comitada (dock por-espécie, apply granular, Substrato em janela, modal de
rede). Selftests 7–34.1 verdes; golden byte-idêntico.

## Escopo — traços que migram para o `GenomeRecord`

Adicionar ao `GenomeRecord` (por indivíduo), com defaults = valores globais atuais:

- **Locomoção:** `maxSpeed`, `maxTurn`, `allowReverse`, `movementMode` (e quaisquer parâmetros
  do modo de locomoção que hoje sejam globais).
- **Custos de locomoção:** `moveCostV0`, `moveCostVmax` (custo de energia por velocidade).
- **Morte:** `deathEnergy` (limiar de energia), `deathByAgeEnabled`, `deathAge`.
- **Geometria de visão:** `visionRadius`, `retinaCount`, `retinaFov`, `eyeCount`, `eyeAngle`,
  canais de retina (RGB/mono) e `visionInputMode` — tudo o que define o **formato da entrada** da
  rede e o **alcance/ângulo** da percepção.

Manter os que já são do genoma (corpo, energia, reprodução, dieta, flags de visão, mutação, brain
config). Resultado: o `GenomeRecord` descreve **100%** do organismo.

## Arquitetura — rewire dos sistemas para ler por-genoma

1. **MovementSystem** lê `maxSpeed/maxTurn/allowReverse/movementMode` do genoma do agente (via
   `genomeId` → `GenomeStore`), não do config global. Assinatura/loop data-oriented: resolver o
   genoma uma vez por agente no início da iteração daquele agente.
2. **EnergySystem** lê `moveCostV0/moveCostVmax` (e o que for de custo herdável) do genoma.
3. **DeathSystem** lê `deathEnergy/deathByAgeEnabled/deathAge` do genoma (preservando o piso por
   espécie e a ordenação determinística já existentes).
4. **PerceptionSystem** lê a **geometria** (raio/retinas/FOV/olhos/ângulo/canais/modo) do genoma do
   agente. O loop de retinas já existe; passa a usar a contagem/FOV/raio **por-agente**. A entrada
   da rede daquele agente passa a ter o tamanho ditado pelo seu genoma.
5. **Defaults de fábrica:** o bootstrap (`SpeciesBootstrap`/`overwriteGenomeScalarsFromRegistry`)
   passa a copiar TAMBÉM esses novos campos dos `bacteria_*` para o genoma-template ao criar uma
   espécie. No loop de simulação, os `bacteria_*` desses traços **não são mais lidos** — só servem
   de default ao instanciar genomas novos.
6. **Save/load:** serializar os campos novos do `GenomeRecord` (versão do formato bumpada com
   retrocompatibilidade de leitura, se o projeto mantiver saves antigos; senão, documentar a quebra).
7. **Editor por-espécie (34.1):** os campos que eram "global (34.2)" deixam de ser marcados como
   globais e passam a ser **por-espécie ao vivo**, usando o MESMO `setSpeciesGenomeField` da 34.1.
   Os campos de **geometria de visão** alteram o formato da entrada da rede → ao editá-los, dispara
   o **modal de confirmação da 34.1** (reconstrói os cérebros da espécie). Reuso direto, sem
   mecanismo novo.

## Performance e a otimização por assinatura (PONTO CRÍTICO)

A Fase 32 agrupa o forward neural por **assinatura de arquitetura** (agentes com a mesma topologia
processam em lote). A geometria de visão por-genoma muda o **tamanho da entrada** → entra na
assinatura. Implicações que você DEVE respeitar:

- Agentes com a **mesma geometria** continuam no mesmo lote (caso comum: indivíduos de uma espécie
  compartilham geometria). A otimização **se preserva** enquanto a geometria for uniforme dentro da
  espécie — que é o estado normal sem mutação.
- A assinatura de lote deve passar a **incluir** os parâmetros de geometria que afetam o formato da
  entrada (não só a topologia interna). Garanta que dois agentes só caem no mesmo lote se a entrada
  tiver o mesmo formato.
- **Não** introduza alocação por passo ao montar lotes/entradas variáveis: reaproveite buffers,
  agrupe por assinatura, evite `std::vector` temporários por agente no caminho quente. Meça com o
  benchmark (`--phase29-bench`) antes/depois: Perception e Neural **não podem regredir** em
  cenários de geometria uniforme.

## Fora de escopo (NÃO fazer agora)

- **Mutação** de qualquer traço (escalar ou de neurônios). A estrutura deve TORNAR trivial mutar
  qualquer campo do genoma no futuro, mas **nada** de mutação é implementado — nem placeholder.
- **Custo de energia por neurônio** (virá quando houver mutação de topologia para conter o inchaço
  NEAT). Só não atrapalhe: o genoma já terá o brain config acessível por-agente.
- **Promover indivíduo a espécie** como ação nova (a base — genoma completo por-indivíduo — fica
  pronta aqui; a ação fica para depois).
- **Ecossistema** (scavenger, plantas fotossintéticas, herbívoros/carnívoros, comunicação por cor,
  reprodução por tempo). A primitiva de comida já existe (sites); os canais de retina por-genoma
  habilitam comunicação por cor no futuro — mas nada disso é implementado agora.
- **Otimização de colisão/percepção** (a alavanca de FPS) — fase própria.

## Coerência com o roadmap (projete pensando nisto, sem implementar)

- **Mutação geral:** com 100% dos traços no genoma, uma futura `mutate(genome, rng)` poderá tocar
  qualquer campo (inclusive ângulo de retina) sem mexer em nenhum sistema. Deixe a escrita de campos
  do genoma centralizada (o mesmo setter por-chave da 34.1) para a mutação reusar.
- **Promover a espécie:** já é "rotular o genoma de um indivíduo" — trivial sobre esta base.
- **Custo por neurônio:** o brain config por-agente fica acessível ao EnergySystem no futuro.
- **Ecossistema:** geometria/canais/dieta por-genoma são os tijolos de papéis ecológicos distintos.

## Testes obrigatórios — estender `--phase34-selftest` (headless)

Adicionar blocos cobrindo:
1. Cada traço migrado é lido **do genoma**, não do global: criar duas espécies com
   `maxSpeed` (e morte, e geometria de visão) diferentes e provar comportamento/percepção distintos
   no mesmo mundo.
2. Equivalência de golden: com defaults do genoma = globais antigos, um passo a passo bate com o
   comportamento pré-34.2 (ou justificar e regenerar o golden, provando equivalência).
3. Determinismo: mesma seed, duas execuções, estado idêntico, com espécies de traços diferentes.
4. Save/load round-trip preserva todos os campos novos do genoma.
5. Editar geometria de visão por-espécie via `setSpeciesGenomeField` redimensiona a entrada da rede
   e dispara a reconstrução (cérebros resetados só daquela espécie).
6. Performance (assinatura): agentes de geometria uniforme continuam em um único lote (sem
   fragmentação indevida do batch neural).

## Build, regressões, golden, benchmark, documentação

- Build **Debug E Release**.
- Regressões **Fases 7–34.1** + `--phase34-selftest` (estendido) verdes.
- `--phase32-checksum`: **byte-idêntico** se defaults = globais; caso contrário, golden regenerado
  com justificativa documentada de equivalência.
- `--phase29-bench` antes/depois: Perception e Neural **sem regressão** em geometria uniforme;
  anexar números no status.
- Criar `PHASE_34_2_FULL_GENOME_STATUS.md`: quais traços migraram; o rewire de cada sistema; a
  estratégia de golden (e o resultado); a análise da otimização por assinatura com números de
  benchmark; o save/load; como isso habilita (sem implementar) mutação/promoção/ecossistema.

## Saída final esperada

O `GenomeRecord` descreve o organismo inteiro; nenhum traço herdável é lido de global no loop de
simulação (os `bacteria_*` viram só defaults de fábrica). Espécies podem ter velocidade, morte e
visão diferentes, todas editáveis ao vivo pelo editor por-espécie da 34.1 (geometria de visão via
modal de confirmação). Determinismo intacto, golden byte-idêntico (ou regenerado com prova),
performance preservada (otimização por assinatura respeitada). A base para mutação por indivíduo,
promoção a espécie e ecossistema está pronta — sem nada disso implementado ainda.
