#!/usr/bin/env bash

for dir in examples/*; do
    for test in "$dir"/*.iira; do
        if [[ "$test" == *"error"* ]]; then
            if timeout 1 build/linux/x86_64/debug/iirac "$test" &> /dev/null ; then
                echo -e "[\e[1;31mFAIL\e[0m]: $test"
            else
                echo -e "[\e[1;32mPASS\e[0m]: $test"
            fi
        else
            if timeout 1 build/linux/x86_64/debug/iirac "$test" &> /dev/null ; then
                echo -e "[\e[1;32mPASS\e[0m]: $test"
            else
                echo -e "[\e[1;31mFAIL\e[0m]: $test"
            fi
        fi
    done;
done
