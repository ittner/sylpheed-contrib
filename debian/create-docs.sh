#!/bin/bash

DIR="build-tree/${1}"

for file in debian/*.docs.in; do
    cat ${file} | sed "s,-DIR-,${DIR},g" > ${file/.in/}
done
