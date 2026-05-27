# Relatorio comparativo: Python/Pygame vs C++/SFML

Teste isolado, sem importar o AgentBioSim principal.

## Configuracao comum

| Parametro | Valor |
|---|---:|
| Organismos | 600 |
| Comida | 300 |
| Setores de visao | 16 |
| Passos simulados | 1200 |
| Tempo simulado | 40 s |
| Seed | 123 |
| Rede neural | MLP individual, 16 entradas, 12 ocultos, 2 saidas |
| Aprendizado | Evolucao por selecao periodica + mutacao de pesos |

## Resultado principal

| Metrica | Python + Pygame | C++ + SFML |
|---|---:|---:|
| Wall time | 12.540 s | 3.773 s |
| Passos por segundo real | 95.69 | 318.06 |
| Velocidade relativa | 1.00x | 3.32x |
| Tempo simulado por segundo real | 3.19x | 10.60x |
| Comidas consumidas | 28,271 | 29,452 |
| Media de comidas por organismo | 47.12 | 49.09 |
| Geracoes | 4 | 4 |

## Perfil por secao

| Secao | Python + Pygame | C++ + SFML |
|---|---:|---:|
| Visao | 8.688 s / 69.28% | 2.088 s / 55.34% |
| Rede neural | 0.272 s / 2.17% | 0.242 s / 6.41% |
| Fisica/movimento | 0.099 s / 0.79% | 0.031 s / 0.83% |
| Alimentacao/colisao | 0.968 s / 7.72% | 0.160 s / 4.23% |
| Evolucao/mutacao | 0.016 s / 0.13% | 0.004 s / 0.11% |
| Renderizacao | 2.486 s / 19.82% | 1.246 s / 33.02% |

## Leitura tecnica

O C++/SFML foi 3.32x mais rapido nesse teste minimo. O ganho veio principalmente da visao e da interacao/alimentacao, que em Python ainda pagam muito custo de array temporario e operacoes de alto nivel.

A rede neural em Python ficou competitiva porque foi feita com NumPy vetorizado. Mesmo assim, no projeto real a rede convive com objetos, sensores, labels, UI e renderizacao, entao o ganho de C++ tende a aparecer mais quando o nucleo inteiro fica em arrays nativos.

A renderizacao SFML tambem foi mais rapida em tempo absoluto, mas no C++ ela vira uma fatia maior do total porque o resto da simulacao ficou muito mais rapido.

## Arquivos gerados

- Python: `results/python_pygame_learning.json`
- C++/SFML: `results/cpp_sfml_learning.json`

## Como abrir

Visual Python:

```bat
run_python_visual.bat
```

Visual C++/SFML:

```bat
cpp_sfml_learning\run_cpp_visual.bat
```

Benchmark Python:

```bat
run_python_benchmark.bat
```

Benchmark C++/SFML:

```bat
cpp_sfml_learning\run_cpp_benchmark.bat
```
