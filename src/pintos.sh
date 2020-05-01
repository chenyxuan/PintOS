#!/bin/sh
subdir=`pwd|sed 's/.*\/\(.*\)\/build$/\1/g'`
testcase=$2

command -v pintos >/dev/null 2>&1 || {
    echo "Error: pintos not found, check your PATH"
    exit 1
}

(command -v bochs >/dev/null 2>&1) || (command -v qemu-system-i386 >/dev/null 2>&1) || {
    echo "Error: no simulator found"
    exit 1
}

case $1 in
    'run')
    echo "Running testcase $testcase"
    rm -f "tests/$subdir/$testcase.result"
    rm -f "tests/$subdir/$testcase.output"
    make "tests/$subdir/$testcase.result" VERBOSE=1
    ;;

    'debug')
    echo "Debugging testcase $testcase, awaiting gdb"
    rm -f "tests/$subdir/$testcase.result"
    rm -f "tests/$subdir/$testcase.output"
    make "tests/$subdir/$testcase.result" PINTOSOPTS='--gdb' VERBOSE=1
    ;;

    'runall')
    make clean
    make check
    ;;

    *)
    echo "Usage: ./pintos.sh [ run | debug | runall ] [ testcase ]"
esac
