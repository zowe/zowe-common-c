

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DATASERVICE__
#define __DATASERVICE__ 1

/** \file
 *  \brief dataservice.h defines the in-memory representation of a loaded VirtualDesktop plugin.
 *
 *
 */


typedef int ExternalAPI();

struct JsonObject_tag;
struct HttpServer_tag;
struct DataService_tag;

typedef struct InternalAPIMap_tag {
  int length;
  char **names;
  ExternalAPI **entryPoints;
} InternalAPIMap;

typedef int InitializerAPI(struct DataService_tag *dataService, struct HttpServer_tag *server);

int pluginTypeFromString(const char*);
const char* pluginTypeString(int);

#define WEB_PLUGIN_TYPE_APPLICATION      0
#define WEB_PLUGIN_TYPE_LIBRARY          1
#define WEB_PLUGIN_TYPE_WINDOW_MANAGER   2
#define WEB_PLUGIN_TYPE_UNKNOWN        (-1)

typedef struct WebPlugin_tag {
  char *identifier;
  char *baseURI;
  char *pluginLocation;
  int pluginType;

  struct JsonObject_tag *pluginDefinition;
  int dataServiceCount;
  struct DataService_tag **dataServices;
} WebPlugin;

typedef struct DataService_tag {
  char *identifier; /* use java-style */
  char *name;
  char *subURI; /* only used for groups */
  char *pattern;
  int   (*initializer)(struct DataService_tag *dataService, struct HttpServer_tag *server);
  int   runInRequestorUID;  /* or an enumeration of security styles */
  /* static content root */
  ExternalAPI *externalAPI;  /* a function call to some lower level module to get data */
  void        *extension;    /* another slot to stash things */
  JsonObject *serviceDefinition;
  WebPlugin *plugin;
} DataService;

WebPlugin *makeWebPlugin(char *baseDir, struct JsonObject_tag *pluginDefintion, InternalAPIMap *internalAPIMap);
void initalizeWebPlugin(WebPlugin *plugin, HttpServer *server);

/**
   \brief Creates an http service ready from a DataService

   This function is used after a plugin is brought into memory as a DataService.  It is usually called by a plugin 
   initializer.

   Plugin Initializers  have the prototype <initializerName>(DataService *,HttpServer *) and called from the internals
   of the MVD server.
 */

HttpService *makeHttpDataService(DataService *dataService, HttpServer *server);
int makeHttpDataServiceUrlMask(DataService *dataService, char *urlMaskBuffer, int urlMaskBufferSize, char *productPrefix);



#endif /* __DATASERVICE__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

