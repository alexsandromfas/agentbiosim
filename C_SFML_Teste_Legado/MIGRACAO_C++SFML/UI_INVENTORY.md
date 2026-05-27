# Inventario da UI Atual

Fonte principal: `sim/ui.py`, complementado por `README.md` e `sim/game.py`. A UI atual mistura partes modernas e possivelmente legadas. Nada deve ser removido na migracao sem verificacao manual no software rodando.

## Estrutura Geral

- Janela principal: `SimulationUI(QMainWindow)`.
- Renderizacao da simulacao: Pygame embutido/associado via `PygameView`.
- Painel lateral do agente selecionado com abas.
- Barra de ferramentas superior com controles de simulacao e ferramentas de canvas.
- Menus superiores.
- Abas de configuracao.
- Janelas modais de preferencias.
- Grafico de metricas.
- Tooltips/baloes explicativos por label clicavel.

## Terminologia de UI

- `Labels` e o nome encontrado no codigo atual; `Especies` e o nome conceitual recomendado para a migracao.
- `Agente` aparece no menu atual; `Genoma` e o destino conceitual para import/export de configuracao heredavel.
- Abas e menus antigos encontrados no codigo devem ser classificados como ativos ou legados por verificacao manual antes de qualquer remocao.

## Menus Superiores

### Arquivo

Acoes encontradas:

- `Novo`
- `Abrir Simulacao`
- `Salvar Simulacao`
- `Salvar Como`
- `Exportar substrato JSON`
- `Importar substrato JSON`

Fluxos associados:

- Criar novo estado de simulacao.
- Abrir `.biosim` ou JSON.
- Salvar no caminho atual.
- Salvar em novo caminho.
- Exportar/importar substrato em formato JSON legado.

### View

Acoes encontradas:

- `Mostrar ativacoes neurais`
- Submenu `Layout da rede neural`

Controles relacionados:

- Mostrar/ocultar detalhes do agente selecionado.
- Mostrar/ocultar grafico.
- Mostrar spatial hash.
- Mostrar visao do agente.
- Mostrar visao de multi-selecao.
- Layout neural `fixed`/preencher painel.

### Preferencias

Acoes/submenus encontrados:

- `Opcoes de simulacao`
- `Autosave`
- `Aparencia do ambiente`
- `Sistema de Visao`
- `Redes neurais`
- Submenu `Grafico`
- Submenu `Resolucao da renderizacao`
- Submenu `Novas mudancas`
- Acao `Percepcao, cerebro e escala`

Janelas associadas:

- Janela de opcoes de simulacao/fisica/performance geral.
- Janela de autosave.
- Janela de aparencia.
- Janela de sistema de visao.
- Janela de redes neurais.
- Janela de novas mudancas/performance.

### Agente

Acoes encontradas:

- `Exportar agente selecionado`
- `Carregar agente`
- `Criar linhagem a partir do selecionado`

Observacao:

- O conceito vem migrando para `Genoma`/`Especie`. Na versao C++ deve ser redesenhado, mas com compatibilidade funcional.

### Ajuda

Acoes encontradas:

- `Ajuda e atalhos`

## Abas Encontradas

As seguintes abas foram identificadas em `ui.py`:

- `Editor Genetico`
- `Populacao`
- `Substrato`
- `Labels`
- `Simulacao`
- `Bacterias`
- `Predadores`
- `Ajuda`
- `Teste`

Observacao importante:

- `Labels` aparece em mais de uma construcao no codigo.
- `Substrato` aparece em mais de uma construcao no codigo.
- `Bacterias`, `Predadores`, `Simulacao` e `Teste` podem representar UI legada ainda presente no arquivo.
- A migracao deve primeiro confirmar visualmente quais abas aparecem na aplicacao atual.

## Editor Genetico

Controles observados:

- Campo de nome do agente/template (`agent_template_name`).
- Botoes de importar/exportar genoma/agente, conforme estado atual da UI.
- Mini paineis por grupo de configuracao.
- Botoes `Aplicar a todos` e `Aplicar aos selecionados` em mini paineis.
- Cor do organismo.
- Corpo e movimento:
  - tamanho;
  - formato elipse/circulo;
  - velocidade maxima;
  - giro maximo;
  - permitir marcha re;
  - modo de movimento (`forward`, quatro direcoes etc.).
- Metabolismo/energia:
  - energia inicial;
  - energia de morte;
  - energia de split;
  - custo parado;
  - custo em vmax;
  - cap de energia;
  - morte por idade;
  - idade da morte;
  - virar comida ao morrer;
  - idade minima de reproducao;
  - cooldown.
- Visao:
  - raio de visao;
  - quantidade de retinas;
  - FOV;
  - quantidade de olhos;
  - angulo entre olhos;
  - separacao dos olhos;
  - ver comida;
  - ver organismos;
  - ver predadores;
  - ver obstaculos;
  - ver tudo colorido;
  - ver atraves paredes;
  - canais R/G/B/D.
- Dieta:
  - come comida;
  - come organismos;
  - pode comer mesma label;
  - eficiencia de comida;
  - eficiencia de agente.
- Rede neural:
  - numero de camadas ocultas;
  - neuronios por camada;
  - taxa de mutacao;
  - forca de mutacao;
  - structural jitter.

## Aba Populacao

Controles encontrados:

- Limites de bacteria e predador.
- Respeitar minimo das labels.
- Botao `Aplicar populacao`.

Observacao:

- Conceitualmente esta area deve migrar para `Especies`, mas qualquer funcionalidade atual precisa ser preservada.

## Aba Substrato

Controles observados:

- Tipo de comida (`instant`, `chunk/pedacos`).
- Target de comida.
- Raio minimo/maximo.
- Intervalo de reposicao.
- Tempo de mordida/consumo.
- Raio da particula de comida.
- Raio do aglomerado.
- Espacamento das particulas.
- Modo de reposicao (`spawn_cluster`, `grow_existing`, `grow_particles`).
- Trim de excesso e max trim por passo.
- Cor da comida.
- Formato do substrato (`rectangular`, `circular`).
- Largura/altura.
- Raio circular.
- Botao `Aplicar ambiente` ou `Aplicar Parametros`.
- Botao `Limpar Comida`.
- Botoes `Exportar substrato JSON` e `Importar substrato JSON`.

## Labels / Grupos

Controles observados:

- Mini painel por label.
- Nome da label.
- Quantidade atual.
- Minimo.
- Maximo.
- Inicial (em versoes recentes/conceito).
- Checkbox `Grafico`.
- Botao `Cor`.
- Botao `Selecionar`.
- Botao `Atribuir selecionados`.
- Botao `Remover selecionados`.
- Botao `Resetar rede neural`.
- Botao `Excluir`.
- Botao `+ Nova label com selecionados`.
- Scroll vertical solicitado/implementado.

Observacao:

- Futuro nome conceitual: `Especies`.
- Cada especie deve poder carregar genoma associado.

## Aba Simulacao / Experimento Legada

Controles encontrados em codigo:

- Escala de tempo.
- Pausado.
- Resgate populacao minima.
- Auto export/autosave.
- Mostrar detalhes agente selecionado.
- Mostrar ativacoes neurais.
- Tracebacks no debug.
- Botoes de iniciar/resetar/aplicar.

Observacao:

- Parte disso ja foi movida para toolbar/preferencias, mas o codigo ainda contem a aba. Verificar manualmente antes de remover.

## Abas Bacterias e Predadores Legadas

Controles encontrados:

- Populacao inicial/min/max.
- Cores.
- Energia/metabolismo.
- Visao.
- Rede neural.
- Botoes `Aplicar a novos individuos`, `Aplicar a todos vivos`, `Aplicar ao selecionado`.

Observacao:

- Podem estar obsoletas em relacao ao editor genetico moderno. A versao C++ deve preservar funcionalidade, nao necessariamente essas abas.

## Painel do Agente Selecionado

Estrutura atual:

- Painel retratil.
- Abas `Genoma` e `Rede Neural`.
- Aba Genoma:
  - informacoes de corpo;
  - movimento;
  - energia;
  - dieta;
  - visao;
  - rede;
  - inteligencia local.
- Aba Rede Neural:
  - visualizador neural grafico;
  - retinas/canais;
  - camadas;
  - saidas;
  - pesos/ativacoes.

Requisitos de migracao:

- Aparecer apenas quando houver agente selecionado.
- Expandir largura no modo rede neural.
- Nao calcular informacoes caras quando oculto.

## Visualizador Neural

Controles/estado:

- Layout `fixed` ou preencher painel.
- Mostrar valores por padrao.
- Pesos fracos ocultos por padrao, conforme configuracao atual.
- Entrada visual derivada do genoma/canais.
- RNN mostrada de forma simplificada.
- NEAT deve ter representacao apropriada quando usado.

## Grafico

Controles encontrados/esperados:

- Checkbox por metrica.
- Valor atual junto da legenda.
- Janela de tempo: ultima hora, 5h, 10h, 24h, tudo.
- Taxa de amostragem: 1s, 5s, 30s, 1min, 10min.
- Altura redimensionavel.
- Pausa junto da simulacao.
- Coleta desde inicio da simulacao conforme taxa.
- Mostrar populacao total, comida, predadores, inteligencia de grupo e grupos/labels.

## Ferramentas de Canvas

Botoes/ferramentas encontrados ou solicitados no historico do projeto:

- `S`: seletor unitario.
- Lasso selection.
- Square selection.
- `F`: inserir comida.
- `A`: inserir agente/genoma carregado.
- Pincel de obstaculo.
- Apagar obstaculo.
- `M`: mover.
- `D`: deletar/matar item clicado.
- Pipeta/coletar genoma.
- Pan fixo no botao direito.
- Tooltips/legendas ao passar mouse.

## Atalhos de Teclado

Atalhos encontrados:

- `Delete`: apagar agentes selecionados.
- `Space`: pausar/retomar.
- `Esc`: limpar selecao.

Atalhos do README/game:

- `R`: reset populacao.
- `F`: enquadrar mundo.
- `T`: alternar renderer simples/detalhado.
- `V`: alternar ativacoes/visao.
- `WASD`/setas: mover camera.
- Scroll: zoom.

## Janelas de Preferencias

### Opcoes de Simulacao

- FPS.
- Hz fisico.
- Max substeps.
- Backlog maximo.
- Renderizar.
- Spatial hash.
- Reutilizar grid espacial.
- Locomocao suave.
- Inercia/arrasto linear/angular.
- Interpolar render.
- Colisao organismo-organismo.
- Elasticidade.
- Transferencia de velocidade.
- Viscosidade.
- Comida em pedacos movel.
- Colisao comida-comida.
- Adesina da comida.
- Ruido browniano.

### Sistema de Visao

- Modo de visao global.
- Mostrar visao de multi-selecao.
- Modo dos bins.
- Subdivisoes de distancia.
- Distribuicao de distancia.
- Falloff de distancia.
- Projecao.
- Candidate limit.
- Obstaculos bloqueiam visao.
- Auto sector em alta escala.
- Setor global experimental.

### Redes Neurais

- Tipo de rede.
- Painel especifico por tipo:
  - MLP;
  - gated MLP;
  - shortcut MLP;
  - modulated MLP;
  - simple RNN;
  - NEAT comum;
  - NEAT simplificada;
  - NEAT recorrente.
- Parametros de gates, atalhos, recorrencia, topologia, mutacoes e limites.

### Autosave

- Ativar autosave.
- Intervalo.
- Salvar ativacoes neurais.
- JSON legivel manual.
- Tracebacks/debug.

### Aparencia do Ambiente

- Fundo solido ou gradiente.
- Cores topo/baixo do fundo.
- Substrato solido ou gradiente.
- Cores topo/baixo do substrato.
- Borda ligada/desligada.
- Cor da borda.

### Resolucao de Renderizacao

- Escala de renderizacao 1x a 3x.

### Novas Mudancas / Performance

- Buffers persistentes da cena.
- Visao por grupos.
- Retina Numba em lote.
- Auto visao setorial em escala.
- Setor global experimental.
- Cache de pesos por grupo.
- Forward do cerebro em Numba.
- Numba locomocao/energia.

## Fluxos Criticos de UI que Devem Ser Preservados

- Abrir/salvar `.biosim`.
- Salvar como.
- Novo arquivo mantendo preferencias de UI quando apropriado.
- Exportar/importar substrato.
- Exportar/importar agente/genoma.
- Autosave sem travar o loop.
- Aplicar parametros por mini painel.
- Aplicar aos selecionados/a todos conforme comportamento atual.
- Selecionar por canvas e labels.
- Visualizar agente e rede.
- Alterar renderizacao sem reiniciar simulacao.

## Itens que Precisam Verificacao Manual

- Quais abas legadas realmente aparecem.
- Estado final do menu `Agente` versus futuro `Genoma`.
- Estado final de pipeta na toolbar ou editor.
- Nomes atuais finais de `Labels`/`Especies`.
- Se botao `Teste` ainda aparece.
- Se `Populacao` ainda e funcional ou apenas legado.
