#!/bin/bash
# usage: run_tests.sh ${unittests_absolute_path}

tests_path="$1"
script_path=$(pwd)
if [ "$tests_path" == "" ];then
  # default unittests path 
  tests_path="$script_path/../build/bin/tests/"
fi

echo "script path: $script_path"
echo "unittests path: $tests_path"

# 'tests' directory doesn't exists.
if [ ! -d "$tests_path" ];then
  echo "ERROR: unittests path doesn't exists."
  exit -1
fi

cd $tests_path
echo "current path: $(pwd)" 
echo "total count of tests: `ls test* | wc -l`"

# run all unittests
for test in `ls test*`
do  
  ./$test
  if [ ! $? -eq 0 ];then
    echo "ERROR: unittests not passed"
    exit -1
  fi
done 
echo "SUCCESS: all unittests passed"



