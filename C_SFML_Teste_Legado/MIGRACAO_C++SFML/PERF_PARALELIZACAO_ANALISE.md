# Análise de Paralelização e Performance — AgentBioSim C++

> Documento de **avaliação** (não de execução). Objetivo: decidir, com critério de
> engenheiro de performance + arquiteto sênior, **o que vale paralelizar**, o que **não**
> vale, e responder em específico: *dá para paralelizar a colisão / jogá-la numa thread
> separada?*
>
> Base de medição: baselines da Fase 29/32 (ver `AgentBioSimCpp/CLAUDE.md`). Os números
> abaixo são **estimativas a re-medir** com o profiler embutido (Janela do Desenvolvedor /
> `--phase27-diagnostics`) no cenário real do usuário antes de investir em qualquer item de
> risco. *Medir primeiro* é parte do plano, não um detalhe.

---

## 0. Sumário executivo (a verdade incômoda primeiro)

**O grosso do trabalho pesado JÁ está paralelizado.** A Fase 32 paralelizou **percepção
(~49% do passo)** e **forward neural (~34%)** — juntos ~83% do tempo de passo a 1000 agentes —
com `std::execution::par`, escritas disjuntas por agente, contadores atômicos e resultado
**byte-idêntico** (golden `--phase32-checksum`). Ganho real medido: **~3x**.

Consequência (Lei de Amdahl) — e este é o ponto central de todo o resto:

| | Pré-Fase 32 | Pós-Fase 32 (estimado) |
|---|---|---|
| Percepção + Neural (paralelo) | 83% | ~49% |
| **Cauda serial** (resto) | **17%** | **~51%** |

> Álgebra: para um speedup total de 3x com fração paralela p=0,83, a parte paralela rodou a
> ~5x efetivo. O passo encolhe para ~0,33 do original; dentro dele, os 17% seriais **viram
> ~metade do passo**. Ou seja: a "cauda serial" que parecia desprezível agora é o gargalo
> dominante. **É lá que está o próximo ganho — não em paralelizar mais a percepção.**

A cauda serial é **fragmentada** em 8 sistemas pequenos, vários deles **inerentemente
sequenciais** (RNG, escritas cruzadas, compactação de store). Não existe um segundo "alvo
gordo" como a percepção. Por isso a maior alavanca **não** é "paralelizar mais um sistema
data-parallel", e sim uma mudança **arquitetural**: **desacoplar a SIMULAÇÃO da
RENDERIZAÇÃO/UI em threads separadas** (Seção 4). Essa é, inclusive, a forma *certa* de
responder ao desejo do usuário de "jogar coisa numa thread separada".

---

## 1. O pipeline de um passo (`SimulationRunner::runOneStep`)

Ordem atual (com a reordenação da microfase 32.4) e natureza dos dados de cada sistema:

| # | Sistema | % passo¹ | Padrão de dados | Paralelizável? |
|---|---------|:---:|-----------------|----------------|
| 1 | **Perception** | ~49% | Lê mundo const; cada agente escreve só sua fatia de `flatInputs` | ✅ **JÁ É** (par) |
| 2 | **Neural** (forward) | ~34% | Cada agente lê sua entrada, muta só seu cérebro, escreve só `controls[i]` | ✅ **JÁ É** (par) |
| 3 | **Movement** | baixo | 1 loop; cada agente escreve só `position/velocity[i]`; sem vizinhança | ✅ Classe A (fácil) |
| 4 | **Energy** | baixo | Custo metabólico por agente; independente | ✅ Classe A (fácil) |
| 5 | **SpatialHash rebuild** | médio? | `push_back` sequencial em buckets compartilhados; **roda 2×/passo** | ⚠️ Classe B |
| 6 | **Interaction** (comer+predação) | médio? | **Claim compartilhado** (`consumedFoodSet`): quem itera primeiro come | ⚠️ Classe B |
| 7 | **Collision** | dado-dependente | **Gauss-Seidel in-place**: resolver (i,j) muta os DOIS na hora | ⚠️ Classe B (difícil) |
| 8 | **Food** (spawn/trim) | baixo | RNG sequencial semeado; ordem de sorteio importa | ⚠️ Classe B |
| 9 | **Reproduction** | esparso | RNG por filho + clona cérebro + append no store | ❌ serial por natureza |
| 10 | **Death** | esparso | Scan (paralelizável) + swap-remove (compactação serial) | ❌ serial por natureza |

¹ Percentuais de percepção/neural são da baseline da Fase 29. Os "baixo/médio" são hipóteses
a confirmar com o profiler — **dependem fortemente do cenário** (densidade, raio de visão,
nº de agentes, colisão ligada/desligada).

**Cadeia de dependências dura** (não dá para reordenar livremente): `Movement → rebuild →
Interaction → Collision → Food/Repro/Death → rebuild → (próximo passo) Perception`. A
predação **precisa** rodar antes da colisão (32.4), e a colisão escreve as posições que a
percepção do próximo passo e o render leem. Guarde isto para a Seção 3.

---

## 2. A regra que governa tudo: determinismo (Fase 32)

> "Qualquer otimização futura deve manter a **ordem de FP por agente** (sem SIMD em reduções,
> sem threading com escrita cruzada) **ou refazer a prova de regressão-zero**" (`--phase32-checksum`).

Classifico cada proposta por isto:

- **Classe A — byte-idêntica por construção.** Escritas disjuntas por agente, sem RNG, sem
  reordenar redução. O golden continua passando **sem reprovar nada**. (Movement, Energy,
  buffers thread_local, split Sim/Render.)
- **Classe B — exige um GOLDEN NOVO.** Muda a numérica ou a ordem (algoritmo diferente, claim
  paralelo, ordem de bucket, ordem de RNG). Não está "errado", mas **muda o comportamento** e
  precisa de: nova baseline de checksum + benchmark antes/depois + (quando muda comportamento
  visível, ex.: colisão) validação visual + knob para manter o caminho serial como referência.

Isto não é burocracia: é o que protege a reprodutibilidade-por-seed, que é uma propriedade de
**usabilidade científica** da simulação (o usuário pediu "sem perder usabilidade").

---

## 3. A pergunta da colisão (resposta direta + funda)

Há **duas** interpretações do pedido. Elas têm respostas opostas.

### 3a. "Jogar a colisão numa thread SEPARADA (assíncrona)" — ❌ **Não.**

A colisão está **no meio da cadeia de dependência dura**. Ela lê as posições que o Movement
acabou de escrever, **tem de** rodar depois da predação (32.4), e **escreve** as posições que
(a) a percepção do próximo passo lê e (b) o render desenha. Rodá-la "em paralelo com o resto"
significaria a percepção/coletor espacial lerem posições que a colisão ainda está mutando →
**corrida de dados + quebra de determinismo**, e a separação dos corpos não estaria refletida
quando o agente percebe o mundo. Além disso **não há trabalho independente** para sobrepor a
ela (todo o resto do passo depende do resultado dela ou de uma entrada dela). Custo de
sincronização > qualquer ganho. **Esta ideia específica não compensa.**

> A intuição "thread separada" do usuário **está certa** — só que o corte certo não é a
> colisão; é a **simulação inteira vs. render/UI** (Seção 4). Mesmo desejo, lugar onde a
> dependência permite e o determinismo sai de graça.

### 3b. "Paralelizar a colisão INTERNAMENTE (dentro do slot dela)" — ⚠️ possível, Classe B, só se for hotspot.

Por que é difícil (ver `CollisionSystem.cpp:204-326`): o agente-agente é **Gauss-Seidel
in-place** — resolver o par (i,j) **muta as posições de i e de j na hora**, e o par seguinte
já enxerga as posições atualizadas; a deduplicação usa um `std::unordered_set seenPair`
compartilhado. Isso é **escrita cruzada + dependência de ordem** simultâneas: o oposto da
percepção. Paralelizar exige **trocar o algoritmo**:

- **Opção 1 — Coloração espacial (checkerboard / 4–9 cores).** Particiona o mundo em células
  de tamanho ≥ raio de interação; células de uma mesma "cor" estão distantes o bastante para
  que dois agentes nelas **nunca** se toquem → resolve todas as células de uma cor em
  paralelo, barreira, próxima cor. Pares na fronteira entre cores precisam ser atribuídos a
  **exatamente uma** cor de forma determinística. Mantém uma semântica parecida com
  Gauss-Seidel, mas **não** byte-idêntica à ordem-de-store atual → golden novo.
- **Opção 2 — Jacobi / Position-Based Dynamics.** Tira um snapshot das posições, calcula
  todas as correções a partir do snapshot read-only, **acumula o deslocamento por agente**
  (cada agente dono do seu acumulador) e aplica num passe único (ou K iterações). Totalmente
  paralelo. Porém a separação por passe fica mais "mole" que o Gauss-Seidel (resíduo de
  sobreposição maior em pilhas densas) — **mudança visível** + golden novo.

**Pré-requisito honesto: medir.** Com o spatial hash a colisão é O(n·k) (k = vizinhos
locais); em densidade normal ela é uma fatia pequena da cauda serial. Paralelizá-la
perfeitamente seria uma fração pequena de uma fração pequena — **a menos que** o usuário rode
cenas patológicas (milhares de agentes amontoados num mundo minúsculo), onde k explode e a
colisão vira hotspot real. **Só investir aqui se o profiler mostrar que a colisão dói** no
cenário dele.

**Degrau barato antes de paralelizar (Classe A, serial):** hoje a colisão aloca um `cand` e um
`seenPair` (hash set que cresce com o nº de pares) por passo. Dá para **eliminar o set**
resolvendo cada par só pela ótica do agente de menor índice (i<j) — menos alocação, menos
trabalho, e deixa o terreno pronto para a coloração. Isto é seguro e pode já valer sozinho.

---

## 4. A maior alavanca: desacoplar SIMULAÇÃO de RENDER/UI (Classe A) ⭐

Hoje o laço é **serial numa thread só** (`App::run`): `processEvents → update (até
maxStepsPerFrame passos de sim) → render → repete`. Sim e render **não se sobrepõem**. Em
1x (poucos passos por frame), o render (draw SFML + ImGui, que com muitos agentes e as janelas
de Métricas/Dev custa vários ms) é uma fatia grande e **bloqueia** a simulação.

A arquitetura **já está pronta** para o padrão produtor/consumidor com double/triple buffer:
- Engine é **headless** e determinístico; a UI **já** fala com ele por `core::CommandQueue`
  (fila de comandos) — não muta stores direto.
- Já existe captura de snapshot de posições para interpolação de render
  (`captureRenderPrevPositions`).

Desenho proposto:
- **Thread de simulação:** roda `runOneStep` (exatamente igual, mesma ordem → **golden
  byte-idêntico de graça**), e ao fim de cada passo **publica um snapshot imutável** (posições,
  cores, overlay do selecionado) num back-buffer; troca por ponteiro atômico (triple buffer =
  lock-free) ou lock curtíssimo.
- **Thread principal (render+UI+eventos):** dona do contexto SFML/ImGui (que **não** são
  thread-safe — por isso ficam juntos na main), lê o último snapshot publicado, desenha, trata
  eventos e **empurra comandos** na fila que a thread de sim drena nas bordas de passo.

**Ganho:** render e simulação passam a rodar **concorrentes**. Em 1x num multicore, aproxima
de **dobrar o throughput efetivo** (sim e render sobrepostos), e a UI fica **mais responsiva**
sob carga — ganho de *usabilidade*, não perda. E **sobrepõe a cauda serial inteira** (Seção 1)
com o render, então ajuda independentemente da composição interna do passo.

**Custos/riscos (gerenciáveis):** handoff thread-safe do snapshot; ImGui+SFML confinados à
main; `Save/Load/reset/pick` precisam de um ponto de quiescência (parar a thread de sim 1
passo) — a fila de comandos já é o lugar natural para isso. Determinismo **intacto** (a sim
roda sozinha, na mesma ordem). É a melhor relação ganho/risco do documento.

---

## 5. Outras alavancas (um perf engineer não para no "threading")

- **`output` thread_local no NeuralSystem** (`NeuralSystem.cpp:101`): hoje aloca um
  `std::vector<double> output` por agente por passo. Tornar `thread_local` (como o `input` já
  é) elimina N alocações/passo. Classe A, trivial, ganho pequeno mas grátis.
- **Overhead de dispatch do `std::execution::par`.** No MSVC ele usa o thread pool do Windows;
  cada chamada tem custo fixo (µs a dezenas de µs). Para loops **baratos** (Movement/Energy a
  1000 agentes podem ser ~dezenas de µs seriais), paralelizar pode ser **net-negativo**. Por
  isso: gate por threshold mais alto **e benchmark** — não paralelize por reflexo.
- **Fundir Movement+Energy num único `par`** após o neural (ambos Classe A, independentes)
  reduz o overhead de dispatch a um só. Avançado, faça depois de medir.
- **Banda de memória, não CPU, a 5000 agentes.** A percepção paralela pode estar
  *bandwidth-bound* nesse ponto — mais threads não ajudam; reduzir bytes tocados / blocar
  cache, sim. Confirmar com profiler antes de assumir "é CPU".
- **Rebuild do spatial hash 2×/passo:** se o profiler acusar custo, um *counting-sort*
  paralelo que **preserva a ordem intra-bucket** (por índice de store) é viável (Classe B —
  a ordem de bucket afeta empates de "mais próximo" na percepção e a ordem de pares na
  colisão; precisa golden novo).

---

## 6. Roteiro priorizado (ganho × risco × esforço)

| Pri | Item | Classe | Esforço | Ganho | Determinismo |
|:---:|------|:---:|:---:|:---:|---|
| 0 | **Medir** com o profiler no cenário real (densidade/visão/nº agentes) | — | XS | — | — |
| 1 | **Split Sim ↔ Render/UI** (thread da simulação + snapshot) | A | M–L | **Alto** | Intacto (grátis) |
| 2 | `output` thread_local no neural + auditar allocs por passo | A | XS | Baixo | Intacto |
| 3 | Paralelizar **Movement+Energy** (threshold + benchmark; talvez fundidos) | A | S | Baixo–Médio | Intacto |
| 4 | **Colisão serial mais barata** (remover `seenPair`, usar i<j) | A | S | Baixo–Médio | Intacto |
| 5 | **Rebuild do spatial hash** paralelo (se for hotspot) | B | M | Médio | Golden novo |
| 6 | **Colisão paralela interna** (coloração ou Jacobi) **se for hotspot** | B | L | Dado-dep. | Golden novo + validação visual + knob |
| 7 | **Interaction** com resolução de claim em 2 fases | B | L | Médio | Golden novo |
| — | Food / Reproduction / Death | B/❌ | — | Baixo | Não compensa (RNG/estrutural) |

**Sequência recomendada:** 0 → 1 → (2,3,4 como ganhos baratos Classe A) → re-medir → só então
decidir 5/6/7 com dados na mão. Não comece pela colisão: ela é o **item 6**, condicional a
medição, justamente o oposto da intuição inicial.

---

## 7. O que NÃO fazer (para fechar o escopo com honestidade)

- ❌ Colisão em thread separada/assíncrona (Seção 3a) — quebra a cadeia de dependência.
- ❌ SIMD em reduções da percepção/neural — proibido pela regra da Fase 32 (mudaria ordem FP).
- ❌ Paralelizar Food/Repro/Death reordenando o RNG — quebra determinismo-por-seed por ganho
  desprezível.
- ❌ Paralelizar Movement/Energy **sem** benchmark — o overhead de dispatch pode comê-los.
- ❌ Qualquer item Classe B sem o golden novo + benchmark antes/depois.

---

## 8. Decisão pendente do usuário

1. **Confirmar o alvo nº 1**: encaro o **split Sim/Render** como a próxima fase de performance
   (maior ganho, determinismo grátis)? Ou o foco é mesmo a **colisão** (que recomendo só após
   medir, e que muda comportamento)?
2. **Cenário de referência** para medir: quantos agentes, qual densidade, colisão ligada,
   quais flags de visão? Isso define se a colisão é hotspot ou ruído.
3. Itens Classe B exigem aceitar **golden novo**. Topa, quando chegarmos neles?
