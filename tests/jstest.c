#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "yaml2json.h"
#include "quickjs.h"

#ifndef __ZOWE_OS_WINDOWS
#include <unistd.h>
#endif

/* This hack is needed because we can mention incomplete types, but not actually *USE* them */
struct JSValueBox_tag {
  JSValue value;
};
#include "embeddedjs.h"

/*

  set QJS=c:\repos\quickjs
  
  set YAML=c:\repos\libyaml   ## Wherever you git clone'd libyaml
  
  clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

  clang -I %QJS%/porting -I%YAML%/include -I../platform/windows -I %QJS% -I ..\h -DCONFIG_VERSION=\"2021-03-27\" -D_CRT_SECURE_NO_WARNINGS -Dstrdup=_strdup -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 -Wdeprecated-declarations --rtlib=compiler-rt -o jstest.exe jstest.c %QJS%\quickjs.c %QJS%\cutils.c %QJS%\quickjs-libc.c %QJS%\libbf.c %QJS%\libregexp.c %QJS%\libunicode.c %QJS%\porting\winpthread.c %QJS%\porting\wintime.c %QJS%\porting\windirent.c %QJS%\porting\winunistd.c %QJS%\repl.c %YAML%/src/api.c %YAML%/src/reader.c %YAML%/src/scanner.c %YAML%/src/parser.c %YAML%/src/loader.c %YAML%/src/writer.c %YAML%/src/emitter.c %YAML%/src/dumper.c ../c/yaml2json.c ../c/embeddedjs.c ../c/jsonschema.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c cppregex.o winregex.o ../platform/windows/winfile.c


  A software configuration is a virtual hierarchical document
    - representable as JSON in memory (Javascript data structures, more precisely)
    - not necessarily in one source document
    - can be cloned with modifications for HA or Provisioning Needs
    - can be semantically validated as a pre-flight mechanism to launch components, or from config tools
          - not all things can be validate, but most can

  The ConfigManager is the single interface to the software configuation
    - it is callable/embeddabl from all points
        - Windows, Linux, ZOS at minimum
        - JavaScript,C Java Bindings
        - output to STDOUT for scripting
        - it is embodied in a staticly linked executable that is part of the base software install
    - like JDBC client.  You get the data and query semantics w/o exactly knowing where every
    - it is bootstrapped from a minimal set of schemas and file system paths
    - it self-augments through property values that bring in more schemas and path elements
    - it provides output of its state as environment variables for components that are
         not highly configuration
    - is THE ONLY means of ZOWE component learning its parameters, outside of legacy components that read envvars

  A configuration source is a user (or UI) created document
    - is owned by the user
    - is text
    - "No Dark Matter" - all source/paths must be known
    - Zowe-wide Changes to sources/path must be reviewed
    - can be place under Version Control, eg Git
    - is not modified by installers, launchers, etc.
    - can be syntactically validated and prompted for syntax in editors
    - configuration sources have template capabilities, ie are not static data structures,
      - can always co-reference other parts of same source
      - can reference after merge/overlay any part of schema
      - can be evaluated with additional args/globals defined at different times in the InstallationTarget's lifecycle
    - SSL may be an exception to these rules - 

  An RunTarget is the unit software instance request
    - RunTargets can require other IT's by name and version.
    - RunTargets have sections of the SoftwareConfiguration that probably come from separate ConfigSource
    - RunTargets form a directed acyclic graph (like Make targets, OSGI Modules, NPM Packages) to be instantiale
    - RunTargets have a lifecycle - 
          initialInstall
          upgrade
          clone(<for_ha_or_provisioning>)
          start
          kill (maybe, for optional cleanup)
          uninstall
 */


int main(int argc, char **argv){
  int i;
  int include_count = 0;
  char *command = argv[1];
  int loadMode = EJS_LOAD_DETECT_MODULE;
  int errorBufferSize = 1024;
#ifdef __ZOWE_OS_WINDOWS
  int stdoutFD = _fileno(stdout);
#else
  int stdoutFD = STDOUT_FILENO;
#endif
  char *errorBuffer = safeMalloc(errorBufferSize,"ErrorBuffer");
  
  printf("JSTest Start \n");
  fflush(stdout);
  
  EmbeddedJS *ejs = allocateEmbeddedJS(NULL);
  configureEmbeddedJS(ejs,NULL,0,argc,argv);

  /* no includes yet 
  for(i = 0; i < include_count; i++) {
    if (eval_file(ctx, include_list[i], module))
      goto fail;
  }
  */
  int evalStatus = 0;

  /* JOE, here, eval buffer, or file, or go interactive */
  if (!strcmp(command,"expr")){
    char *sourceCode = argv[2];
    JSValueBox output = ejsEvalBuffer(ejs, sourceCode, strlen(sourceCode), "<cmdline>", 0, &evalStatus);
    if (evalStatus){
      printf("evaluation failed, status=%d\n",evalStatus);
    } else {
      ejsFreeJSValue(ejs,output);
    }
  } else if (!strcmp(command,"file")){
    char *filename = argv[2];
    if (ejsEvalFile(ejs, filename, loadMode)){
      printf("file eval failed\n");
    }
  } else if (!strcmp(command,"yamlcopy") ||
             !strcmp(command,"yamleval") ||
             !strcmp(command,"yamlreadwrite")){
    char *filename = argv[2];
    ShortLivedHeap *slh = makeShortLivedHeap(0x10000, 100);
    yaml_document_t *doc = readYAML(filename, errorBuffer, errorBufferSize);
    if (doc == NULL){
      printf("yaml file is bad: %s\n",errorBuffer);
      return 0;
    }
    Json *json = yaml2JSON(doc,slh);
    jsonPrinter *p = makeJsonPrinter(stdoutFD);
    jsonEnablePrettyPrint(p);
    printf("Yaml file as json\n");
    jsonPrint(p,json);

    if (!strcmp(command,"yamlcopy")){
      JSValueBox jsValue = ejsJsonToJS(ejs,json);
      printf("made jsValue\n");
      fflush(stdout);
      ejsSetGlobalProperty(ejs,"magic1",jsValue);
      char *s1 = "console.log(\"theThing=\"+JSON.stringify(magic1))";
      JSValueBox throwAway = ejsEvalBuffer(ejs, s1, strlen(s1), "<cmdLine>", 0, &evalStatus);
      printf("evalStatus=%d\n",evalStatus);
      Json *json2 = ejsJSToJson(ejs,jsValue,slh);
      printf("JSON translated back to zowe-common-c.json\n");
      jsonPrint(p,json2);
    } else if (!strcmp(command,"yamlreadwrite")){
      char *buffer = NULL;
      int bufferLength = 0;
      int status = json2Yaml2Buffer(json,&buffer,&bufferLength);
      printf("______ Done Yaml Write Status is %d ________________\n",status);
      if (status == YAML_SUCCESS){
        printf("%s\n",buffer);
      }
    } else {
      Json *resultantJSON = evaluateJsonTemplates(ejs,slh,json);
      printf("JSON templates evaluated\n");
      jsonPrint(p,resultantJSON);
    }
  }

  printf("JSTest done\n");
  fflush(stdout);
  return 0;
}
