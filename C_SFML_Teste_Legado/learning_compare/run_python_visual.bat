@echo off
setlocal
cd /d "%~dp0"
python python_pygame_learning.py --visual --agents 600 --foods 300 --seed 123
if errorlevel 1 pause
endlocal
