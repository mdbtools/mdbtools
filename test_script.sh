#!/bin/bash

# Simple test script; run after performing
# git clone https://github.com/mdbtools/mdbtestdata.git test
./src/util/mdb-json test/data/ASampleDatabase.accdb "Asset Items"
./src/util/mdb-json test/data/nwind.mdb "Umsätze"
./src/util/mdb-count test/data/ASampleDatabase.accdb "Asset Items"
./src/util/mdb-count test/data/nwind.mdb "Umsätze"
./src/util/mdb-prop test/data/ASampleDatabase.accdb "Asset Items"
./src/util/mdb-prop test/data/nwind.mdb "Umsätze"
./src/util/mdb-schema test/data/ASampleDatabase.accdb
./src/util/mdb-schema test/data/nwind.mdb
./src/util/mdb-schema test/data/nwind.mdb -T "Umsätze" postgres
./src/util/mdb-tables test/data/ASampleDatabase.accdb
./src/util/mdb-tables test/data/nwind.mdb
./src/util/mdb-ver test/data/ASampleDatabase.accdb
./src/util/mdb-ver test/data/nwind.mdb
./src/util/mdb-queries test/data/ASampleDatabase.accdb qryCostsSummedByOwner
