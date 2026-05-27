@echo off
setlocal
cd /d "%~dp0"
if not exist results mkdir results
python python_pygame_learning.py --benchmark --agents 600 --foods 300 --steps 1200 --seed 123 --output results\python_pygame_learning.json
pause
endlocal
