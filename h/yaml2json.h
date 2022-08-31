#ifndef __ZOWE_YAML2JSON__
#define __ZOWE_YAML2JSON__ 1

#include "yaml.h"

yaml_document_t *readYAML(const char *filename, char *errorBuf, size_t errorBufSize);
void pprintYAML(yaml_document_t *document);
Json *yaml2JSON(yaml_document_t *document, ShortLivedHeap *slh);

#endif
