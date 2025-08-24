<<<<<<< HEAD
=======
## Simulação de Predadores e Bactérias

Breve descrição
- Projeto em Python que simula micro-organismos (bactérias) e predadores num substrato 2D. Cada agente possui sensores (retina), um cérebro (rede neural feedforward simples), atuadores (locomoção, modelo energético) e interage com comida e outros agentes.

Visão geral
- Arquitetura separa motor (headless) e interfaces (Tkinter + Pygame). O motor (`sim/engine.py`) contém a lógica da simulação, sistemas e atualização de entidades; a UI orquestra parâmetros e visualização.

Requisitos mínimos
- Python 3.10+ (recomendado)
- pygame (instalar via pip)

Como rodar
- No diretório do projeto:

```powershell
python main.py
```

Controles principais (Pygame)
- Clique esquerdo: selecionar agente / adicionar comida (se não houver agente)
- Clique do meio: adicionar bactéria
- Clique direito + arrastar: pan da câmera
- Scroll do mouse: zoom (com foco onde o cursor aponta)
- Espaço: pausar / despausar
- R: resetar população
- F: ajustar câmera para o mundo
- T: alternar modo de render
- V: alternar exibição da visão
- WASD / setas: mover a câmera

Arquivos principais e responsabilidades
- `main.py` — bootstrap: cria `Params`, `World`, `Engine`, `PygameView` e `SimulationUI` e conecta callbacks.
- `sim/engine.py` — motor headless: loop de simulação, stepping, processamento de comandos e execução de sistemas.
- `sim/world.py` — mundo (retangular/circular) e `Camera` para conversões coordenadas.
- `sim/game.py` — `PygameView`: inicializa Pygame, input e loop de render.
- `sim/ui_tk.py` — interface Tkinter para controle de parâmetros e start/stop.
- `sim/entities.py` — `Entity`, `Agent`, `Bacteria`, `Predator` e fábricas.
- `sim/sensors.py` — `RetinaSensor`, `SceneQuery` e utilitários de raycast.
- `sim/actuators.py` — `Locomotion`, `EnergyModel` (consumo, divisão).
- `sim/brain.py` — `NeuralNet` feedforward com mutação e resize de entrada.
- `sim/systems.py` — regras: interação, reprodução, morte, colisões.
- `sim/spatial.py` — `SpatialHash` para acelerar queries espaciais.
- `sim/render.py` — `RendererStrategy`, `SimpleRenderer`, `EllipseRenderer`.
- `sim/controllers.py` — `Params`, `FoodController`, `PopulationController`.

Como o motor funciona (contrato resumido)
- Inputs: `Params`, `World`, comandos via `Engine.send_command()` (ex.: `add_food`, `add_bacteria`, `reset_population`).
- Outputs: estado mutável `Engine.entities` (listas de `bacteria`, `predators`, `foods`) e render via `engine.render(surface)`.
- Erros/limites: validações em `Params`; sistemas aplicam limites (população, morte) para estabilidade.

Pontos de extensão para desenvolvedores
- Substituir renderer: criar classe que implemente `RendererStrategy` em `sim/render.py`.
- Trocar o cérebro: manter a interface `forward(inputs)` e `activations(inputs)` ao substituir `NeuralNet` por outra implementação.
- Adicionar sensores: novos sensores devem usar `SceneQuery` para obter dados espaciais.
- Salvar/carregar: serializar `Engine.entities`, `Params`, `Camera` e pesos das redes permite restaurar simulacões.

Notas rápidas de desenvolvimento
- Código usa sintaxe moderna (Python 3.10+).  
- `pygame` é requerido para a view; `tkinter` é parte da stdlib na maioria das instalações do Windows.  
- Recomenda-se adicionar testes unitários antes de refatorações grandes.

Known issues
- Pode haver casos em que predadores/bactérias cresçam de forma inesperada quando reprodução e limites populacionais interagem — revisar `set_mass`, `EnergyModel.prepare_reproduction` e `DeathSystem`.

Melhorias futuras

1) Principais pontos:

- Migrar a interface para customtkinter para uma UI mais moderna.
- Corrigir o bug do tamanho do predador (crescimento excessivo que ignora limites de divisão).
- Implementar save/restore completo (posições, parâmetros, redes neurais, câmera) para retomar simulações.
- Tornar possível exportar/importar o "genoma" de um agente (arquivo por agente) e reinserir na simulação.
- Permitir que agentes possam andar para trás se a rede neural assim decidir.
- Acrescentar menu superior com exportação de genoma selecionado, aba de ajuda e importação de substratos predefinidos.
- Adicionar ferramenta para pintar obstáculos com o mouse no substrato.
- Avaliar alternativa: remover variação de tamanho e introduzir energia interna (bateria) fixa por indivíduo; divisões ocorrem por acúmulo de energia.
- Introduzir alimentos com diferentes valores nutricionais (comida mais nutritiva altera comportamento de competição).
- Dar opção no UI para que predadores exijam múltiplos impactos para matar uma presa (ex.: 3 impactos configuráveis).
- Permitir comportamentos agressivos adicionais (bactérias atacando bactérias; predadores atacando outros predadores).
- Alteração genética de cores para possibilitar diferenciação visual e formação de tribos.

2) Melhorias recomendadas pelo Copilot e já aprovadas:

- Checkpoints periódicos (persistência incremental) para reduzir perda de progresso em simulações longas.
- Exportar métricas históricas (população, energia média, taxas) e opção de salvar CSV para análise externa.
- Ferramenta de perfilamento e logging para identificar gargalos de performance.
- Suporte a múltiplos cenários/substratos carregáveis via UI para facilitar experimentos.
- Modo replay (reproduzir simulação a partir de logs) — recurso opcional e de alto custo computacional; ativar apenas quando necessário.
- Snapshot por agente (exportar genomas e estatísticas) e catálogo de genomas navegável na UI.
- Avaliar aceleração de trechos pesados com Numba / paralelização ou offload para GPU (raycasts / sensores).
- Gráficos em tempo real na UI (população, média de massa, energia média) para monitoramento.
- Implementar integrador/colisão opcional mais estável (ex.: semi-implícito ou Verlet) para maior robustez física.
- Modo observador que fixa a câmera em um agente e grava vídeo/frames para análise detalhada.

Observação: os itens acima foram adicionados sem remover as melhorias originais que você indicou.

Sugestões do Copilot, mas que ainda devem ser estudadas:

- Testes automatizados (item 3 da lista original sugerida)
  - Servem para validar comportamento de componentes críticos (p.ex. `NeuralNet`, `SpatialHash`, sistemas) e prevenir regressões. Recomenda-se usar `pytest` e cobrir caminhos principais mais casos de borda.

- Modo headless / CLI (item 4)
  - Execução da simulação sem UI (apenas `Engine`) para rodar experimentos em lote, varreduras de parâmetros e coleta de métricas em servidores/VMs.

- Throttling / adaptive timestep (item 8)
  - Ajusta dinamicamente o timestep ou o número de substeps quando a simulação fica sobrecarregada, preservando estabilidade numérica. Pode afetar determinismo; usar com cuidado em experimentos controlados.

- Paralelização de partes do Engine (item 9)
  - Permite executar tarefas independentes (sensoriamento de agentes, raycasts) em workers/threads. Em Python, avaliar multiprocessing, Numba ou offload para GPU (GIL limita threads em CPU-bound). Requer reorganização para minimizar estados compartilhados e pontos de sincronização.

- Sistema de eventos/hooks (item 18)
  - Observers (hooks) que emitem eventos (nascimento, morte, reprodução, checkpoint) para logging, UI, gravação de replays ou integração externa. Deve ser desativável para não penalizar desempenho.

>>>>>>> 341a7a0 (Initial commit)


hello world

