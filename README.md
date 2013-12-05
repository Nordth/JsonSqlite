JsonSqlite
==========

Modified version of sqlite library provides additional capabilities to work with json data

Retrieving json values with -> operator
=======================================

SELECT '{"key": "val", "arr": ["v0", "v1"]}'->key; // SAME AS ->'key'
> val
SELECT '{"key": "val", "arr": ["v0", "v1"]}'->arr->0;
> v0