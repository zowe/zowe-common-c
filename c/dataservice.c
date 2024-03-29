

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
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>  
#include <pthread.h>
#include <inttypes.h>
#define __SUSV3 1 /* This heinous hack is needed because dlfcn */
#include <dlfcn.h>
#undef __SUSV3 
#endif

#include "logging.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "le.h"
#include "json.h"
#include "httpserver.h"
#include "storage.h"
#include "dataservice.h"

#define SERVICE_TYPE_SERVICE 1
#define SERVICE_TYPE_GROUP 2
#define SERVICE_TYPE_IMPORT 3

typedef struct {
  const char* pluginString;
  int pluginType;
} PlugInMapEntry;

static PlugInMapEntry plugInMap[] = {
  {"library", WEB_PLUGIN_TYPE_LIBRARY},
  {"application", WEB_PLUGIN_TYPE_APPLICATION},
  {"library", WEB_PLUGIN_TYPE_LIBRARY},
  {"windowManager", WEB_PLUGIN_TYPE_WINDOW_MANAGER},
  {"unknown", WEB_PLUGIN_TYPE_UNKNOWN},
  {0, 0}
};

int pluginTypeFromString(const char*pluginType)
{
  PlugInMapEntry *e = plugInMap;
  while (e->pluginString != 0) {
    if (0 == strcasecmp(pluginType, e->pluginString)) {
      return e->pluginType;
    }
    ++e;
  }
  return WEB_PLUGIN_TYPE_UNKNOWN;
}

const char* pluginTypeString(int pluginType)
{
  PlugInMapEntry *e = plugInMap;
  while (e->pluginString != 0) {
    // this seems like an error, but it appears vestigial
    if (pluginType = e->pluginType) {
      return e->pluginString;
    }
    ++e;
  }
  return pluginTypeString(WEB_PLUGIN_TYPE_UNKNOWN);
}

static ExternalAPI *lookupAPI(InternalAPIMap *namedAPIMap, char *entryPointName){
  int i;
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "lookup count=%d name=%s\n", namedAPIMap->length, entryPointName);
  for (i=0; i<namedAPIMap->length; i++){
    if (!strcmp(namedAPIMap->names[i],entryPointName)){
      return namedAPIMap->entryPoints[i];
    }
  }
  return NULL;
}


static void *lookupDLLEntryPoint(char *libraryName, char *functionName){
  void* ep = NULL;
#ifndef METTLE
  int flags = RTLD_GLOBAL
#ifdef __ZOWE_OS_LINUX
    | RTLD_NOW
#endif
    ;
  FileInfo info;
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;
  status = fileInfo(libraryName, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (!(info.attributeFlags & BPXYSTAT_ATTR_PROGCTL)) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING,
              "FAILURE: Dataservice: %s does not have the Program Control attribute this may cause unexpected errors therefore will not be loaded\n",
              libraryName);
    } else {
      void *library = dlopen(libraryName,flags);
      if (library){
        zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "dll handle for %s = 0x%" PRIxPTR "\n", libraryName, library);
        ep = dlsym(library,functionName);
        /* dlclose(library); - this will really close the DLL, like not be able to call */
        if (ep == NULL){
          zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "%s.%s could not be found -  dlsym error %s\n", libraryName, functionName, dlerror());
        } else {
          zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "%s.%s is at 0x%" PRIxPTR "\n", libraryName, functionName, ep);
          zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Dataservice: %s loaded.\n", libraryName);
        }
      } else {
        zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "dlopen error for %s - %s\n",libraryName, dlerror());
      }
    }       
  } else {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "FAILURE: Dataservice: %s status not available therefore will not be loaded, please check the provided file name: (return = 0x%x, reason = 0x%x)\n",
            libraryName, returnCode, reasonCode);
  }
#endif /* not METTLE */
  return ep;
}

#define LIBRARY_NAME_KEY           "libraryName"
#define INITIALIZER_NAME_KEY       "initializerName"
#ifdef _LP64
#define LIBRARY_NAME_WITH_BITS_KEY "libraryName64"
#else
#define LIBRARY_NAME_WITH_BITS_KEY "libraryName31"
#endif

static InitializerAPI *loadDll(DataService *service, JsonObject *serviceJsonObject, const char *pluginLocation) {
  char *libraryNameWithBits = jsonObjectGetString(serviceJsonObject, LIBRARY_NAME_WITH_BITS_KEY);
  char *libraryName = jsonObjectGetString(serviceJsonObject, LIBRARY_NAME_KEY);
  char *initializerName = jsonObjectGetString(serviceJsonObject, INITIALIZER_NAME_KEY);
  char dllLocation[COMMON_PATH_MAX] = {0};
  int pluginLocationLen = strlen(pluginLocation);
  InitializerAPI *initializer = NULL;

  /* "external will load a DLL in LE on the path, and a load module in the STEPLIB in METAL */
#ifdef METTLE
  /* do a load of 8 char name.  Pray that the STEPLIB yields something good. */
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_SEVERE, "*** PANIC ***  service loading in METTLE not implemented\n");
#else
  if (pluginLocationLen > 0 && pluginLocation[pluginLocationLen - 1] == '/') {
    pluginLocationLen--; /* exclude trailing slash */
  }

  do {
    if (!libraryNameWithBits && !libraryName) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING, "Dataservice: couldn't find '%s' or '%s' key for dataservice '%s'\n",
        LIBRARY_NAME_WITH_BITS_KEY, LIBRARY_NAME_KEY, service->identifier);
      break;
    }
    if (!initializerName) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING, "Dataservice: couldn't find '%s' key for dataservice '%s'\n",
        INITIALIZER_NAME_KEY, service->identifier);
      break;
    }
    if (libraryNameWithBits) {
      snprintf(dllLocation, sizeof(dllLocation), "%.*s/lib/%s", pluginLocationLen, pluginLocation, libraryNameWithBits);
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "going for DLL EP lib=%s epName=%s\n", dllLocation, initializerName);
      if (initializer = (InitializerAPI*) lookupDLLEntryPoint(dllLocation, initializerName)) {
        break;
      }
    }
    if (libraryName) {
      snprintf(dllLocation, sizeof(dllLocation), "%.*s/lib/%s", pluginLocationLen, pluginLocation, libraryName);
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "going for DLL EP lib=%s epName=%s\n", dllLocation, initializerName);
      if (initializer = (InitializerAPI*) lookupDLLEntryPoint(dllLocation, initializerName)) {
        break;
      }
    }
    if (libraryNameWithBits) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "going for DLL EP lib=%s epName=%s\n", libraryNameWithBits, initializerName);
      if (initializer = (InitializerAPI*) lookupDLLEntryPoint(libraryNameWithBits, initializerName)) {
        break;
      }
    }
    if (libraryName) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "going for DLL EP lib=%s epName=%s\n", libraryName, initializerName);
      initializer = (InitializerAPI*) lookupDLLEntryPoint(libraryName, initializerName);
    }
    if (!initializer) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING, "Dataservice: couldn't load dll for dataservice '%s'\n", service->identifier);
    }
  } while(0);
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "DLL EP = 0x%x\n", initializer);
#endif
  return initializer;
}

static DataService *makeDataService(WebPlugin *plugin, JsonObject *serviceJsonObject, char *subURI, InternalAPIMap *namedAPIMap,
                                    unsigned int *idMultiplier, int pluginLogLevel, Storage *remoteStorage) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "%s begin data service:\n", __FUNCTION__);
  DataService *service = (DataService*) safeMalloc(sizeof (DataService), "DataService");
  memset(service, 0, sizeof (DataService));
  service->plugin = plugin;
  service->serviceDefinition = serviceJsonObject;
  service->pattern = jsonObjectGetString(serviceJsonObject, "pattern");
  service->subURI = subURI;
  service->remoteStorage = remoteStorage;
  service->localStorage = makeMemoryStorage(NULL);
  if (subURI) { /* group member */
    service->name = jsonObjectGetString(serviceJsonObject, "subserviceName");
  } else {
    service->name = jsonObjectGetString(serviceJsonObject, "name");
  }
  service->identifier = safeMalloc(strlen(plugin->identifier) + 1
                                   + (service->subURI ? strlen(service->subURI) + 1 : 0)
                                   + (service->name ? strlen(service->name) : 0) + 1, "service identifier name");
  if (service->subURI && service->name) {
    sprintf(service->identifier, "%s/%s/%s", plugin->identifier, service->subURI, service->name);
  } else if (service->subURI) {
    sprintf(service->identifier, "%s/%s", plugin->identifier, service->subURI);
  } else if (service->name) {
    sprintf(service->identifier, "%s/%s", plugin->identifier, service->name);
  } else {
    sprintf(service->identifier, "%s", plugin->identifier);
  }
  
  service->loggingIdentifier = LOG_PROD_PLUGINS + (0x10000 * (*idMultiplier));
  logConfigureComponent(NULL, service->loggingIdentifier, service->identifier,
                        LOG_DEST_PRINTF_STDOUT, pluginLogLevel);
  zowelog(NULL, service->loggingIdentifier, ZOWE_LOG_DEBUG, "added identifier for %s\n", service->identifier);

  char *initializerLookupMethod = jsonObjectGetString(serviceJsonObject, "initializerLookupMethod");
  char *initializerName = jsonObjectGetString(serviceJsonObject, "initializerName");
  if (!strcmp(initializerLookupMethod, "internal")) {
    service->initializer = (InitializerAPI*) lookupAPI(namedAPIMap, initializerName);
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "Service=%s initializer set internal method=0x%p\n",
            service->identifier, service->name, service->initializer);
  } else if (!strcmp(initializerLookupMethod,"external")){
    service->initializer = loadDll(service, serviceJsonObject, plugin->pluginLocation);
  } else {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "unknown plugin initialize lookup method %s\n", initializerLookupMethod);
  }
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "%s end\n", __FUNCTION__);
  return service;
}

void initalizeWebPlugin(WebPlugin *plugin, HttpServer *server) {
  int i = 0;
  int count = plugin->dataServiceCount;
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2, "%s begin, count=%d\n", __FUNCTION__,count);  
  for (i = 0; i < count; i++) {
    DataService *service = plugin->dataServices[i];
    if (service->initializer){
      service->initializer(service, server);
    } else{
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "*** PANIC *** service %s has no installer\n", service->identifier);
    }
  }
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
}

static void freeWebPlugin(WebPlugin *plugin) {
  safeFree((char*)plugin, sizeof(WebPlugin));
}

static bool isValidServiceDef(JsonObject *serviceDef) {
  char *type = jsonObjectGetString(serviceDef, "type");
  char *serviceName = jsonObjectGetString(serviceDef, "name");
  char *sourceName = jsonObjectGetString(serviceDef, "sourceName");
  bool isImport = false;
  if (type) {
    isImport = strcmp(type, "import") ? false : true;
    if (!isImport && !serviceName) {
      // Return a null plugin when 'name' is not set for dataservices of types: router or service or modeService or external.
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "** PANIC: Missing 'name' fields for dataservices. **\n");
      return false;
    } else if (isImport && !sourceName) {
      // Return a null plugin when 'sourceName' is not set for dataservices of type: import.
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "** PANIC: Missing 'sourceName' fields for dataservices of type 'import'. **\n");
      return false;
    }else{
      // Add more validations if any.
    }
  } else {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "** PANIC: Check pluginDefinition for correct 'type' fields on dataservices.** \n");
    return false;
  }
  return true;
}

WebPlugin *makeWebPlugin2(char *pluginLocation, JsonObject *pluginDefintion, InternalAPIMap *internalAPIMap,
                         unsigned int *idMultiplier, int pluginLogLevel, Storage *remoteStorage) {
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);
  WebPlugin *plugin = (WebPlugin*)safeMalloc(sizeof(WebPlugin),"WebPlugin");
  memset(plugin, 0, sizeof (WebPlugin));
  plugin->pluginLocation = pluginLocation;
  plugin->identifier =  jsonObjectGetString(pluginDefintion, "identifier");
  plugin->baseURI = jsonObjectGetString(pluginDefintion, "baseURI");
  char *pluginType = jsonObjectGetString(pluginDefintion, "pluginType");
  if (pluginType) {
    plugin->pluginType = pluginTypeFromString(pluginType);
  } else {
    plugin->pluginType = WEB_PLUGIN_TYPE_APPLICATION;
  }
  plugin->pluginDefinition = pluginDefintion;
  JsonArray *dataServices = jsonObjectGetArray(pluginDefintion, "dataServices");

  if (dataServices && jsonArrayGetCount(dataServices) > 0) {
    /* count data services */
    plugin->dataServiceCount = 0;
    for (int i = 0; i < jsonArrayGetCount(dataServices); i ++) {
      JsonObject *serviceDef = jsonArrayGetObject(dataServices, i);
      char *type = jsonObjectGetString(serviceDef, "type");
      if (!isValidServiceDef(serviceDef)){
        freeWebPlugin(plugin);
        plugin = NULL;
        return NULL;
      }

      if (!strcmp(type, "service")){
        plugin->dataServiceCount ++;
      } else if (!strcmp(type, "group")) {
        JsonArray* group = jsonObjectGetArray(serviceDef, "subservices");
        if (group) {
          plugin->dataServiceCount += jsonArrayGetCount(group);
        }
      }
    }

    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Plugin=%s, found %d data service(s)\n", plugin->identifier, plugin->dataServiceCount);
    if (plugin->dataServiceCount > 0) {
      plugin->dataServices = (DataService**)safeMalloc(sizeof(DataService*) * plugin->dataServiceCount,"DataServices");
    } else {
      plugin->dataServices = NULL;
    }
    int k = 0;
    for (int i = 0; i < jsonArrayGetCount(dataServices); i++) {
      JsonObject *serviceDef = jsonArrayGetObject(dataServices, i);
      char *type = jsonObjectGetString(serviceDef, "type");

      if (!type || !strcmp(type, "service")) {
        plugin->dataServices[k++] = makeDataService(plugin, jsonArrayGetObject(dataServices, i), NULL, internalAPIMap,
                                                    idMultiplier, pluginLogLevel, remoteStorage);
        (*idMultiplier++);
      } else if (!strcmp(type, "group")) {
        char *subURI = jsonObjectGetString(serviceDef, "name");
        JsonArray* group = jsonObjectGetArray(serviceDef, "subservices");
        if (group) {
          for (int j = 0; j < jsonArrayGetCount(group); j++) {
            plugin->dataServices[k++] = makeDataService(plugin, jsonArrayGetObject(group, j), subURI, internalAPIMap,
                                                        idMultiplier, pluginLogLevel, remoteStorage);
            (*idMultiplier++);
          }
        }
      } else {
        /* import, ignore */
      }
    }
  }
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
  return plugin;
}

WebPlugin *makeWebPlugin(char *pluginLocation, JsonObject *pluginDefintion, InternalAPIMap *internalAPIMap,
                         unsigned int *idMultiplier, int pluginLogLevel) {
  return makeWebPlugin2(pluginLocation, pluginDefintion, internalAPIMap, idMultiplier, pluginLogLevel, NULL);
}

HttpService *makeHttpDataService(DataService *dataService, HttpServer *server) {
  char urlMask[512] = {0};
  int result;
  HttpService *httpService = NULL;
  result = makeHttpDataServiceUrlMask(dataService, urlMask, sizeof(urlMask), server->defaultProductURLPrefix);
  if (result != -1) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Installing dataservice %s to URI %s\n", dataService->identifier, urlMask);
    httpService = makeGeneratedService(dataService->identifier, urlMask);
    httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN; /* The default */
    registerHttpService(server, httpService);
    httpService->userPointer = dataService;
  }
  return httpService;
}

int makeHttpDataServiceUrlMask(DataService *dataService, char *urlMaskBuffer, int urlMaskBufferSize, char *productPrefix) {
  WebPlugin *plugin = dataService->plugin;
  if (productPrefix) {
    if (dataService->subURI) {
      snprintf(urlMaskBuffer, urlMaskBufferSize, "/%s/plugins/%s/services/%s/%s", productPrefix, plugin->identifier,
               dataService->subURI, dataService->pattern);
    } else if (dataService->name) {
      snprintf(urlMaskBuffer, urlMaskBufferSize, "/%s/plugins/%s/services/%s/**", productPrefix, plugin->identifier,
               dataService->name);
    } else {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "** PANIC: Data service has no name, group name, or pattern **\n");
      return -1;
    }
  } else {
    if (dataService->subURI) {
      snprintf(urlMaskBuffer, urlMaskBufferSize, "/plugins/%s/services/%s/%s", plugin->identifier,
               dataService->subURI, dataService->pattern);
    } else if (dataService->name) {
      snprintf(urlMaskBuffer, urlMaskBufferSize, "/plugins/%s/services/%s/**", plugin->identifier,
               dataService->name);
    } else {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG, "** PANIC: Data service has no name, group name, or pattern **\n");
      return -1;
    }
  }
  return 0;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

