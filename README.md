## AgentBioSim V1.0.0 – Simulação de Bactérias e Predadores 2D

Simulador evolutivo/ambiental em Python onde populações de bactérias e predadores interagem num substrato 2D (retangular ou circular), consumindo comida, gastando energia para se locomover e se reproduzindo conforme um modelo metabólico contínuo. Cada agente percebe o ambiente via um sensor de retina multi‑raios, processa entradas com uma rede neural feedforward mutável e atua através de um sistema de locomoção parametrizado. A energia regula morte e reprodução.

### Destaques atuais
- UI moderna PyQt6 com embed de visualização Pygame (fallback Tkinter disponível).
- Modelo energético contínuo (custo v=0 e custo v=vmax) com introspecção automática nas exportações.
- Exportação / importação de substrato (snapshot completo) manual e automática (autosave rotacionado por data).
- Exportação e importação de agentes individuais (genótipo/arquitetura + pesos + estado básico).
- Aplicação automática de todos os parâmetros UI imediatamente antes de cada export (garante snapshots consistentes mesmo sem clicar “Aplicar”).
- Sistema de retina configurável (nº de raios, FOV, filtros de tipos, skip frames, raio de visão).
- Ajuste dinâmico do tamanho de entrada da rede neural quando sensores mudam (resize_input).
- Processamento em lote de agentes (batch forward) + cache multi‑arquitetura (quando habilitado) para performance.
- Spatial Hash reutilizável com atualização incremental para queries rápidas.
- Profiler interno por seção (ativável) e métricas de CPU/RAM (com psutil ou fallback).
- Renderização com duas estratégias: simples (rápida) e elipses detalhadas; alternável em tempo real.
- Limites suaves de população, reprodução controlada e morte limitada por frame.
- Mundo circular ou retangular com câmera com zoom focal e ajuste para caber mundo.

### Requisitos
- Python 3.10+ (tipagem e sintaxe modernas)
- Dependências (pip): pygame, PyQt6, psutil, numpy
  (ver `requirements.txt`)

Instalação (Windows PowerShell):
```powershell
python -m venv .venv; .\.venv\Scripts\Activate.ps1; pip install -r requirements.txt
```

### Executando
```powershell
python main.py           # tenta PyQt6, fallback Tk
python main.py --ui qt   # força PyQt6
python main.py --ui tk   # força Tkinter
```

### Controles (Pygame)
- Mouse Esquerdo: selecionar agente ou adicionar comida
- Mouse Meio: adicionar bactéria
- Mouse Direito + arrastar: mover câmera (ou spawn de protótipo carregado se existir)
- Scroll: zoom focado no cursor
- Espaço: pausa / continua
- R: reset população
- F: enquadra mundo
- T: alterna renderer simples/detalhado
- V: (na UI) alterna ativações / visão do agente selecionado
- WASD / Setas: mover câmera

### Fluxo geral de arquitetura
1. `main.py` constrói `Params`, `World`, `Camera`, `Engine` (headless) e a view Pygame.
2. UI PyQt6 (`sim/ui.py`) cria abas de parâmetros (Simulação, Substrato, Bactérias, Predadores, Teste) e se conecta ao engine.
3. Loop de render roda dentro de `PygameView`; engine executa passos fixos (substeps) com `time_scale`.
4. Sistemas (`systems.py`) aplicam interação (comer/predar), reprodução, morte e colisões.
5. Sensoriamento em lote + forward NN -> locomoção -> energia -> atualização de listas.
6. Exportações capturam: params aplicados, valores brutos de UI, mundo, câmera, comida e estado completo dos agentes (incluindo pesos NN e atributos dinâmicos do modelo energético).

### Principais módulos
- `sim/engine.py`: loop, comandos thread‑safe, batch update, métricas, spawn por protótipo.
- `sim/ui.py`: interface PyQt6, parâmetros, export/import (substrato e agente), autosave.
- `sim/entities.py`: hierarquia `Entity`, `Agent`, `Bacteria`, `Predator`, atualização em lote, reprodução e mutação.
- `sim/brain.py`: `NeuralNet` + funções auxiliares (batch, mutação, resize de input, caching multi‑brains).
- `sim/sensors.py`: `RetinaSensor`, raycasts, `SceneQuery` com/sem spatial hash.
- `sim/actuators.py`: `Locomotion` (steer + velocidade normalizados) e `EnergyModel` contínuo.
- `sim/systems.py`: Interação (comer), Reprodução (limites), Morte (fila limitada), Colisão.
- `sim/spatial.py`: `SpatialHash` eficiente reutilizável.
- `sim/render.py`: estratégias de render + overlay de métricas e detalhes de agente.
- `sim/profiler.py`: profiler de seções habilitável.
- `sim/controllers.py`: `Params` (validação, callbacks), `FoodController` (reposição suave), `PopulationController` (limites).
- `sim/game.py`: `PygameView` (input + loop de render).

### Parâmetros & UI
Todos os widgets registrados entram no CSV (`ui_params.csv`) e são aplicados em bloco via “Aplicar TODOS” ou implicitamente nas exportações. Campos com sufixo `_deg` são convertidos para radianos internamente. Parametrizações de rede neural (número de camadas e neurônios) ajustam dinamicamente a arquitetura dos cérebros novos e em reprodução.

### Exportação / Importação
Substrato:
- Manual: botão “Exportar Substrato” (gera arquivo em `substrates/manual_exports`).
- Automática: se habilitada, salva snapshots sequenciais em `substrates/auto_exports/<data>/` com numeração incremental.
- Snapshot inclui: versão, timestamp, params aplicados, valores brutos da UI, mundo, câmera, estatísticas básicas, comida e lista de agentes.
- Modelo energético exportado dinamicamente por introspecção (`energy_<campo>`), preservando também chaves legacy para compatibilidade.

Agente individual:
- Exporta tipo, atributos físicos, energia, idade, arquitetura NN, pesos/bias, sensor, locomoção e energia.
- Importação registra protótipo; clique direito insere instâncias no substrato.

### Modelo Energético Contínuo
`cost(v) = v0_cost + ( clamp(v,0,vmax_ref) / vmax_ref ) * (vmax_cost - v0_cost)`
- Campos: `death_energy`, `split_energy`, `v0_cost`, `vmax_cost`, `vmax_ref`, `energy_cap`.
- Atualizados dinamicamente a partir de `Params` (bactérias/predadores) a cada passo.
- Reproduzir divide energia internamente ao meio antes de criar o filho.

### Performance & Otimizações
- Batch forward de múltiplos agentes (reduz overhead Python por passo).
- Reaproveitamento de Spatial Hash opcional (`reuse_spatial_grid`).
- Skip de retina (`retina_skip`) e toggle de render simples para populações grandes.
- Activations só para agente selecionado (ou desligadas totalmente) reduzindo custo.
- Profiler interno para identificar gargalos e emitir snapshot textual.
- Métricas de CPU/RAM usando psutil (com fallback manual simples em Windows se ausente).

### Extensão / Customização
Adicionar novo sensor: implementar método `sense(agent, scene, params)` retornando lista de floats e ajustar a montagem de inputs da NN.
Novo renderer: herdar de `RendererStrategy` e substituir em tempo real via comando.
Rede neural alternativa: implementar interface `forward` / `activations`; opcionalmente fornecer versão em lote.
Hooks adicionais: podem ser inseridos no loop do engine ou sistemas (ex.: logging de eventos) – manter leve para não degradar desempenho.

### Roadmap (curto/médio prazo)
1. Gráficos de métricas em tempo real (população, energia média).
2. Sistema de eventos (nascimento/morte/reprodução) para logging e replay.
3. Execução headless batch (CLI) para varrer combinações de parâmetros.
4. Obstáculos / diferentes tipos de alimento (nutrição variável).
5. Testes automatizados cobrindo sensores, reprodução, consumo energético, limites de população.
6. Aceleração opcional (Numba ou Cython) para hot loops (raycasts / batch forward).

### Problemas Conhecidos
- Balanceamento energético ainda empírico; valores extremos podem levar a explosão ou colapso rápido de população.
- Sem persistência incremental de longo prazo (apenas snapshots completos atuais).
- NN não possui normalização adaptativa de inputs; mudanças drásticas de sensores podem gerar saturação temporária.

### Contribuindo / Próximos Passos
- Abrir issues descrevendo experimentos desejados ou bottlenecks de performance.
- Adicionar testes (pytest) começando pelos utilitários determinísticos (raycast, energy cost, reprodução).
- Documentar resultados de benchmarks com diferentes configurações (usar scripts em `tests/`).

### Licença
Projeto educativo. Definir licença explícita (ex: MIT) conforme necessidade futura.

---
Resumo rápido: `python main.py` abre a UI PyQt6 com visualização Pygame embarcada; ajuste parâmetros, observe comportamentos emergentes, exporte substratos/agentes para reuso e explore mutação/energia em populações crescentes.




