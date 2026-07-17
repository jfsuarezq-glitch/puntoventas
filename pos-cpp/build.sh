#!/usr/bin/env bash
# Compila POS Quiosco en Linux/macOS
set -e
g++ -std=c++17 -O2 -o PosQuiosco main.cpp
echo "[OK] Compilado: ./PosQuiosco"
