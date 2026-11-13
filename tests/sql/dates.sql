select OrderDate from Orders where OrderID = 10248;
select count(*) from Orders where OrderDate > '1996-05-01';
select OrderID, OrderDate from Orders where OrderDate = '1996-05-02';
select OrderID, OrderDate from Orders where OrderDate = '1996-05-02 00:00:00';
select OrderID from Orders where OrderDate = strptime('1996-05-02','%Y-%m-%d');
select count(*) from Orders where OrderDate < strptime( '1994-11-18' , '%Y-%m-%d' );
select count(*) from Orders where (ShipVia = 1) AND NOT(EmployeeID = 4);
