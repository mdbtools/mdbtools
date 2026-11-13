select count(*) from Orders where ShippedDate IS NULL;
select count(*) from Orders where ShippedDate IS NOT NULL;
select OrderID, ShipVia from Orders where ShippedDate IS NULL AND ShipVia = 3;
select count(*) from Orders where ShipRegion = 'RJ';
select count(*) from Orders where NOT ShipRegion = 'RJ';
select count(*) from Orders where ShipRegion IS NULL;
select count(*) from Products where Discontinued IS NULL;
select count(*) from Products where Discontinued = 1;
