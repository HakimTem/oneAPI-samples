#!/bin/bash
source /opt/intel/oneapi/setvars.sh > /dev/null 2>&1
icpx -fsycl lab/buffer_mem_move_0.cpp 
if [ $? -eq 0 ]; then ./a.out; fi
