@echo off
call python src/gen.py
call third_party\ninja\ninja-win.exe -C out\w%1 %2 %3 %4 %5
