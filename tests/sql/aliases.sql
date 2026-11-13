select ShipperID as id, CompanyName name from Shippers;
select s.ShipperID [Shipper ID], s.CompanyName AS "Company" from Shippers s where s.ShipperID = 1;
select ShipperID, ShipperID as again, CompanyName, CompanyName x from Shippers;
select ShipperID, Shippers.* from Shippers where ShipperID = 3;
select OrderID, Freight, Freight AS f2 from Orders where OrderID < 10250;
select ShipperID as id from Shippers where id = 1;
