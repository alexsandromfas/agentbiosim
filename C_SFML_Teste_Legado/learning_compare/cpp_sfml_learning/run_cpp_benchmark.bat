@echo off
setlocal
set ROOT=%~dp0
set BUILD=%ROOT%build
set SFML_ROOT=C:\Users\Alex Martins\Desktop\Meus Projetos\simulacoes_biologicas\AntSimulator-master\AntSimulator-master\third_party\SFML-2.6.2
set OUT=%ROOT%..\results\cpp_sfml_learning.json

if not exist "%ROOT%..\results" mkdir "%ROOT%..\results"
cmake -S "%ROOT%" -B "%BUILD%" -DSFML_ROOT="%SFML_ROOT%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 pause && exit /b 1
cmake --build "%BUILD%" --config Release
if errorlevel 1 pause && exit /b 1

"%BUILD%\Release\LearningCompareSFML.exe" --benchmark --agents 600 --foods 300 --steps 1200 --seed 123 --output "%OUT%"
pause
endlocal
