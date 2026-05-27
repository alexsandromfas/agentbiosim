# AgentBioSimCpp

Esqueleto minimo da migracao C++/SFML/CMake do AgentBioSim.

Este projeto ainda nao implementa simulacao, agentes, comida, visao, redes neurais ou UI completa. A Fase 1 validou a janela SFML vazia. A Fase 2 adicionou um `ParameterRegistry` inicial para catalogar parametros herdados do projeto Python antes da simulacao existir em C++.

## Requisitos

- Windows
- CMake 3.20+
- Compilador C++17, preferencialmente Visual Studio 2022/MSVC
- SFML 2.6.2

Por padrao, o `CMakeLists.txt` tenta encontrar a SFML em:

```text
C:/Users/Alex Martins/Desktop/Meus Projetos/simulacoes_biologicas/AntSimulator-master/AntSimulator-master/third_party/SFML-2.6.2
```

Se a SFML estiver em outro lugar, use `-DSFML_ROOT=...`.

## Build Debug

```powershell
cmake -S C_SFML_Teste_Legado/AgentBioSimCpp -B C_SFML_Teste_Legado/AgentBioSimCpp/build -DCMAKE_BUILD_TYPE=Debug
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Debug
```

## Build Release

```powershell
cmake -S C_SFML_Teste_Legado/AgentBioSimCpp -B C_SFML_Teste_Legado/AgentBioSimCpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Release
```

## Executar

Com geradores multi-config, como Visual Studio:

```powershell
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe
```

ou:

```powershell
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe
```

Com geradores single-config, o executavel pode ficar diretamente em:

```powershell
C_SFML_Teste_Legado/AgentBioSimCpp/build/AgentBioSimCpp.exe
```

## Dump de parametros

O `ParameterRegistry` registra nome interno, tipo, valor padrao, categoria, descricao curta, aliases legados, faixas numericas e dominios como `runtime`, `ui`, `debug`, `world`, `vision`, `neural`, `food`, `physics`, `render`, `save` e `performance`.

Para verificar o cadastro inicial sem abrir a janela SFML:

```powershell
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --dump-params
```

ou, em Debug:

```powershell
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --dump-params
```

## Estado atual

- Abre janela SFML vazia.
- Permite fechar a janela.
- Mostra FPS no titulo da janela.
- Contem `ParameterRegistry` inicial.
- Permite dump de parametros via `--dump-params`.
- Nao contem simulacao.
- Nao contem entidades.
- Nao contem sistemas.
- Nao contem UI tecnica completa.
