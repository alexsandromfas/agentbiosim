# Fase 2 - Status do ParameterRegistry

## Objetivo

Criar o cadastro inicial de parametros da futura versao C++ sem implementar simulacao, engine, entidades, comida, visao, redes neurais ou UI completa.

## Arquivos C++ Criados

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/config/Parameter.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/config/ParameterRegistry.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/config/ParameterRegistry.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/config/ParameterDefaults.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/config/ParameterDefaults.cpp`

## Arquivos C++ Alterados

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/main.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/README.md`

## Parametros Implementados

Cadastro inicial criado com 283 parametros registrados no dump de validacao. Categorias principais cobertas:

- tempo e simulacao: escala de tempo, FPS, pausa, passos fisicos, backlog, seed e limites basicos de lifecycle;
- mundo e substrato: formato, largura, altura, raio, cores e borda;
- render/performance: renderizacao, resolucao, render simples, spatial hash, toggles legados de Numba/native, batch de visao e cache neural;
- visao global: modo de visao, skip de retina, modo por bins/setores, subdivisoes de distancia, projection, falloff e limite de candidatos;
- comida: modo instant/chunk, target, raio, intervalo de reposicao, parametros de particulas/pedacos e trim de excesso;
- organismos base legados: parametros de `bacteria` e `predator` para populacao, energia, metabolismo, corpo, movimento, visao, dieta, rede neural e cor;
- fisica: locomocao suave, inercia, arrasto, colisao elastica, transferencia de velocidade, viscosidade, comida movel, colisao de comida, adesao de chunk e ruido browniano;
- UI/debug/save/aparencia: detalhes do selecionado, grafico, autosave, export, tracebacks, diagnostico, cores de ambiente e alimento;
- redes neurais globais: MLP, MLP com gates, atalhos, RNN simples, NEAT comum, proto-NEAT e NEAT recorrente.

## Aliases Implementados

Aliases legados cadastrados para preservar compatibilidade conceitual com nomes historicos:

- `species_template_name` -> `agent_template_name`
- `use_native_kernels` -> `use_numba_kernels`
- `use_native_batch_retina` -> `use_numba_batch_retina`
- `use_native_locomotion_energy` -> `use_numba_locomotion_energy`
- `use_native_brain_forward` -> `use_numba_brain_forward`
- `food_type` -> `food_mode`
- `predator_enabled` -> `predators_enabled`
- `autosave_enabled` -> `auto_export_substrate`
- `autosave_interval_minutes` -> `auto_export_interval_minutes`
- `substrate_background_color` -> `substrate_bg_color`
- aliases dinamicos de especies para `*_retina_see_agents` e `*_diet_same_species`.

## Decisoes Tomadas

- `ParameterRegistry` nao depende de SFML.
- Cor RGB usa `ColorRgb` proprio, com inteiros `r`, `g`, `b`.
- Valores usam `std::variant<bool, int, double, std::string, ColorRgb>`.
- Faixas numericas usam `NumericRange` com `std::optional<double>` para min/max.
- O dump de parametros fica disponivel por `--dump-params` para validar a Fase 2 sem abrir janela.
- NEAT entrou apenas como cadastro de parametros. Nenhuma rede neural foi implementada.
- Parametros Python/Numba foram preservados como categoria `performance.python_compat`, porque sao importantes para rastreio historico, mas nao definem implementacao C++.

## Parametros Pendentes

- Parametros dinamicos criados em runtime por especies/labels futuras ainda precisam ser definidos na fase de SpeciesRegistry.
- Parametros especificos de save/load `.biosim` ainda precisam de esquema formal.
- Valores padrao devem ser refinados durante fases de paridade com Python.
- Nem todos os controles de UI possuem mapeamento definitivo para widgets C++/ImGui.
- Alguns nomes podem precisar ajuste depois de comparar diretamente com saves reais.

## Limitacoes Atuais

- Nao existe leitura/escrita de arquivos de configuracao.
- Nao existe persistencia de preferencias.
- Nao existe UI para editar parametros.
- Nao existe validacao runtime alem de cadastro e dump.
- Nao existe simulacao, engine, entidade, mundo, comida, visao, rede neural ou metricas.

## Confirmacao de Escopo

- Esta fase implementa somente o sistema inicial de cadastro de parametros.
- A simulacao continua inexistente na versao C++.
- Nenhum arquivo Python deve ser alterado por esta fase.
- A Fase 3 nao foi iniciada.
