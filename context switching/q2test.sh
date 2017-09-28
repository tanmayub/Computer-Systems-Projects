#!/bin/bash

# Bash script to test question 2 of homework 1
# Usage :- chmod +x q2test.sh
#          ./q2test.sh

#-------------------------------------------------------------------
# TEST CASE# 1 : To check if the lines contain the matching pattern 
#-------------------------------------------------------------------
echo "Test 1 : Basic Execution to check for matching pattern."
./homework q2 > actual_result.out <<EOF || { echo TEST q2.1 FAILED; exit; }
q2prog test
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -q "\-- we have a test tomorrow" actual_result.out ; then
    if grep -q "my name is Shailesh" actual_result.out ; then
        echo "ERROR: Incorrect Output"
        echo "Expected output: \-- we have a test tomorrow"
    else
        echo "SUCCESS ====> TEST q2.1 passed"
    fi  
fi

#------------------------------------------------------------------------
# TEST CASE# 2 : To check if the lines don't contain the matching pattern 
#------------------------------------------------------------------------
echo "Test 2 : Basic Execution to check if no matching pattern."
./homework q2 > actual_result.out <<EOF || { echo TEST q2.2 FAILED; exit; }
q2prog world 
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -q "we have a test tomorrow" actual_result.out || grep -q "my name is Shailesh" actual_result.out ; then
    echo "ERROR: Incorrect Output"
    echo "Expected output: > "
else
    echo "SUCCESS ====> TEST q2.2 passed"
fi

#-------------------------------------------------------------------
# TEST CASE# 3 : To check if the command line handles blank lines
#-------------------------------------------------------------------
echo "Test 3 : Check if the command line handles blank lines."
./homework q2 > actual_result.out <<EOF || { echo TEST q2.3 FAILED; exit; }

q2prog test
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -q "\-- we have a test tomorrow" actual_result.out ; then
    if grep -q "my name is Shailesh" actual_result.out ; then
        echo "ERROR: Incorrect Output"
        echo "Expected output: \-- we have a test tomorrow"
    else
        echo "SUCCESS ====> TEST q2.3 passed"
    fi  
fi

#-------------------------------------------------------------------
# TEST CASE# 4 : To check if we are actually looking for the file to
#                execute, rather than hardcoding it. i.e. we are 
#                copying q2prog to some other name and running the 
#                same test q2.1 but with the new program name.
#                Expected to work the same. 
#-------------------------------------------------------------------
echo "Test 4 : Check with changing the executable name."
# Changing the executable name from q2prog to patternMatch
cp q2prog patternMatch
./homework q2 > actual_result.out <<EOF || { echo TEST q2.4 FAILED; exit; }
patternMatch test
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -q "\-- we have a test tomorrow" actual_result.out ; then
    if grep -q "my name is Shailesh" actual_result.out ; then
        echo "ERROR: Incorrect Output"
        echo "Expected output: \-- we have a test tomorrow"
    else
        echo "SUCCESS ====> TEST q2.4 passed"
    fi  
fi
rm -f patternMatch
#-------------------------------------------------------------------------
# TEST CASE# 5 : To check for bad command names i.e. which are not present 
#-------------------------------------------------------------------------
echo "Test 5 : Check to handle bad command names."
./homework q2 > actual_result.out <<EOF || { echo TEST q2.5 FAILED; exit; }
bad-command-name test
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -q "ERROR: Unable to load the binary file bad-command-name" actual_result.out ; then
    echo "SUCCESS ====> TEST q2.5 passed"
else
    echo "ERROR: Incorrect Output"
    echo "Expected output: ERROR: Unable to load the binary file bad-command-name"
fi

#-------------------------------------------------------------------------
# TEST CASE# 6 : To test using Valgring i.e. for memory leaks in program. 
#-------------------------------------------------------------------------
echo "Test 6 : Valgrind test."
echo "Running valgring to test for memory errors....." 
valgrind ./homework q2 > actual_result.out <<EOF
q2prog test
my name is Shailesh
we have a test tomorrow

quit
EOF

if grep -v "ERROR SUMMARY: 0 errors from 0 contexts" actual_result.out ; then
    echo "Valgrind run successfull. No Memory Leaks"
    echo "SUCCESS ====> TEST q2.6 passed"
else
    echo "Valgrind gave errors..... Need to resolve."
fi
