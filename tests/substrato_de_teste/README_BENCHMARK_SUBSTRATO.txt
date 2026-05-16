BENCHMARK DO SUBSTRATO DE TESTE

Arquivo principal:
- tests/benchmark_substrate_dynamics.py

Objetivo:
- Rodar a simulacao real em modo headless usando o substrato salvo em:
  tests/substrato_de_teste/substrato_de_teste_20260515_210719.json
- Gerar metricas comparaveis ao longo do tempo para saber se uma mudanca no
  codigo melhorou, piorou ou quebrou a dinamica da simulacao.

Por que existe:
- Mudancas internas podem nao aparecer visualmente na UI, mas podem alterar
  alimentacao, predacao, mortes, nascimentos, energia, movimento, sensores,
  saidas neurais e performance.
- Este benchmark cria um baseline historico para comparacoes futuras.

Como rodar:
  python tests/benchmark_substrate_dynamics.py --seconds 10 --label baseline

Argumentos uteis:
- --seconds N: duracao logica do teste em segundos reais de loop.
- --fps N: passos logicos por segundo. Padrao: 60.
- --sample-interval N: intervalo entre amostras agregadas. Padrao: 1s.
- --neural-sample N: quantidade de agentes por tipo usados para metricas neurais.
- --time-scale N: override opcional do time_scale salvo no substrato.
- --substrate PATH: usa outro snapshot JSON.

Saidas geradas:
- tests/substrato_de_teste/resultados_benchmark/<run_id>/summary.json
  Resumo completo da rodada, incluindo configuracao, contagens, eventos,
  integridade, metricas por tipo, performance e profiler.

- tests/substrato_de_teste/resultados_benchmark/<run_id>/timeseries.jsonl
  Serie temporal com uma linha JSON por amostra.

- tests/substrato_de_teste/resultados_benchmark/<run_id>/report.txt
  Relatorio humano curto para leitura rapida.

- tests/substrato_de_teste/resultados_benchmark/<run_id>/fator_comparativo.json
  Fatores principais daquela rodada.

- tests/substrato_de_teste/fatores_comparativos.jsonl
  Historico acumulado de fatores comparativos. Este e o arquivo mais importante
  para comparar mudancas daqui a semanas ou meses.

- tests/substrato_de_teste/layouts/*_food_layout_seed_*.json
  Layout canonico de comidas usado pelo benchmark quando o snapshot antigo
  contem apenas a quantidade de comida. Na primeira execucao ele e criado; nas
  seguintes, o mesmo arquivo e reutilizado para que o cenario seja identico.

Determinismo do benchmark:
- O runner fixa random.seed e np.random.seed.
- Para reduzir variacao entre rodadas, o benchmark tambem ordena os resultados
  do spatial hash dentro deste processo de teste. Isso evita que a ordem
  arbitraria de sets do Python escolha comidas/presas diferentes em empates.
- Essa ordenacao e local do benchmark; a UI normal nao usa essa instrumentacao.

Metricas coletadas:
- Populacao de bacterias, predadores, comidas e all_agents.
- Pressao contra limites maximos de populacao e target de comida.
- Energia, velocidade, idade e raio por tipo.
- Fracao perto da morte e pronta para reproducao.
- Comida consumida, predacoes, nascimentos e mortes.
- Ativacao de retina e saidas neurais amostradas.
- Estatisticas de arquiteturas/versoes de cerebros.
- Integridade: all_agents vs entities, NaN/infinito, fora do mundo, energia
  negativa e amostra de consistencia do spatial hash.
- Performance: tempo por step, ms/agente/step, secoes do profiler, CPU/RAM.

Observacao importante:
- O snapshot v2 atual salva apenas a quantidade de comida, nao as posicoes
  exatas das comidas. Portanto este runner restaura agentes exatamente e usa um
  layout canonico persistido para as comidas do benchmark. Isso nao recupera as
  posicoes originais perdidas pelo snapshot, mas garante que todas as rodadas
  futuras usem exatamente o mesmo cenario de comidas.
- Para recriar o layout canonico de comidas, use --rebuild-food-layout. Isso deve
  ser raro, porque muda a base de comparacao historica.
- Quando o export/import de substrato for corrigido para salvar comida completa,
  este runner ja tenta usar a lista exata se ela existir.

Uso normal da UI:
- Este benchmark nao roda automaticamente quando voce abre main.py.
- As metricas mais caras ficam isoladas neste modulo de teste.
