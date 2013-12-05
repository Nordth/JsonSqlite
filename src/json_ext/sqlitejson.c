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

#include <math.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

#define FLOAT_EPSILON 0.00000001
#include "cJSON.h"


/*
**  Select one of sqlite3_result_* function to store json_obj value
*/
static void writeJsonValToContext(
  sqlite3_context *context,
  cJSON* json_obj
){ 
  if (json_obj)
  {
    switch (json_obj->type)
    {
      case cJSON_False:
        sqlite3_result_int(context, 0);
        break;
      case cJSON_True:
        sqlite3_result_int(context, 1);
        break;
      case cJSON_NULL:
        sqlite3_result_null(context);
        break;
      case cJSON_Number:
        if (fabs(json_obj->valuedouble - json_obj->valueint) < FLOAT_EPSILON)
          sqlite3_result_int(context, json_obj->valueint);
        else
          sqlite3_result_double(context, json_obj->valuedouble);
        break;
      case cJSON_String:
        sqlite3_result_text(context, json_obj->valuestring, -1, SQLITE_TRANSIENT);
        break;
      case cJSON_Array:
      case cJSON_Object:
      {
        char *selected_json = cJSON_PrintUnformatted(json_obj);
        sqlite3_result_text(context, selected_json, -1, SQLITE_TRANSIENT);
        sqlite3_free(selected_json);
        break;
      }
    }
  }
  else sqlite3_result_null(context);
}

/*
** Implementation of the json_get(json, key) function
** Parameters: 
**   json - json-string
**   key - path of retrieving key
**         'kobj', 'kobj1->kobj2', 'karray->1->kobj'
*/
static void jsonGetFunc(
  sqlite3_context *context, 
  int argc, 
  sqlite3_value **argv
){  
  if (argc == 0)
  {
    sqlite3_result_error(context, "Invalid number of arguments", -1);
  }
  else
  {
    const unsigned char *json = sqlite3_value_text(argv[0]);
    cJSON *json_obj, *json_root;
    int i;
    json_root = cJSON_Parse(json);
    json_obj = json_root;
    for (i = 1; i < argc && json_obj; i++)
    {
      if (sqlite3_value_type(argv[i]) == SQLITE_INTEGER)
      {
        int index = sqlite3_value_int(argv[i]);
        json_obj = cJSON_GetArrayItem(json_obj, index);
      }
      else
      {
        const unsigned char *key = sqlite3_value_text(argv[i]);
        json_obj = cJSON_GetObjectItem(json_obj, key);
      }
    }
    if (i != argc) sqlite3_result_null(context);
    else writeJsonValToContext(context, json_obj);
    cJSON_Delete(json_root);
  }
}

// Malloc and free hooks for cJSON
static cJSON_Hooks cJsonHooks;

static void *memAlloc(size_t len)
{
    return sqlite3_malloc(len);
}

static void memFree(void *pointer)
{
    sqlite3_free(pointer);
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
    {"json_get",   -1, SQLITE_ANY,         0, jsonGetFunc},
  };

  int rc = SQLITE_OK;
  int i;

  for(i=0; rc==SQLITE_OK && i<(int)(sizeof(scalars)/sizeof(scalars[0])); i++){
    struct JsonScalar *p = &scalars[i];
    rc = sqlite3_create_function(
        db, p->zName, p->nArg, p->enc, p->pContext, p->xFunc, 0, 0
    );
  }

  // Init cJSON
  cJsonHooks.malloc_fn = memAlloc;
  cJsonHooks.free_fn = memFree;
  cJSON_InitHooks(&cJsonHooks);

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
