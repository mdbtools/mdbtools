select count(*) from Shippers;
select count(*) from Shippers where CompanyName like '%Pack%';
select count(*) from Orders where ShipVia = 2 AND EmployeeID = 4;
select count(*) from Orders where ShipVia = 1 OR EmployeeID = 4;
select count(*) from Orders where ShipCountry = 'Germany';
