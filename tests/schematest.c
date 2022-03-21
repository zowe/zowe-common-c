#include <yaml.h>
/* #include <yaml_private.h> */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#include "zowetypes.h"

#ifndef __ZOWE_OS_WINDOWS
#include <unistd.h>
#endif

#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "jsonschema.h"
#include "yaml2json.h"

/*
  Notes:

  (all work assumed to be done from shell in this directory)

  Windows Build ______________________________

    Assuming clang is installed

    set YAML=c:\repos\libyaml   ## Wherever you git clone'd libyaml

    clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

    clang -I%YAML%/include -I./src -I../h -I ../platform/windows -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 -o schematest.exe schematest.c %YAML%/src/api.c %YAML%/src/reader.c %YAML%/src/scanner.c %YAML%/src/parser.c %YAML%/src/loader.c %YAML%/src/writer.c %YAML%/src/emitter.c %YAML%/src/dumper.c ../c/yaml2json.c ../c/jsonschema.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c cppregex.o winregex.o

  ZOS Build _________________________________

  


  Running the Test ________________________________
     
     schematest <JsonOrYaml> <instancePath> <charset> <schemajson>?

     schematest yaml schemadata/example-zowe.yaml utf8 schemadata/zoweyaml.schema
 */

int main(int argc, char *argv[])
{
    int number;
    char *syntax = argv[1];
    char *filename = argv[2];
    char *charset = argv[3];
    char *schemaFilename = (argc >= 5 ? argv[4] : NULL);
    int errorBufferSize = 1024;
#ifdef __ZOWE_OS_WINDOWS
    int stdoutFD = _fileno(stdout);
#else
    int stdoutFD = STDOUT_FILENO;
#endif
    char *errorBuffer = safeMalloc(errorBufferSize,"ErrorBuffer");

    /* 
       Arg for charset
       functionalize the char tests
       materialize the char constants
       see if we can parse on other alphabets
       "soft" fork to JoeNemo
     */

    if (argc < 4) {
      printf(" <syntax> <filename> <charset> <schema>? \n");
      return 0;
    }

    Json *json = NULL;
    ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);

    if (!strcmp(syntax,"json")){  
      memset(errorBuffer,0,errorBufferSize);
      json = jsonParseFile2(slh,filename,errorBuffer,errorBufferSize);
      printf("json parsed json=0x%p, with error '%s'\n",json,errorBuffer);
      
    } else if (!strcmp(syntax,"yaml")){
      printf("yaml syntax case\n");
      fflush(stdout);
      yaml_document_t *doc = readYAML(filename, errorBuffer, errorBufferSize);
      printf("yaml doc at 0x%p\n",doc);
      if (doc){
        pprintYAML(doc);
        json = yaml2JSON(doc,slh);
      } else {
        printf("%s\n", errorBuffer);
      }
    } else {
      printf("unhandled syntax\n");
    }

    if (json){
      printf("Showing internal JSON whether from Yaml, JSON or PARMLIB\n");
      jsonPrinter *p = makeJsonPrinter(stdoutFD);
      jsonEnablePrettyPrint(p);
      jsonPrint(p,json);

      if (schemaFilename){
        memset(errorBuffer,0,errorBufferSize);
        Json *schemaJSON = jsonParseFile2(slh,schemaFilename,errorBuffer,errorBufferSize);
        printf("json parsed schema json=0x%p, with error '%s'\n",json,errorBuffer);
        fflush(stdout);
        if (schemaJSON){
          printf("Regurgitating JSON Schema before digesting it for real\n");
          jsonPrint(p,schemaJSON);
          printf("\n");
          fflush(stdout);
          JsonSchemaBuilder *builder = makeJsonSchemaBuilder(DEFAULT_JSON_SCHEMA_VERSION);
          JsonSchema *schema = jsonBuildSchema(builder,schemaJSON);
          if (schema){
            JsonValidator *validator = makeJsonValidator();
            printf("Before Validate\n");fflush(stdout);
            int validateStatus = jsonValidateSchema(validator,json,schema);
            switch (validateStatus){
            case JSON_VALIDATOR_NO_EXCEPTIONS:
              printf("No validity Exceptions\n");
              break;
            case JSON_VALIDATOR_HAS_EXCEPTIONS:
              {
                printf("Validity Exceptions:\n");
                ValidityException *e = validator->firstValidityException;
                while (e){
                  printf("  %s\n",e->message);
                  e = e->next;
                }
              }
              break;
            case JSON_VALIDATOR_INTERNAL_FAILURE:
              printf("validation internal failure");
            break;
          }
            freeJsonValidator(validator);
            printf("Done with Validation Test\n");
            fflush(stdout);
          } else {
            printf("Schema Build Failed: %s\n",builder->errorMessage);
          }
          freeJsonSchemaBuilder(builder); 
        } else {
          printf("Failed to read schema JSON\n");
        }
      }
    } else{
      printf("was not able to make JSON from source\n");
      
    }
    printf("JSON Test end\n");
    fflush(stdout);
    return 0;
}

