@echo off
chcp 65001 >nul
set PYTHONIOENCODING=utf-8
set PY=C:\Users\Lenovo\AppData\Local\Programs\Python\Python311\python.exe
if not exist "%PY%" set PY=python
cd /d "%~dp0..\.."
"%PY%" -u plans\tasks\ektx010-set-address.py --addr 2 %*
echo.
pause
