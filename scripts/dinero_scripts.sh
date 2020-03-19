#!/bin/sh

#Script to run valgrind - perl_script - dinero

benchmark='/home/janjust/valgrind_test/tests/mibench'

dinero_param1='-l1-dsize 32768 -l1-dbsize 32 -l1-dassoc 4 -l1-drepl r -informat d'
dinero_param2='-l1-dsize 32768 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'

dinero_param3='-l1-dsize 16384 -l1-dbsize 32 -l1-dassoc 4 -l1-drepl r -informat d'
dinero_param4='-l1-dsize 16384 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'

dinero_param5='-l1-dsize 8192 -l1-dbsize 32 -l1-dassoc 4 -l1-drepl r -informat d'
dinero_param6='-l1-dsize 8192 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'

dinero_param_test='-l1-dsize 4096 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'
dinero_param_test1='-l1-dsize 2048 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'
dinero_param_test2='-l1-dsize 1024 -l1-dbsize 32 -l1-dassoc 1 -l1-drepl r -informat d'



p_dinero='/home/janjust/janjust_svn/DineroIV_modified'
script_path='/home/janjust/research/svn_janjust'

echo Start of Script
    
    $p_dinero/dineroIV $dinero_param2 < ~/research/test_traces/dijkstra/dijkstra_large.valgrind.out > dijkstra_large.dinero.out

echo End of Script 
