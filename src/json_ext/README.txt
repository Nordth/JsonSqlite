
This directory contains source code for the SQLite "Json" extension, which
provide extension function json_get

mixed json_get(text json, path_element1, path_element2 ...)

Parameters
 json - json source text
 path_element - one or more keys and indexes to retrieving value.
                If path_element is integer it is interpreted as array index
                If path_element is string it is interpreted as object key

Return:
 If retrieved value is array or json object, return json text
 If retrieved value is null, return NULL
 If retrieved value is false, return 0
 If retrieved value is true, return 1
 If retrieved value is string, return string
 If retrieved value is number, return integer or double

Example:

SELECT json_get('{"key": "val", "arr": ["v0", "v1"]}', 'key');
> val

SELECT json_get('{"key": "val", "arr": ["v0", "v1"]}', 'arr', 0);
> v0

This extension uses JsonGet library to parse JSON