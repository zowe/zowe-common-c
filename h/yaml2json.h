#ifndef __ZOWE_YAML2JSON__
#define __ZOWE_YAML2JSON__ 1

#include "yaml.h"

yaml_document_t *readYAML(const char *filename, char *errorBuf, size_t errorBufSize);
yaml_document_t *readYAML2(const char *filename, char *errorBuf, size_t errorBufSize, bool *wasMissing);
void pprintYAML(yaml_document_t *document);
Json *yaml2JSON(yaml_document_t *document, ShortLivedHeap *slh);

#define YAML_SUCCESS 0
#define YAML_GENERAL_FAILURE 12

int json2Yaml2Buffer(Json *json, char **buffer, int *bufferLen);
int json2Yaml2File(Json *json, FILE *out);

/* ---- Comment-preserving YAML round-trip ---- */

/* Opaque comment list extracted from a YAML file */
typedef struct YamlCommentList_tag YamlCommentList;

/* Scan a YAML file for comments. Returns NULL on failure. Caller must free with freeYamlComments(). */
YamlCommentList *scanYamlComments(const char *filename);

/* Free a comment list returned by scanYamlComments */
void freeYamlComments(YamlCommentList *list);

/* Convert YAML document to JSON, attaching comments from the scanned list */
Json *yaml2JSONWithComments(yaml_document_t *document, YamlCommentList *comments, ShortLivedHeap *slh);

/* Options for the custom YAML writer (comment alignment, etc.) */
typedef struct YamlWriteOptions_tag {
  JsonCommentAlign commentAlignMode;  /* JSON_COMMENT_ALIGN_NONE, _FIXED, or _ORIGINAL */
  int              commentPadWidth;   /* column width for FIXED mode (default: 40) */
} YamlWriteOptions;

/* Write JSON as YAML with comment preservation to a buffer (caller must free buffer) */
int json2Yaml2BufferWithComments(Json *json, char **buffer, int *bufferLen, YamlWriteOptions *opts);

/* Write JSON as YAML with comment preservation to a FILE */
int json2Yaml2FileWithComments(Json *json, FILE *out, YamlWriteOptions *opts);


  
#endif
