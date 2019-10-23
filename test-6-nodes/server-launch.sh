#!/bin/bash

if [[ -d ../../test ]]; then
    if [[ -d ../n1 ]]; then
	if [[ -f derecho.cfg ]]; then
	    num=`pwd | rev | cut -dn -f1`
	    taskset_num=$[num%6 ]
	    echo "Node" $num "using core" $taskset_num
	    if [[ -d ../../build ]]; then
		if [[ -d ../../build/CMakeFiles ]]; then
		    if [[ -f ../../build/src/sospdemo ]]; then
			echo "executing 'taskset --cpu-list $taskset_num ../../build/src/sospdemo server'"
			taskset --cpu-list $taskset_num ../../build/src/sospdemo server
		    else
			echo "Error: you do not appear to have built the demo yet!"
		    fi
		else
		    echo "error: build directory exists, but was not used for the cmake build."
		    exit 1
		fi
	    else
		echo "error: build directory does not exist.  please create it and use it for the cmake build"
		exit 1
	    fi
	    exit 0
	fi
    fi
fi
echo "Error: must run from within tutorial node folder (test/n0, test/n1, test/n2, test/n3)"

