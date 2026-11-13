mdb-schema -T "Order Details" test/data/nwind.mdb sqlite | sed -n '/^CREATE TABLE/,/^);/p'
