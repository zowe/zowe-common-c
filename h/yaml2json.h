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

  
#endif
