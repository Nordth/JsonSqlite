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
*/

#include "jsonget.h"

/*
** ------------------------------------------
** Private section
** ------------------------------------------
*/

// Struct to represent up to 4-byte unicode char
typedef struct
{
	char c[4];
	int len;
} JsonGetUtf8Char;

// Condtion for fake do-while loop used in multiline macros
// Switch to second line if you want disable VS warning C4127: conditional expression is constant
#define JSONGET_FAKE_LOOP_CONDITION 0
// #define JSONGET_FAKE_LOOP_CONDITION (void)0,0

// Return invalid cursor
#define JSONGET_RETURN_INVALID_CURSOR do {JsonGetCursor ret_val = {0}; return ret_val;} while(JSONGET_FAKE_LOOP_CONDITION)

// Check if character is whitespace
#define JSONGET_IS_WHITESPACE(c) ((c) ==  ' ' || (c) == '\t'  || (c) == '\r' || (c) == '\n' || (c) == '\b' || (c) == '\f')

// Check if character is whitespace
#define JSONGET_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

// Skip spaces
#define JSONGET_SKIP_SPACES(pstr) do { while (JSONGET_IS_WHITESPACE(*(pstr))) (pstr)++; } while(JSONGET_FAKE_LOOP_CONDITION)

// Skip string content
#define JSONGET_SKIP_STRING_CONTENT(pstr, cquote, cescape)  do { int is_escape = 0; \
	while (*(pstr) && (*(pstr)  != (cquote) || is_escape)) { \
		if (*(pstr) == (cescape)) is_escape = !is_escape; else is_escape = 0; (pstr)++; \
	}} while(JSONGET_FAKE_LOOP_CONDITION)

// Fill JsonGetUtf8Char with one char
#define JSONGET_ANSIUNICODE_CHAR(uchar_var, achar) do { JsonGetUtf8Char ch = {0}; ch.len = 1; ch.c[0] = (achar); (uchar_var) = ch; } while(JSONGET_FAKE_LOOP_CONDITION)

// Unicode first byte marks
#define JSONGET_UTF8_FIRST_BYTE_MARK(x) \
	((x) == 0 || (x) == 1 ? 0x00 : \
	((x) == 2 ? 0xC0 : \
	((x) == 3 ? 0xE0 : \
	((x) == 4 ? 0xF0 : \
	((x) == 5 ? 0xF8 : 0xFC \
	)))))

#define JSONGET_CHAR_HEX(x) \
	((x) >= '0' && (x) <= '9' ? (x) - '0' : \
	((x) >= 'a' && (x) <= 'f' ? (x) - 'a' + 10 : \
	((x) >= 'A' && (x) <= 'F' ? (x) - 'A' + 10 : -1 \
	)))


// Make cursor with type JSON_PAIR and specified str pointer
static JsonGetCursor pjsonget_make_pair_cursor(const char *pstr)
{
	JSONGET_SKIP_SPACES(pstr);
	if (*pstr)
	{
		JsonGetCursor ret_val;
		ret_val.pstr = pstr;
		ret_val.type = JSONGET_PAIR;
		return ret_val;
	}
	else JSONGET_RETURN_INVALID_CURSOR;
}

// Get cursor of string pointer
static JsonGetCursor pjsonget_decode_cursor(const char *pstr)
{
	JSONGET_SKIP_SPACES(pstr);
	if (*pstr)
	{
		JsonGetCursor ret_val;
		ret_val.pstr = pstr;
		switch (*pstr)
		{
			case '{': ret_val.type = JSONGET_OBJECT; break;
			case '[': ret_val.type = JSONGET_ARRAY; break;
			case 'n': ret_val.type = JSONGET_NULL; break;
			case 't': 
			case 'f': ret_val.type = JSONGET_BOOLEAN; break;
			case '"': ret_val.type = JSONGET_STRING; break;
			default:
			{
				// Skip '-' of negative number
				if (*pstr == '-') pstr++;
				if (JSONGET_IS_DIGIT(*pstr))
				{
					// if number, check it is int or double
					while (JSONGET_IS_DIGIT(*pstr)) pstr++;
					if (*pstr == '.') ret_val.type = JSONGET_DOUBLE;
					else ret_val.type = JSONGET_INTEGER;
				}
				else JSONGET_RETURN_INVALID_CURSOR;
			}
		}
		return ret_val;
	}
	else JSONGET_RETURN_INVALID_CURSOR;
}

// Copy one string to another
static void pjsonget_copy_str(const char* from, int from_len, char* to, int to_len)
{
	while (from_len > 0 && to_len > 1)
	{
		*to++ = *from++;
		from_len--;
		to_len--;
	}
	*to = 0;
}

static int pjson_read_hex4(const char *pstr, unsigned *out_hex_val)
{
	int read;
	*out_hex_val = 0;
	for (read = 0; read < 4; read++)
	{
		char h = JSONGET_CHAR_HEX(pstr[read]);
		if (h == -1) return read;
		*out_hex_val = (*out_hex_val << 4) + h;
	}
	return read;
}

// Try to read character from string
// Return number of read characters
static int pjson_read_string_char(const char *pstr, JsonGetUtf8Char* out_char)
{
	int rc = 1;
	if (!*pstr || *pstr == '"') 
	{
		out_char->len = 0;
		return 0;
	}
	else if (*pstr == '\\')
	{
		switch (pstr[rc])
		{
			case '"': 
			case '\\':
			case '/': JSONGET_ANSIUNICODE_CHAR(*out_char, pstr[rc]); rc++; break;
			case 'f': JSONGET_ANSIUNICODE_CHAR(*out_char, '\f'); rc++; break;
			case 't': JSONGET_ANSIUNICODE_CHAR(*out_char, '\t'); rc++; break;
			case 'r': JSONGET_ANSIUNICODE_CHAR(*out_char, '\r'); rc++; break;
			case 'n': JSONGET_ANSIUNICODE_CHAR(*out_char, '\n'); rc++; break;
			case 'b': JSONGET_ANSIUNICODE_CHAR(*out_char, '\b'); rc++; break;
			default:
			{
				int unicode_ok = 0;
				if (pstr[rc] == 'u')
				{
					do
					{
						// Decode utf16 to utf8
						int read; unsigned h1;
						if ((read = pjson_read_hex4(pstr + rc + 1, &h1)) < 4) break;
						rc += read + 1;
											
						// Single and double chars
						if (h1 < 0x80) out_char->len = 1;
						else if (h1 < 0x800) out_char->len = 2;
						// Surrogate pairs
						else if (h1 >= 0xD800 && h1 <= 0xDBFF)
						{
							unsigned h_low;
							if (pstr[rc] != '\\' || pstr[rc + 1] != 'u') break; 
							if ((read = pjson_read_hex4(pstr + rc + 2, &h_low)) < 4) break;
							rc += read + 2;
							if (h_low >= 0xDC00 || h_low <= 0xDFFF)
							{
								h1 = (((h1 - 0xD800) << 10) | (h_low - 0xDC00)) + 0x10000;
								if (h1 < 0x110000) out_char->len = 4;
								else
								{
									// Unknown character
									out_char->len = 3;
									h1 = 0x0000FFFD;
								}
							}
							// Wrong low surrogate
							else break;
						}
						// Wrong values, this also forbids \u0000
						else if (h1 >= 0xDC00 && h1 <= 0xDFFF || h1 == 0)  break;
						// Triple chars
						else if (h1 < 0x10000) out_char->len = 3;

						if (out_char->len == 4) { out_char->c[3] = (char)((h1 | 0x80) & 0xBF); h1 >>= 6; }
						if (out_char->len >= 3) { out_char->c[2] = (char)((h1 | 0x80) & 0xBF); h1 >>= 6; }
						if (out_char->len >= 2) { out_char->c[1] = (char)((h1 | 0x80) & 0xBF); h1 >>= 6; }
						if (out_char->len >= 1) { out_char->c[0] = (char)(h1 | JSONGET_UTF8_FIRST_BYTE_MARK(out_char->len)); }

						unicode_ok = 1;

					} while (JSONGET_FAKE_LOOP_CONDITION); // fake cycle to use break 
				}
				if (!unicode_ok)
				{
					// Wrong sequence
					JSONGET_ANSIUNICODE_CHAR(*out_char, '\\');
					rc = 1;
					break;
				}
			}
		}
		
	}
	else JSONGET_ANSIUNICODE_CHAR(*out_char, *pstr);
	return rc;
}

// Return integer under *pstr and move to end of integer
static int pjson_eat_int(const char** ppstr)
{
	int sign = **ppstr == '-';
	int res = 0;	
	if (**ppstr == '-' || **ppstr == '+') (*ppstr)++;
	while (JSONGET_IS_DIGIT(**ppstr)) 
	{
		res = res * 10 + (**ppstr - '0');
		(*ppstr)++;
	} 
	return sign ? -res : res;
}

// Read integer and double representation of number under pstr
static void pjson_read_number(const char* pstr, int *out_as_int, double *out_as_double)
{
	const char *p = pstr;
	// Read int 
	*out_as_int = pjson_eat_int(&p);

	// Read double
	*out_as_double = *out_as_int;
	if (*p == '.')
	{
		double scale = *out_as_int > 0 ? 0.1 : -0.1;
		p++;
		while (JSONGET_IS_DIGIT(*p))
		{
			*out_as_double += scale * (*p - '0');
			scale /= 10;
			p++;
		}
	}

	// Read mantissa
	if (*p == 'e' || *p == 'E')
	{
		int e;
		p++;
		e = pjson_eat_int(&p);
		if (e >= 0) while (e) { *out_as_double *= 10; *out_as_int *= 10;  e--; }
		else while (e) { *out_as_double /= 10; *out_as_int /= 10;  e++; }
	}
}

// Skip specified word
// if _pstr_ is less than _word_ or _pstr_ doesn't contain _word_ return 0, otherwise return 1
static int pjson_skip_word(const char **ppstr, const char *word)
{
	while (**ppstr && *word && **ppstr == *word)
	{
		(*ppstr)++;
		word++;
	}
	return (*word) ? 0 : 1;
}

// Skip value in json
// is_pair - skip pair key : value
// Return 1 when ok, 0 if parse error
static int pjson_skip_val(const char **ppstr, int is_pair)
{
	switch (**ppstr)
	{
		case '{':
		case '[':
		{
			char closec = **ppstr == '[' ? ']' : '}';
			(*ppstr)++; // skip { or [
			JSONGET_SKIP_SPACES(*ppstr);
			while (**ppstr && **ppstr != closec)
			{
				pjson_skip_val(ppstr, closec == '}');
				JSONGET_SKIP_SPACES(*ppstr);
				if (**ppstr == ',') (*ppstr)++;
				JSONGET_SKIP_SPACES(*ppstr);
			}
			if (**ppstr) (*ppstr)++; // skip } or ]
			else return 0;
			break;
		}
		case '"':
		{
			(*ppstr)++; // skip "
			JSONGET_SKIP_STRING_CONTENT(*ppstr, '"', '\\');
			if (**ppstr) (*ppstr)++; // skip "
			else return 0;
			break;
		}
		case 't': if (!pjson_skip_word(ppstr, "true")) return 0;
		case 'f': if (!pjson_skip_word(ppstr, "false")) return 0;
		case 'n': if (!pjson_skip_word(ppstr, "null")) return 0;
		default:
			// Skip '-' of negative number
			if (**ppstr == '-') (*ppstr)++;
			if (JSONGET_IS_DIGIT(**ppstr))
			{
				while(JSONGET_IS_DIGIT(**ppstr) || **ppstr == '.' || 
							     **ppstr == 'e' || **ppstr == 'E' ||
								 **ppstr == '+' || **ppstr == '-')   (*ppstr)++;
			}
			else 
			{
				(*ppstr)++; // skip wrong char
				return 0;
			}
	}
	if (is_pair)
	{
		JSONGET_SKIP_SPACES(*ppstr);
		if (**ppstr == ':') (*ppstr)++; // skip :
		else return 0;
		JSONGET_SKIP_SPACES(*ppstr);
		pjson_skip_val(ppstr, 0);
	}
	return 1;
}

// Compare string representation of cursor value with _str2_
static int pjsonget_string_compare(const JsonGetCursor cursor, const char *str2, const char** out_token_end)
{
	const char *p = cursor.pstr;
	if (cursor.type == JSONGET_INVALID) return -1;
	
	*out_token_end = cursor.pstr;
	if (cursor.type == JSONGET_STRING || cursor.type == JSONGET_PAIR)
	{
		int read;
		JsonGetUtf8Char uchar;
		int diff = 0;
		if (*p == '\"') p++; // skip "
		else return -1;
		while (!diff && *str2 && (read = pjson_read_string_char(p, &uchar)) > 0)
		{
			int i;
			p += read;
			for (i = 0; i < uchar.len && diff == 0 && *str2; i++, str2++) diff = uchar.c[i] - *str2;
			if (!*str2 && i != uchar.len) diff = uchar.c[i];
		}
		if (!read) diff = 0 - *str2;
		else 
		{ 
			// Move to end of string value			
			JSONGET_SKIP_STRING_CONTENT(p, '"', '\\');
		}
		
		if (*p == '"') p++;
		*out_token_end = p;
		return diff;
	}
	else
	{
		// Move to end of value
		if (!pjson_skip_val(out_token_end, 0)) return -1;
		while (p != *out_token_end && *str2 && *str2 == *p)
		{
			p++;
			str2++;
		}
		return (p != *out_token_end ? *p : 0) - *str2;
	}
}
/*
** ------------------------------------------
** Init cursor
** ------------------------------------------
*/

// Create json cursor from NULL-terminated json_str buffer
JsonGetCursor jsonget(const char *json_str)
{
	return pjsonget_decode_cursor(json_str);
}


/*
** ------------------------------------------
** Move cursor
** ------------------------------------------
*/

// Move to _key_ field of current json object
JsonGetCursor jsonget_move_key(const JsonGetCursor cursor, const char* key)
{
	if (cursor.type == JSONGET_OBJECT)
	{
		int not_found = 1;
		JsonGetCursor ckey;
		const char *p = cursor.pstr;
		p++; // skip {
		while(not_found && *p && *p != '}')
		{
			ckey = pjsonget_decode_cursor(p);
			not_found = pjsonget_string_compare(ckey, key, &p);
			if (not_found)
			{
				JSONGET_SKIP_SPACES(p);
				if (*p == ':') p++; // skip :
				else JSONGET_RETURN_INVALID_CURSOR;
				JSONGET_SKIP_SPACES(p);
				if (!pjson_skip_val(&p, 0)) JSONGET_RETURN_INVALID_CURSOR;
				JSONGET_SKIP_SPACES(p);
				if (*p == ',') p++;
			}
		}
		if (not_found) JSONGET_RETURN_INVALID_CURSOR;
		else
		{
			JSONGET_SKIP_SPACES(p);
			if (*p == ':') p++; // skip :
			else JSONGET_RETURN_INVALID_CURSOR;
			return pjsonget_decode_cursor(p);
		}
	}
	else JSONGET_RETURN_INVALID_CURSOR;
}

// Move to _index_ index of current json array
JsonGetCursor jsonget_move_index(const JsonGetCursor cursor, const int index)
{
	if (cursor.type == JSONGET_ARRAY || cursor.type == JSONGET_OBJECT)
	{
		int i = 0;
		const char *p = cursor.pstr;
		char closec = *p == '[' ? ']' : '}';
		if (*p) p++; // skip [ or {
		while (*p && i != index && *p != closec)
		{
			JSONGET_SKIP_SPACES(p);
			if (!pjson_skip_val(&p, cursor.type == JSONGET_OBJECT)) JSONGET_RETURN_INVALID_CURSOR;
			JSONGET_SKIP_SPACES(p);
			if (*p == ',') p++;
			i++;
		}
		if (i == index)
		{
			if (cursor.type == JSONGET_OBJECT) return pjsonget_make_pair_cursor(p);
			else return pjsonget_decode_cursor(p);
		}
		else JSONGET_RETURN_INVALID_CURSOR;
	}
	else JSONGET_RETURN_INVALID_CURSOR;
}

// Move to next element in array or pair in json object
JsonGetCursor jsonget_move_next(const JsonGetCursor cursor)
{
	if (cursor.type != JSONGET_INVALID)
	{
		const char *p = cursor.pstr;
		if (!pjson_skip_val(&p, cursor.type == JSONGET_PAIR)) JSONGET_RETURN_INVALID_CURSOR;
		JSONGET_SKIP_SPACES(p);
		if (*p == ',') p++;
		else JSONGET_RETURN_INVALID_CURSOR;
		if (cursor.type == JSONGET_PAIR) return pjsonget_make_pair_cursor(p);
		else return pjsonget_decode_cursor(p);
	}
	else JSONGET_RETURN_INVALID_CURSOR;
}

// Move to pair value 
JsonGetCursor jsonget_move_pair_value(const JsonGetCursor cursor)
{
	if (cursor.type == JSONGET_PAIR)
	{
		const char *p = cursor.pstr;
		if (!pjson_skip_val(&p, 0)) JSONGET_RETURN_INVALID_CURSOR;
		JSONGET_SKIP_SPACES(p);
		if (*p == ':') p++;
		else JSONGET_RETURN_INVALID_CURSOR;
		return pjsonget_decode_cursor(p);
	}
	else JSONGET_RETURN_INVALID_CURSOR;

}


/*
** ------------------------------------------
** Read values from cursor
** ------------------------------------------
*/

// Function to get integer value from cursor
int jsonget_int(const JsonGetCursor cursor, int* out_int)
{
	switch (cursor.type)
	{
		case JSONGET_NULL: *out_int = 0; return 1;
		case JSONGET_BOOLEAN: *out_int = *cursor.pstr == 't' ? 1 : 0; return 1;
		case JSONGET_DOUBLE: 
		case JSONGET_INTEGER:
		{
			double unused;
			pjson_read_number(cursor.pstr, out_int, &unused);
			return 1;
		}
		default: return 0;
	}
}

// Function to get float-point value from cursor
int jsonget_double(const JsonGetCursor cursor, double* out_double)
{
	int unused;
	if (cursor.type == JSONGET_DOUBLE || cursor.type == JSONGET_INTEGER)
	{
		pjson_read_number(cursor.pstr, &unused, out_double);
		return 1;
	}
	else return 0;
}

// Function to get raw representation of cursor value
int jsonget_raw(const JsonGetCursor cursor, const char **out_string_start, int *out_length)
{
	if (cursor.type != JSONGET_INVALID)
	{
		const char *p = cursor.pstr;
		*out_string_start = p;
		if (!pjson_skip_val(&p, 0)) return 0;
		*out_length = p - *out_string_start;
		return 1;
	}
	return 0;
}

// Copy result of jsonget_raw function to buffer dest and truncate to max_length
int jsonget_raw_copy(const JsonGetCursor cursor, char *dest_buffer, int buffer_size, int *out_real_length)
{
	const char *string_start;
	int length;
	int ok = jsonget_raw(cursor, &string_start, &length);
	if (ok)
	{
		pjsonget_copy_str(string_start, length, dest_buffer, buffer_size);
		*out_real_length = length;
	}
	return ok;
}

// Function to get string from cursor
int jsonget_string(const JsonGetCursor cursor, char *dest_buffer, int buffer_size, int *out_real_length)
{
	if (cursor.type == JSONGET_STRING || cursor.type == JSONGET_PAIR)
	{
		int read;
		const char *p = cursor.pstr;
		JsonGetUtf8Char uchar;
		*out_real_length = 0;
		if (*p == '"') p++;
		else return 0;
		while ((read = pjson_read_string_char(p, &uchar)) > 0)
		{
			int i;
			for (i = 0; i < uchar.len; i++) 
			{
				if (buffer_size > 1) 
				{
					*dest_buffer++ = uchar.c[i];
					buffer_size--;
				}
			}
			*out_real_length += uchar.len;
			p += read;
		}
		*dest_buffer = 0;
		return 1;
	}
	else return jsonget_raw_copy(cursor, dest_buffer, buffer_size, out_real_length);
}

/*
** ------------------------------------------
** Utils
** ------------------------------------------
*/

// Return type of cursor.
int jsonget_type(const JsonGetCursor cursor)
{
	return cursor.type;
}

// Return 1 if cursor is INVALID or NULL, otherwise return 0
int jsonget_isnull(const JsonGetCursor cursor)
{
	return cursor.type == JSONGET_INVALID || cursor.type == JSONGET_NULL;
}

// Return 1 if cursor is BOOLEAN and value is true, otherwise return 0
int jsonget_istrue(const JsonGetCursor cursor)
{
	return cursor.type == JSONGET_BOOLEAN && *cursor.pstr == 't';
}

// Return count of elements in json array
int jsonget_array_count(const JsonGetCursor cursor)
{
	if (cursor.type == JSONGET_ARRAY || cursor.type == JSONGET_OBJECT)
	{
		int i = 0;
		JsonGetCursor cur = jsonget_move_index(cursor, 0);
		while (cur.type != JSONGET_INVALID)
		{
			i++;
			cur = jsonget_move_next(cur);
		}
		return i;
	}
	else return 0;
}

// Compare string representation of cursor value with _str2_
int jsonget_string_compare(const JsonGetCursor cursor, const char *str2)
{
	const char *unused;
	return pjsonget_string_compare(cursor, str2, &unused);
}
