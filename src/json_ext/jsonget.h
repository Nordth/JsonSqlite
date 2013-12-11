/*
** Copyright (c) 2013 Elantcev Mikhail
** 
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** ----------------------------------------------------------------------------
** JsonGet -
**   small ansi C library to retrieve values from json 
** ----------------------------------------------------------------------------
** 
** Features:
** =========
** 1) No dynamic memory allocations!
**    All functions work only with original json buffer
** 2) On-demand single pass parsing!
**    Parse only requested branch
** 3) No dependencies!
**    Even from standart C libs
**
** Library doesn't try to construct full syntax tree of json. It parses only requested branch, so it
** can work with partly-corrupted json data
**
** Usage examples:
** ===============
** 1. Basic usage
**    JsonGetCursor root, k, a0, wrong;
**    char buffer[255];
**    int real_len, val;
**
**    root = jsonget("{\"k\": \"v\", \"a\": [10, 20]}")
**
**    k = jsonget_move_key(root, "k");
**    jsonget_string(k, &buffer, sizeof(buffer), &real_length); 
**    // -> return 1, buffer = "v", real_length = 1
**
**    a0 = jsonget_move_index(jsonget_move_key(root, "a"), 0);
**    jsonget_int(k, &val); 
**    // -> return 1, val = 10
**
**    wrong = jsonget_move_key(root, "wrong");
**    jsonget_int(wrong, &val)
**    // -> return 0
**    if (wrong.type != JSON_INVALID) ... // check if property exists
**
** 
** 2. Iterate over json object fields or array elements
**    char key[255];
**    JsonGetCursor cur = jsonget_move_index(root, 0);
**    while (cur.type != JSONGET_INVALID)
**    {
**      jsonget_string(cur, key, sizeof(key), &real_len); 
**      jsonget_string(jsonget_move_pair_value(cur), buffer, sizeof(buffer), &real_len); 
**      printf("%s: %s\n", key, buffer);
**      cur = jsonget_move_next(cur);
**    }
**    // Prints:
**    // k: v
**    // a: [10, 20]
**
*/

#ifndef JSONGET_H
#define JSONGET_H

#ifdef __cplusplus
extern "C"
{
#endif
		
// Json types
#define JSONGET_INVALID 0 // Json parse error or value is not exists
#define JSONGET_NULL    1 // null
#define JSONGET_BOOLEAN 2 // true | false
#define JSONGET_INTEGER 3 // 123
#define JSONGET_DOUBLE  4 // 1.23
#define JSONGET_STRING  5 // "string"
#define JSONGET_OBJECT  6 // {"k1": "v1", "k2": 2}
#define JSONGET_ARRAY   7 // [0, 1, 2]
#define JSONGET_PAIR    8 // "key": "value"

// Cursor is a pointer to some value in json
//
typedef struct 
{
	const char* pstr;		// pointer to first value character
	int type;		// type of json value
} JsonGetCursor;

/*
** ------------------------------------------
** Init cursor
** ------------------------------------------
*/

// Create json cursor from NULL-terminated json_str buffer
extern JsonGetCursor jsonget(const char *json_str);

/*
** ------------------------------------------
** Move cursor
** ------------------------------------------
*/

// Move to _key_ field of current json object
extern JsonGetCursor jsonget_move_key(const JsonGetCursor cursor, const char *key);

// Move to _index_ index of current json array or _index_ pair in json object
extern JsonGetCursor jsonget_move_index(const JsonGetCursor cursor, const int index);

// Move to next element in array
// If cursor is pair in json object, move to next pair
extern JsonGetCursor jsonget_move_next(const JsonGetCursor cursor);

// Move to pair value 
extern JsonGetCursor jsonget_move_pair_value(const JsonGetCursor cursor);

/*
** ------------------------------------------
** Read values from cursor
** ------------------------------------------
*/

// Function to get integer value from cursor
//
// Type    | Value
// --------------------------------
// NULL    | 0
// BOOLEAN | 1 - true, 0 - false
// INTEGER | actual integer val
// DOUBLE  | floor(double val)
//
// If cursor type is one of listed above, return 1 overwise return 0
//
extern int jsonget_int(const JsonGetCursor cursor, int *out_int);

// Function to get float-point value from cursor
//
// Type    | Value
// --------------------------------
// INTEGER | integer val converted to double
// DOUBLE  | actual double val
//
// If cursor type is one of listed above, return 1 overwise return 0
//
extern int jsonget_double(const JsonGetCursor cursor, double *out_double);

// Function to get raw representation of cursor value
// E.g. [1, 2] for ARRAY, {"key": 2} for OBJECT and "string" for STRING
// ! This function does not allocate any memory. It just gives pointer to piece of original json_str buffer
// Return 0 if cursor type is INVALID
extern int jsonget_raw(const JsonGetCursor cursor, const char **out_string_start, int *out_length);

// Copy result of jsonget_raw function to buffer dest and truncate to max_length
// ! This function does not allocate any memory.
// Return 0 if cursor type is INVALID
extern int jsonget_raw_copy(const JsonGetCursor cursor, char *dest_buffer, int buffer_size, int *out_real_length);

// Function to get string from cursor
// Unlike jsonget_raw this function also unescape string
// If cursor type is not STRING, result is equivalent to jsonget_raw_copy
// ! This function does not allocate any memory.
// Return 0 if cursor type is INVALID
extern int jsonget_string(const JsonGetCursor cursor, char *dest_buffer, int buffer_size, int *out_real_length);

/*
** ------------------------------------------
** Utils
** ------------------------------------------
*/

// Return type of cursor.
// Instead of calling this function you can just use cursor.type field
extern int jsonget_type(const JsonGetCursor cursor);

// Return 1 if cursor is INVALID or NULL, otherwise return 0
extern int jsonget_isnull(const JsonGetCursor cursor);

// Return 1 if cursor is BOOLEAN and value is true, otherwise return 0
extern int jsonget_istrue(const JsonGetCursor cursor);

// Return count of elements in json array
// If cursor type is not ARRAY or OBJECT, return 0
extern int jsonget_array_count(const JsonGetCursor cursor);

// Compare string representation of cursor value with _str2_
// Return 0 if strings are equal
extern int jsonget_string_compare(const JsonGetCursor cursor, const char *str2);


#ifdef __cplusplus
}
#endif

#endif