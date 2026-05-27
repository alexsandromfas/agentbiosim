# Learning Compare: Python/Pygame vs C++/SFML

Este teste e isolado do AgentBioSim principal. Ele compara duas implementacoes da mesma simulacao simples:

- `python_pygame_learning.py`
- `cpp_sfml_learning`

A dinamica e propositalmente pequena: organismos com MLP individual, visao por setores, comida, colisao/alimentacao, movimento e evolucao por selecao periodica.

## Benchmark Python

```bat
run_python_benchmark.bat
```

## Visual Python

```bat
run_python_visual.bat
```

## Benchmark C++/SFML

```bat
cpp_sfml_learning\run_cpp_benchmark.bat
```

## Visual C++/SFML

```bat
cpp_sfml_learning\run_cpp_visual.bat
```

Ou abra diretamente:

```bat
cpp_sfml_learning\build\Release\LearningCompareSFML.exe
```

No modo visual C++/SFML, o painel lateral tem rolagem com o scroll do mouse. Controles principais:

- aceleracao do tempo;
- zoom visual;
- tamanho do substrato;
- organismos de reset, minimo e maximo de organismos;
- target de comida e reposicao por segundo;
- energia/tamanho da comida;
- raio do organismo;
- raio de visao;
- retinas/bins;
- subdivisoes de distancia por bin;
- camadas da RN e neuronios por camada;
- velocidade, giro, custos energeticos;
- custo energetico em velocidade maxima;
- energia/cooldown de reproducao;
- taxa/forca de mutacao;
- colisao entre organismos;
- mostrar campo de visao;
- pausa.

Atalhos:

- `Espaco`: pausa/despausa;
- `V`: alterna visao;
- `F11`: alterna tela cheia;
- scroll sobre a simulacao: zoom;
- botao direito + arrastar: move a camera;
- `R`: reset;
- `ESC`: fecha.

Os resultados JSON ficam em `results/`.

Este teste nao importa nenhum modulo da simulacao principal.
