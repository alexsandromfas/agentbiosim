@echo off
setlocal
set ROOT=%~dp0
set BUILD=%ROOT%build
set SFML_ROOT=C:\Users\Alex Martins\Desktop\Meus Projetos\simulacoes_biologicas\AntSimulator-master\AntSimulator-master\third_party\SFML-2.6.2

cmake -S "%ROOT%" -B "%BUILD%" -DSFML_ROOT="%SFML_ROOT%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 pause && exit /b 1
cmake --build "%BUILD%" --config Release
if errorlevel 1 pause && exit /b 1

start "" "%BUILD%\Release\LearningCompareSFML.exe" --visual --agents 600 --foods 300 --seed 123
endlocal
