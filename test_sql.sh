#!/bin/bash

# Simple test script; run after performing
# git clone https://github.com/mdbtools/mdbtestdata.git test
./src/util/mdb-sql -i test/sql/nwind.sql test/data/nwind.mdb
