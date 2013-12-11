/*
** 2013 December 4
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** 
*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_JSON)

#include <assert.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

#include "jsonget.h"

static void sqlitejsonDestructor(void *p)
{
  sqlite3_free(p);
}


#define SQLITEJSON_STATIC_STRING_BUFFER_SIZE 512

/*
**  Select one of sqlite3_result_* function to store json_obj value
*/
static void sqlitejsonWriteJsonValToContext(
  sqlite3_context *context,
  JsonGetCursor json_obj
){ 
    switch (json_obj.type)
    {
      case JSONGET_BOOLEAN:
      case JSONGET_INTEGER:
      {
        int val = 0;
        jsonget_int(json_obj, &val);
        sqlite3_result_int(context, val);
        break;
      }
      case JSONGET_NULL:
        sqlite3_result_null(context);
        break;
      case JSONGET_DOUBLE:
      {
        double val = 0;
        jsonget_double(json_obj, &val);
        sqlite3_result_double(context, val);
        break;
      }
      case JSONGET_STRING:
      {
        char static_buffer[SQLITEJSON_STATIC_STRING_BUFFER_SIZE];
        char *buf = static_buffer;
        int real_len;
        sqlite3_destructor_type destruct = SQLITE_TRANSIENT;
        // In the first time, try read string to stack
        if (jsonget_string(json_obj, buf, sizeof(static_buffer), &real_len))
        {
          if (real_len >= sizeof(static_buffer))
          {
            buf = sqlite3_malloc(real_len + 1);
            // read to heap
            jsonget_string(json_obj, buf, real_len + 1, &real_len);
            destruct = sqlitejsonDestructor;
          }
          sqlite3_result_text(context, buf, -1, destruct);
        }
        else sqlite3_result_null(context);
        break;
      }
      case JSONGET_ARRAY:
      case JSONGET_OBJECT:
      {
        const char *buf;
        int len, i;
        char* mem;
        if (jsonget_raw(json_obj, &buf, &len))
        {
          mem = sqlite3_malloc(len + 1);
          for(i = 0; i < len; i++) mem[i] = *buf++;
          mem[i] = 0;
          sqlite3_result_text(context, mem, len + 1, sqlitejsonDestructor);                
        }
        else sqlite3_result_null(context);
        break;
      }
      default:
        sqlite3_result_null(context);
        break;
  }  
}

/*
** Implementation of the json_get(json, key) function
** Parameters: 
**   json - json-string
**   key - path of retrieving key
**         'kobj', 'kobj1->kobj2', 'karray->1->kobj'
*/
static void sqlitejsonGetFunc(
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv
){  
  if (argc == 0)
  {
    sqlite3_result_error(context, "Invalid number of arguments", -1);
  }
  else if (sqlite3_value_type(argv[0]) == SQLITE_NULL) sqlite3_result_null(context);
  else
  {
      const char *json = (char*)sqlite3_value_text(argv[0]);
      JsonGetCursor json_obj, json_root;
      int i;
      json_root = jsonget(json);
      json_obj = json_root;
      for (i = 1; i < argc && json_obj.type != JSONGET_INVALID; i++)
      {
        if (sqlite3_value_type(argv[i]) == SQLITE_INTEGER)
        {
          int index = sqlite3_value_int(argv[i]);
          json_obj = jsonget_move_index(json_obj, index);
        }
        else
        {
          const char *key = (char*)sqlite3_value_text(argv[i]);
          json_obj = jsonget_move_key(json_obj, key);
        }
      }
      if (i != argc) sqlite3_result_null(context);
      else sqlitejsonWriteJsonValToContext(context, json_obj);
  }
}

/*
** Register the ICU extension functions with database db.
*/
int sqlite3JsonInit(sqlite3 *db){
  struct JsonScalar {
    const char *zName;                        /* Function name */
    int nArg;                                 /* Number of arguments */
    int enc;                                  /* Optimal text encoding */
    void *pContext;                           /* sqlite3_user_data() context */
    void (*xFunc)(sqlite3_context*,int,sqlite3_value**);
  } scalars[] = {
    {"json_get",   -1, SQLITE_ANY,         0, sqlitejsonGetFunc},
  };

  int rc = SQLITE_OK;
  int i;

  for(i=0; rc==SQLITE_OK && i<(int)(sizeof(scalars)/sizeof(scalars[0])); i++){
    struct JsonScalar *p = &scalars[i];
    rc = sqlite3_create_function(
        db, p->zName, p->nArg, p->enc, p->pContext, p->xFunc, 0, 0
    );
  }

  return rc;
}

#if !SQLITE_CORE
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_json_init(
  sqlite3 *db, 
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3JsonInit(db);
}
#endif

#endif
