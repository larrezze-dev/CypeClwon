#!/bin/bash
# da.sh "steps" : drive the already running original (attach variant)
powershell -NoProfile -ExecutionPolicy Bypass -File "D:\Coding\CypeClwon\tools\drive_attach.ps1" -StartupMs 0 -MaxSeconds ${2:-120} -Steps "$1" 2>&1 | grep -v "^$" | tail -${3:-4}
