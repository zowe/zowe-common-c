

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE 

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <math.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "charsets.h"
#include "collections.h"
#include "json.h"
#include "jsonschema.h"
#include "yaml.h"
#include "yaml2json.h"
#include "embeddedjs.h"
#ifdef METTLE 

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>
#include <metal/ctype.h>  
#include <metal/stdbool.h>  
#include "metalio.h"
#include "qsam.h"

#else

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>  
#include <sys/stat.h>
#include <sys/types.h> 
#include <stdint.h> 
#include <stdbool.h> 
#include <errno.h>
#include <math.h>

#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#include "openprims.h"
#include "bpxnet.h"
#include "unixfile.h"
#include "charsets.h"
#include "collections.h"
#include "json.h"
#include "embeddedjs.h"
#include "configmgr.h"

/*

    ## wherever you git cloned quickjs-portable
    set QJS=c:\repos\quickjs-portable
      
    ## Wherever you git clone'd libyaml
    set YAML=c:\repos\libyaml

    clang++ -c ../platform/windows/cppregex.cpp ../platform/windows/winregex.cpp

    clang -I %QJS%\porting -I%YAML%/include -I %QJS% -I./src -I../h -I ../platform/windows -DCONFIG_VERSION=\"2021-03-27\" -Dstrdup=_strdup -D_CRT_SECURE_NO_WARNINGS -DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_DECLARE_STATIC=1 --rtlib=compiler-rt -o qjstest.exe qjstest.c ../c/embeddedjs.c %QJS%\quickjs.c %QJS%\cutils.c %QJS%\quickjs-libc.c %QJS%\libbf.c %QJS%\libregexp.c %QJS%\libunicode.c %QJS%\porting\winpthread.c %QJS%\porting\wintime.c %QJS%\porting\windirent.c %QJS%\porting\winunistd.c %YAML%/src/api.c %YAML%/src/reader.c %YAML%/src/scanner.c %YAML%/src/parser.c %YAML%/src/loader.c %YAML%/src/writer.c %YAML%/src/emitter.c %YAML%/src/dumper.c ../c/yaml2json.c ../c/microjq.c ../c/parsetools.c ../c/jsonschema.c ../c/json.c ../c/xlate.c ../c/charsets.c ../c/winskt.c ../c/logging.c ../c/collections.c ../c/timeutls.c ../c/utils.c ../c/alloc.c ../platform/windows/winfile.c cppregex.o winregex.o

  
 */



/*
  Notes from QuickJS Doc

  C opaque data can be attached to a Javascript object. The type of
  the C opaque data is determined with the class ID (JSClassID) of the
  object. Hence the first step is to register a new class ID and JS
  class (JS_NewClassID(), JS_NewClass()). Then you can create objects
  of this class with JS_NewObjectClass() and get or set the C opaque
  point with JS_GetOpaque()/JS_SetOpaque().

  When defining a new JS class, it is possible to declare a finalizer
  which is called when the object is destroyed. The finalizer should
  be used to release C resources. It is invalid to execute JS code
  from it. A gc_mark method can be provided so that the cycle removal
  algorithm can find the other objects referenced by this
  object. Other methods are available to define exotic object
  behaviors.

  The Class ID are globally allocated (i.e. for all runtimes). The
  JSClass are allocated per JSRuntime. JS_SetClassProto() is used to
  define a prototype for a given class in a given
  JSContext. JS_NewObjectClass() sets this prototype in the created
  object.

  Examples are available in quickjs-libc.c. 
 */

typedef struct Person_tag {
  const char *name;
  int   age;
  int   shoeSize;
} Person;

int personHaveBirthday(Person *p, EJSNativeInvocation *invocation){
  p->age++;
  ejsReturnInt(invocation,p->age);
  return EJS_OK;
}

int personAge(Person *p, EJSNativeInvocation *invocation){
  ejsReturnInt(invocation,p->age);
  return EJS_OK;
}

int personSetShoeSize(Person *p, EJSNativeInvocation *invocation){
  ejsIntArg(invocation,0,&p->shoeSize);
  return EJS_OK;
}

int personGetShoeSize(Person *p, EJSNativeInvocation *invocation){
  ejsReturnInt(invocation,p->shoeSize);
  return EJS_OK;
}

int personSetName(Person *p, EJSNativeInvocation *invocation){
  ejsStringArg(invocation,0,&p->name); /* should this copy */
  return EJS_OK;
}

int personGetName(Person *p, EJSNativeInvocation *invocation){
  ejsReturnString(invocation,p->name,false);
  return EJS_OK;
}

void *makePerson(void *userData, /* of EJSNativeClass */
                 EJSNativeInvocation *invocation){
  Person *p = (Person*)safeMalloc(sizeof(Person),"Person");
  p->name = "Fred";
  p->age = 20;
  p->shoeSize = 10;
  return p;
}

void freePerson(EmbeddedJS *ejs, void *userData, void *nativePerson){
  safeFree((char*)nativePerson,sizeof(Person));
}

static EJSNativeModule *makeFFITestData(EmbeddedJS *ejs){
  EJSNativeModule *module = ejsMakeNativeModule(ejs,"FFI1");
  EJSNativeClass *person = ejsMakeNativeClass(ejs,module,"Person",makePerson,freePerson,NULL);
  EJSNativeMethod *haveBirthday = ejsMakeNativeMethod(ejs,person,"haveBirthday",
                                                      EJS_NATIVE_TYPE_INT32,
                                                      (EJSForeignFunction*)personHaveBirthday);
  EJSNativeMethod *age = ejsMakeNativeMethod(ejs,person,"age",
                                             EJS_NATIVE_TYPE_INT32,
                                             (EJSForeignFunction*)personAge);
  EJSNativeMethod *name = ejsMakeNativeMethod(ejs,person,"name",
                                              EJS_NATIVE_TYPE_CONST_STRING,
                                              (EJSForeignFunction*)personGetName);
  EJSNativeMethod *setShoeSize = ejsMakeNativeMethod(ejs,person,"setShoeSize",
                                                     EJS_NATIVE_TYPE_VOID,
                                                     (EJSForeignFunction*)personSetShoeSize);
  ejsAddMethodArg(ejs,setShoeSize,"newSize",EJS_NATIVE_TYPE_INT32);
  EJSNativeMethod *getShoeSize = ejsMakeNativeMethod(ejs,person,"getShoeSize",
                                                     EJS_NATIVE_TYPE_INT32,
                                                     (EJSForeignFunction*)personGetShoeSize);
  
  return module;
}

int main(int argc, char **argv){
  printf("qjstest start argc=%d\n",argc);fflush(stdout);
  if (argc < 2) {
    printf("please supply a js filename to evaluate\n");
    return 0;
  }
  char *filename = argv[1];
  EJSNativeModule *modules[1];
  EmbeddedJS *ejs = allocateEmbeddedJS(NULL);
  modules[0] = makeFFITestData(ejs);
  configureEmbeddedJS(ejs,modules,1,argc,argv);

  int evalStatus = ejsEvalFile(ejs,filename,EJS_LOAD_IS_MODULE);
  printf("File eval returns %d\n",evalStatus);
  return 0;
}

