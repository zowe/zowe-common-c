/*
  Comment Round-Trip Test for YAML <-> JSON Pipeline
  
  This program exercises the comment-preserving YAML round-trip:
    1. Scans a YAML file for comments
    2. Loads the YAML document via libyaml
    3. Converts to JSON tree, attaching comments
    4. Prints the JSON tree (showing comment fields)
    5. Writes the JSON back to YAML with comments preserved
  
  Usage:
    commenttest <yamlfile> [options]
  
  Options:
    -v           Verbose: show comment scan results
    -j           Print JSON representation
    -y           Print YAML output (default)
    -o <file>    Write YAML output to file instead of stdout
    -r           Round-trip: read -> convert -> write -> re-read -> compare
    -a           Show all (verbose + json + yaml)
    -A <mode>    Comment alignment: none, fixed, original (default: none)
    -W <width>   Pad width for fixed alignment (default: 40)
*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "yaml2json.h"

/* Helper: print a separator line */
static void printSep(const char *title) {
  printf("\n========== %s ==========\n\n", title);
}

/* Helper: recursively print JSON tree with comment annotations */
static void printJsonWithComments(Json *json, int depth) {
  char indent[256];
  int i;
  for (i = 0; i < depth * 2 && i < 254; i++) indent[i] = ' ';
  indent[i] = '\0';
  
  if (jsonIsObject(json)) {
    JsonObject *obj = jsonAsObject(json);
    const char *docComment = jsonObjectGetDocumentComment(obj);
    if (docComment) {
      printf("%s[DOC COMMENT] \"%s\"\n", indent, docComment);
    }
    printf("%s{\n", indent);
    JsonProperty *prop = jsonObjectGetFirstProperty(obj);
    while (prop) {
      const char *before = jsonPropertyGetBeforeComment(prop);
      if (before) {
        printf("%s  [BEFORE] \"%s\"\n", indent, before);
      }
      const char *key = jsonPropertyGetKey(prop);
      Json *val = jsonPropertyGetValue(prop);
      const char *inl = jsonPropertyGetInlineComment(prop);
      
      printf("%s  \"%s\": ", indent, key);
      if (jsonIsObject(val) || jsonIsArray(val)) {
        if (inl) {
          printf(" [INLINE] \"%s\"", inl);
        }
        printf("\n");
        printJsonWithComments(val, depth + 2);
      } else if (jsonIsString(val)) {
        printf("\"%s\"", jsonAsString(val));
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else if (jsonIsBoolean(val)) {
        printf("%s", jsonAsBoolean(val) ? "true" : "false");
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else if (jsonIsInt64(val)) {
#ifdef __ZOWE_OS_WINDOWS
        printf("%lld", jsonAsInt64(val));
#else
        printf("%ld", jsonAsInt64(val));
#endif
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else if (jsonIsNumber(val)) {
        printf("%d", jsonAsNumber(val));
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else if (jsonIsNull(val)) {
        printf("null");
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else {
        printf("(type=%d)", val->type);
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      }
      prop = jsonObjectGetNextProperty(prop);
    }
    printf("%s}\n", indent);
  } else if (jsonIsArray(json)) {
    JsonArray *arr = jsonAsArray(json);
    int count = jsonArrayGetCount(arr);
    printf("%s[\n", indent);
    for (int j = 0; j < count; j++) {
      Json *elem = jsonArrayGetItem(arr, j);
      const char *before = jsonGetBeforeComment(elem);
      if (before) {
        printf("%s  [BEFORE] \"%s\"\n", indent, before);
      }
      const char *inl = jsonGetInlineComment(elem);
      if (jsonIsObject(elem) || jsonIsArray(elem)) {
        printJsonWithComments(elem, depth + 1);
        if (inl) printf("%s  [INLINE] \"%s\"\n", indent, inl);
      } else if (jsonIsString(elem)) {
        printf("%s  \"%s\"", indent, jsonAsString(elem));
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      } else {
        printf("%s  <scalar>", indent);
        if (inl) printf("  [INLINE] \"%s\"", inl);
        printf("\n");
      }
    }
    printf("%s]\n", indent);
  }
}

/* Count comments that survived the round-trip */
static int countComments(Json *json) {
  int count = 0;
  if (jsonIsObject(json)) {
    JsonObject *obj = jsonAsObject(json);
    if (jsonObjectGetDocumentComment(obj)) count++;
    JsonProperty *prop = jsonObjectGetFirstProperty(obj);
    while (prop) {
      if (jsonPropertyGetBeforeComment(prop)) count++;
      if (jsonPropertyGetInlineComment(prop)) count++;
      count += countComments(jsonPropertyGetValue(prop));
      prop = jsonObjectGetNextProperty(prop);
    }
  } else if (jsonIsArray(json)) {
    JsonArray *arr = jsonAsArray(json);
    int cnt = jsonArrayGetCount(arr);
    for (int i = 0; i < cnt; i++) {
      Json *elem = jsonArrayGetItem(arr, i);
      if (jsonGetBeforeComment(elem)) count++;
      if (jsonGetInlineComment(elem)) count++;
      count += countComments(elem);
    }
  }
  return count;
}


/* Callback for custom JSON printer that writes to stdout */
static void stdoutWrite(jsonPrinter *p, char *text, int len) {
  fwrite(text, 1, len, stdout);
}

static JsonCommentAlign parseAlignMode(const char *s) {
  if (!strcmp(s, "fixed"))    return JSON_COMMENT_ALIGN_FIXED;
  if (!strcmp(s, "original")) return JSON_COMMENT_ALIGN_ORIGINAL;
  return JSON_COMMENT_ALIGN_NONE;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: commenttest <yamlfile> [-v] [-j] [-y] [-o outfile] [-r] [-p] [-a]\n");
    printf("       [-A none|fixed|original] [-W padwidth]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -v   Verbose: show comment scan results\n");
    printf("  -j   Print JSON with comment annotations\n");
    printf("  -p   Print raw JSON (via jsonPrintObject, pretty-printed)\n");
    printf("  -y   Print YAML output with comments (default)\n");
    printf("  -o   Write YAML output to specified file\n");
    printf("  -r   Round-trip test: write YAML, re-read, compare comment counts\n");
    printf("  -a   All: enable -v, -j, -y, -p\n");
    printf("  -A   Comment alignment mode: none (default), fixed, original\n");
    printf("  -W   Pad width for fixed alignment (default: 40)\n");
    return 1;
  }


  const char *inputFile = argv[1];
  /* Detect input format by extension */
  int inputIsJson = 0;
  {
    int slen = strlen(inputFile);
    if (slen > 5 && !strcmp(inputFile + slen - 5, ".json")) {
      inputIsJson = 1;
    }
  }
  int verbose = 0, showJson = 0, showYaml = 0, doRoundTrip = 0, showRawJson = 0;
  const char *outputFile = NULL;
  JsonCommentAlign alignMode = JSON_COMMENT_ALIGN_NONE;
  int padWidth = 40;

  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "-v")) verbose = 1;
    else if (!strcmp(argv[i], "-j")) showJson = 1;
    else if (!strcmp(argv[i], "-p")) showRawJson = 1;
    else if (!strcmp(argv[i], "-y")) showYaml = 1;
    else if (!strcmp(argv[i], "-a")) { verbose = 1; showJson = 1; showYaml = 1; showRawJson = 1; }
    else if (!strcmp(argv[i], "-r")) doRoundTrip = 1;
    else if (!strcmp(argv[i], "-o") && i + 1 < argc) outputFile = argv[++i];
    else if (!strcmp(argv[i], "-A") && i + 1 < argc) alignMode = parseAlignMode(argv[++i]);
    else if (!strcmp(argv[i], "-W") && i + 1 < argc) padWidth = atoi(argv[++i]);
  }

  /* Default: show YAML if nothing else specified */
  if (!showJson && !showYaml && !doRoundTrip && !verbose && !showRawJson) {
    showYaml = 1;
  }

  /* Build write options */
  YamlWriteOptions writeOpts;
  writeOpts.commentAlignMode = alignMode;
  writeOpts.commentPadWidth = padWidth;

  char errorBuf[1024];
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
  Json *json = NULL;
  int commentCount = 0;
  YamlCommentList *comments = NULL;
  yaml_document_t *document = NULL;

  if (inputIsJson) {
    /* JSON input path: parse JSON file directly, retaining comments */
    printSep("Parsing JSON file with comments");
    /* Read file into buffer using fopen/fread (avoids zowelog dependency in fileOpen) */
    FILE *fp = fopen(inputFile, "r");
    if (!fp) {
      fprintf(stderr, "ERROR: Could not open '%s'\n", inputFile);
      return 2;
    }
    fseek(fp, 0, SEEK_END);
    long fileLen = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *fileBuf = safeMalloc(fileLen + 1, "json file buffer");
    int bytesRead = fread(fileBuf, 1, fileLen, fp);
    fclose(fp);
    fileBuf[bytesRead] = '\0';

    json = jsonParseUnterminatedStringWithComments(slh, fileBuf, bytesRead, errorBuf, sizeof(errorBuf));
    safeFree(fileBuf, fileLen + 1);
    if (!json) {
      fprintf(stderr, "ERROR: Failed to parse JSON: %s\n", errorBuf);
      return 2;
    }
    commentCount = countComments(json);
    printf("JSON parsed from '%s' with %d comments retained\n", inputFile, commentCount);
  } else {
    /* YAML input path: scan comments, load YAML, convert to JSON */
    printSep("STEP 1: Scanning comments");
    comments = scanYamlComments(inputFile);
    if (!comments) {
      fprintf(stderr, "ERROR: Failed to scan comments from '%s'\n", inputFile);
      return 2;
    }
    printf("Scanned comments from '%s'\n", inputFile);

    if (verbose) {
      printSep("COMMENT SCAN RESULTS");
      printf("(Comment details will be visible in the JSON tree below)\n");
    }

    printSep("STEP 2: Loading YAML document");
    bool wasMissing = false;
    document = readYAML2(inputFile, errorBuf, sizeof(errorBuf), &wasMissing);
    if (!document) {
      fprintf(stderr, "ERROR: Failed to load YAML: %s\n", errorBuf);
      freeYamlComments(comments);
      return 3;
    }
    printf("YAML document loaded successfully\n");

    printSep("STEP 3: Converting YAML to JSON with comments");
    json = yaml2JSONWithComments(document, comments, slh);
    if (!json) {
      fprintf(stderr, "ERROR: Failed to convert YAML to JSON\n");
      freeYamlComments(comments);
      return 4;
    }
    commentCount = countComments(json);
    printf("JSON tree built with %d comments attached\n", commentCount);
  }

  /* Step 4: Show JSON with comments */
  if (showJson) {
    printSep("JSON TREE WITH COMMENTS");
    printJsonWithComments(json, 0);
  }

  /* Step 4b: Show raw JSON via jsonPrintObject */
  if (showRawJson) {
    printSep("RAW JSON (jsonPrintObject)");
    jsonPrinter *jp = makeCustomJsonPrinter(stdoutWrite, NULL);
    jsonEnablePrettyPrint(jp);
    jsonEnableCommentPrint(jp);
    jsonSetCommentAlignment(jp, alignMode, padWidth);
    jsonStartObject(jp, NULL);
    jsonPrintObject(jp, jsonAsObject(json));
    jsonEndObject(jp);
    printf("\n");
    freeJsonPrinter(jp);
  }

  /* Step 5: Write YAML with comments */
  if (showYaml || outputFile) {
    char *yamlBuffer = NULL;
    int yamlLen = 0;
    int status = json2Yaml2BufferWithComments(json, &yamlBuffer, &yamlLen, &writeOpts);
    if (status != 0) {
      fprintf(stderr, "ERROR: Failed to write YAML, status=%d\n", status);
    } else {
      if (showYaml) {
        printSep("YAML OUTPUT WITH COMMENTS");
        printf("%s", yamlBuffer);
      }
      if (outputFile) {
        FILE *outFp = fopen(outputFile, "w");
        if (outFp) {
          fwrite(yamlBuffer, 1, yamlLen - 1, outFp);
          fclose(outFp);
          printf("\nYAML written to '%s'\n", outputFile);
        } else {
          fprintf(stderr, "ERROR: Could not open '%s' for writing\n", outputFile);
        }
      }
      safeFree(yamlBuffer, yamlLen);
    }
  }

  /* Step 6: Round-trip test */
  if (doRoundTrip) {
    printSep("ROUND-TRIP TEST");
    
    /* Write to a temp buffer */
    char *rtBuffer = NULL;
    int rtLen = 0;
    int rtStatus = json2Yaml2BufferWithComments(json, &rtBuffer, &rtLen, &writeOpts);
    if (rtStatus != 0) {
      printf("FAIL: Could not write YAML for round-trip, status=%d\n", rtStatus);
    } else {
      /* Write to a temp file */
      const char *tmpFile = "/tmp/commenttest_rt.yaml";
      FILE *tmpFp = fopen(tmpFile, "w");
      if (tmpFp) {
        fwrite(rtBuffer, 1, rtLen - 1, tmpFp);
        fclose(tmpFp);
        
        /* Re-scan and re-load */
        YamlCommentList *rtComments = scanYamlComments(tmpFile);
        bool wasMissing = false;
        yaml_document_t *rtDoc = readYAML2(tmpFile, errorBuf, sizeof(errorBuf), &wasMissing);
        if (rtDoc && rtComments) {
          ShortLivedHeap *rtSlh = makeShortLivedHeap(0x10000, 100);
          Json *rtJson = yaml2JSONWithComments(rtDoc, rtComments, rtSlh);
          if (rtJson) {
            int rtCommentCount = countComments(rtJson);
            printf("Original comment count: %d\n", commentCount);
            printf("Round-trip comment count: %d\n", rtCommentCount);
            if (rtCommentCount >= commentCount) {
              printf("PASS: Comments preserved through round-trip!\n");
            } else {
              printf("PARTIAL: %d of %d comments preserved (%.0f%%)\n",
                     rtCommentCount, commentCount,
                     commentCount > 0 ? 100.0 * rtCommentCount / commentCount : 0.0);
            }
          } else {
            printf("FAIL: Could not convert round-trip YAML to JSON\n");
          }
          if (rtDoc) {
            yaml_document_delete(rtDoc);
            safeFree((char*)rtDoc, sizeof(yaml_document_t));
          }
          freeYamlComments(rtComments);
        } else {
          printf("FAIL: Could not re-load round-trip YAML\n");
        }
        remove(tmpFile);
      } else {
        printf("FAIL: Could not write temp file '%s'\n", tmpFile);
      }
      safeFree(rtBuffer, rtLen);
    }
  }

  /* Cleanup */
  printSep("SUMMARY");
  printf("Input file: %s\n", inputFile);
  printf("Comments found and attached: %d\n", commentCount);
  printf("Alignment mode: %s\n",
         alignMode == JSON_COMMENT_ALIGN_FIXED ? "fixed" :
         alignMode == JSON_COMMENT_ALIGN_ORIGINAL ? "original" : "none");
  if (alignMode == JSON_COMMENT_ALIGN_FIXED) {
    printf("Pad width: %d\n", padWidth);
  }
  printf("Test complete.\n");

  if (comments) {
    freeYamlComments(comments);
  }
  if (document) {
    yaml_document_delete(document);
    safeFree((char*)document, sizeof(yaml_document_t));
  }

  return 0;
}
