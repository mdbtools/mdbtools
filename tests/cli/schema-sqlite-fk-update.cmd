mdb-schema -T Orders test/data/nwind.mdb sqlite | sed -n '/^CREATE TABLE/,/^);/p'
