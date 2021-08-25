#!/bin/bash

set -e

folders="app
    core
    tests
"
if [[ $# -eq 0 ]]; then
    echo "Missing action"
    exit 1
fi

action=$1

if [ "$action" == "check" ]; then
    clangformat_params="-dry-run --Werror"
elif [ "$action" == "fix" ]; then
    clangformat_params="-i"
else
    echo "Unkown action '$action'"
    exit 1
fi

for folder in $folders
do
    echo "Checking $folder"
    find $folder -regex '.*\.\(cpp\|h\)' -exec clang-format -style=file $clangformat_params {} \;
done
