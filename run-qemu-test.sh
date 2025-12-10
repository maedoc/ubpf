#!/bin/bash

cd examples/esp32_ubpf_demo
rm build/qemu_*

# idf.py build
# timeout 60 python3 runner.py

timeout 20 idf.py build qemu monitor | tee -a log.txt

