#pragma once
typedef struct cJSON {
    struct cJSON *next, *prev, *child;
    int type; char *valuestring; int valueint; double valuedouble; char *string;
} cJSON;
static inline cJSON *cJSON_Parse(const char *val) { return 0; }
static inline cJSON *cJSON_GetObjectItem(const cJSON *obj, const char *key) { return 0; }
static inline void cJSON_Delete(cJSON *obj) {}
#define cJSON_IsString(x) (0)
#define cJSON_IsNumber(x) (0)
#define cJSON_IsBool(x) (0)
static inline cJSON *cJSON_CreateArray(void) { return 0; }
static inline cJSON *cJSON_CreateObject(void) { return 0; }
static inline void cJSON_AddNumberToObject(cJSON* a, const char* b, double c) {}
static inline void cJSON_AddStringToObject(cJSON* a, const char* b, const char* c) {}
static inline void cJSON_AddBoolToObject(cJSON* a, const char* b, int c) {}
static inline void cJSON_AddItemToArray(cJSON* a, cJSON* b) {}
static inline char *cJSON_PrintUnformatted(cJSON* a) { return 0; }
static inline int cJSON_IsArray(const cJSON* a) { return 0; }
static inline int cJSON_GetArraySize(const cJSON* a) { return 0; }
static inline cJSON *cJSON_GetArrayItem(const cJSON* a, int b) { return 0; }
static inline int cJSON_IsObject(const cJSON* a) { return 0; }
static inline int cJSON_IsTrue(const cJSON* a) { return 1; }
