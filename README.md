# JsonSqlite

Modified version of sqlite library provides additional capabilities to work with json data

__Features:__

* New "->" operator to get values from json
* Efficient work with json data


## Example 1: Select value from key

```sql
SELECT '{"key": "val", "arr": ["v0", "v1"]}'->key;
> val
```

## Example 2: Sum values from json

Table Bill:

| id | bill                          | ... |
| --:|-------------------------------| --- |
| 1  | {"items": [], "total": 400}   | ... |
| 2  | {"items": [], "total": 1200}  | ... |
| 3  | {"items": [], "total": 100}   | ... |


```sql
SELECT SUM(bill->total) FROM Bill;
> 1700
```

## Unlicense

```
This is free and unencumbered software released into the public domain

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to http://unlicense.org
```