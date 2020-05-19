

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef MVD_H_ZCCLOGGING_H_
#define MVD_H_ZCCLOGGING_H_

#ifndef ZCC_LOG_ID
#define ZCC_LOG_ID     "ZWE"
#endif

#ifndef ZCC_LOG_SUBCOMP_ID
#define ZCC_LOG_SUBCOMP_ID  "S"
#endif

#ifndef ZCC_LOG_PROD_ID
#define ZCC_LOG_PROD_ID ZCC_LOG_ID ZCC_LOG_SUBCOMP_ID
#endif

#ifndef ZCC_LOG_MSG_SUBCOMP
#define ZCC_LOG_MSG_SUBCOMP ZCC_LOG_SUBCOMP_ID
#endif

#ifndef ZCC_LOG_MSG_PRFX
#define ZCC_LOG_MSG_PRFX  ZCC_LOG_ID ZCC_LOG_MSG_SUBCOMP
#endif


bool isLogLevelValid(int level);

/* default message IDs */
/* 2000 - 2099 Data Service */
/* 2100 - 2199 Dataset JSON */
/* 2200 - 2249 HTTP File Service*/
/* 2250 - 2299 HTTP Server */
/* 2300 - 2349 jcsi */
/* 2350 - 2399 JSON */
/* 2400 - 2449 MTLSKT */
/* 2450 - 2499 PDS UTIL */
/* 2500 - 2549 Recovery */
/* 2550 - 2599 stcbase */
/* 2600 - 2649 UTILS */
/* 2650 - 2699 XML */
/* 2700 - 2799 ZOS ACCOUNTS */
/* 2800 - 2899 ZOS FILE */


#ifndef ZCC_LOG_DATA_LOOK_UP_API_ID
#define ZCC_LOG_DATA_LOOK_UP_API_ID                 ZCC_LOG_MSG_PRFX"2000I"
#endif
#define ZCC_LOG_LOOK_UP_API_TEXT                    "lookup count=%d name=%s\n"
#define ZCC_LOG_LOOK_UP_API                         ZCC_LOG_DATA_LOOK_UP_API_ID" "ZCC_LOG_LOOK_UP_API_TEXT

#ifndef ZCC_LOG_DATA_PRGM_CNTRL_ERR_ID
#define ZCC_LOG_DATA_PRGM_CNTRL_ERR_ID              ZCC_LOG_MSG_PRFX"2001S"
#endif
#define ZCC_LOG_DATA_PRGM_CNTRL_ERR_TEXT            "FAILURE: Dataservice: %s does not have the Program Control attribute this may cause unexpected errors therefore will not be loaded\n"
#define ZCC_LOG_DATA_PRGM_CNTRL_ERR                 ZCC_LOG_DATA_PRGM_CNTRL_ERR_ID" "ZCC_LOG_DATA_PRGM_CNTRL_ERR_TEXT

#ifndef ZCC_LOG_DLL_LOAD_MSG_ID
#define ZCC_LOG_DLL_LOAD_MSG_ID                     ZCC_LOG_MSG_PRFX"2002I"
#endif
#define ZCC_LOG_DLL_LOAD_MSG_TEXT                   "dll handle for %s = 0x%" PRIxPTR "\n"
#define ZCC_LOG_DLL_LOAD_MSG                        ZCC_LOG_DLL_LOAD_MSG_ID" "ZCC_LOG_DLL_LOAD_MSG_TEXT

#ifndef ZCC_LOG_DLSYM_ERR_ID
#define ZCC_LOG_DLSYM_ERR_ID                        ZCC_LOG_MSG_PRFX"2003S"
#endif
#define ZCC_LOG_DLSYM_ERR_TEXT                      "%s.%s could not be found -  dlsym error %s\n"
#define ZCC_LOG_DLSYM_ERR                           ZCC_LOG_DLSYM_ERR_ID" "ZCC_LOG_DLSYM_ERR_TEXT

#ifndef ZCC_LOG_DLSYM_MSG_ID
#define ZCC_LOG_DLSYM_MSG_ID                        ZCC_LOG_MSG_PRFX"2004I"
#endif
#define ZCC_LOG_DLSYM_MSG_TEXT                      "%s.%s is at 0x%" PRIxPTR "\n"
#define ZCC_LOG_DLSYM_MSG                           ZCC_LOG_DLSYM_MSG_ID" "ZCC_LOG_DLSYM_MSG_TEXT

#ifndef ZCC_LOG_DLOPEN_ERR_ID
#define ZCC_LOG_DLOPEN_ERR_ID                       ZCC_LOG_MSG_PRFX"2005S"
#endif
#define ZCC_LOG_DLOPEN_ERR_TEXT                     "dlopen error for %s - %s\n"
#define ZCC_LOG_DLOPEN_ERR                          ZCC_LOG_DLOPEN_ERR_ID" "ZCC_LOG_DLOPEN_ERR_TEXT

#ifndef ZCC_LOG_DLL_FILE_ERR_ID
#define ZCC_LOG_DLL_FILE_ERR_ID                     ZCC_LOG_MSG_PRFX"2006S"
#endif
#define ZCC_LOG_DLL_FILE_ERR_TEXT                   "FAILURE: Dataservice: %s status not available therefore will not be loaded, please check the provided file name: (return = 0x%x, reason = 0x%x)\n"
#define ZCC_LOG_DLL_FILE_ERR                        ZCC_LOG_DLL_FILE_ERR_ID" "ZCC_LOG_DLL_FILE_ERR_TEXT

#ifndef ZCC_LOG_DATA_SERVICE_ID_MSG_ID
#define ZCC_LOG_DATA_SERVICE_ID_MSG_ID              ZCC_LOG_MSG_PRFX"2007W"
#endif
#define ZCC_LOG_DATA_SERVICE_ID_MSG_TEXT            "added identifier for %s\n"
#define ZCC_LOG_DATA_SERVICE_ID_MSG                 ZCC_LOG_DATA_SERVICE_ID_MSG_ID" "ZCC_LOG_DATA_SERVICE_ID_MSG_TEXT

#ifndef ZCC_LOG_DATA_SERVICE_INIT_MSG_ID
#define ZCC_LOG_DATA_SERVICE_INIT_MSG_ID            ZCC_LOG_MSG_PRFX"2008I"
#endif
#define ZCC_LOG_DATA_SERVICE_INIT_MSG_TEXT          "Service=%s initializer set internal method=0x%p\n"
#define ZCC_LOG_DATA_SERVICE_INIT_MSG               ZCC_LOG_DATA_SERVICE_INIT_MSG_ID" "ZCC_LOG_DATA_SERVICE_INIT_MSG_TEXT

#ifndef ZCC_LOG_DATA_SERVICE_METTLE_ERR_ID
#define ZCC_LOG_DATA_SERVICE_METTLE_ERR_ID          ZCC_LOG_MSG_PRFX"2009W"
#endif
#define ZCC_LOG_DATA_SERVICE_METTLE_ERR_TEXT        "*** PANIC ***  service loading in METTLE not implemented\n"
#define ZCC_LOG_DATA_SERVICE_METTLE_ERR             ZCC_LOG_DATA_SERVICE_METTLE_ERR_ID" "ZCC_LOG_DATA_SERVICE_METTLE_ERR_TEXT

#ifndef ZCC_LOG_DLL_EP_LOOK_UP_MSG_ID
#define ZCC_LOG_DLL_EP_LOOK_UP_MSG_ID               ZCC_LOG_MSG_PRFX"2010I"
#endif
#define ZCC_LOG_DLL_EP_LOOK_UP_MSG_TEXT             "going for DLL EP lib=%s epName=%s\n"
#define ZCC_LOG_DLL_EP_LOOK_UP_MSG                  ZCC_LOG_DLL_EP_LOOK_UP_MSG_ID" "ZCC_LOG_DLL_EP_LOOK_UP_MSG_TEXT

#ifndef ZCC_LOG_DLL_EP_MSG_ID
#define ZCC_LOG_DLL_EP_MSG_ID                       ZCC_LOG_MSG_PRFX"2011I"
#endif
#define ZCC_LOG_DLL_EP_MSG_TEXT                     "DLL EP = 0x%x\n"
#define ZCC_LOG_DLL_EP_MSG                          ZCC_LOG_DLL_EP_MSG_ID" "ZCC_LOG_DLL_EP_MSG_TEXT

#ifndef ZCC_LOG_LOOK_UP_METHOD_ERR_ID
#define ZCC_LOG_LOOK_UP_METHOD_ERR_ID               ZCC_LOG_MSG_PRFX"2012W"
#endif
#define ZCC_LOG_LOOK_UP_METHOD_ERR_TEXT             "unknown plugin initialize lookup method %s\n"
#define ZCC_LOG_LOOK_UP_METHOD_ERR                  ZCC_LOG_LOOK_UP_METHOD_ERR_ID" "ZCC_LOG_LOOK_UP_METHOD_ERR_TEXT

#ifndef ZCC_LOG_SERVICE_INSTALLER_ERR_ID
#define ZCC_LOG_SERVICE_INSTALLER_ERR_ID            ZCC_LOG_MSG_PRFX"2013W"
#endif
#define ZCC_LOG_SERVICE_INSTALLER_ERR_TEXT          "*** PANIC *** service %s has no installer\n"
#define ZCC_LOG_SERVICE_INSTALLER_ERR               ZCC_LOG_SERVICE_INSTALLER_ERR_ID" "ZCC_LOG_SERVICE_INSTALLER_ERR_TEXT

#ifndef ZCC_LOG_SERVICE_NAME_ERR_ID
#define ZCC_LOG_SERVICE_NAME_ERR_ID                 ZCC_LOG_MSG_PRFX"2014W"
#endif
#define ZCC_LOG_SERVICE_NAME_ERR_TEXT               "** PANIC: Missing 'name' fields for dataservices. **\n"
#define ZCC_LOG_SERVICE_NAME_ERR                    ZCC_LOG_SERVICE_NAME_ERR_ID" "ZCC_LOG_SERVICE_NAME_ERR_TEXT

#ifndef ZCC_LOG_SERVICE_SOURCE_NAME_ERR_ID
#define ZCC_LOG_SERVICE_SOURCE_NAME_ERR_ID          ZCC_LOG_MSG_PRFX"2015W"
#endif
#define ZCC_LOG_SERVICE_SOURCE_NAME_ERR_TEXT        "** PANIC: Missing 'sourceName' fields for dataservices of type 'import'. **\n"
#define ZCC_LOG_SERVICE_SOURCE_NAME_ERR             ZCC_LOG_SERVICE_SOURCE_NAME_ERR_ID" "ZCC_LOG_SERVICE_SOURCE_NAME_ERR_TEXT

#ifndef ZCC_LOG_SERVICE_TYPE_ERR_ID
#define ZCC_LOG_SERVICE_TYPE_ERR_ID                 ZCC_LOG_MSG_PRFX"2016W"
#endif
#define ZCC_LOG_SERVICE_TYPE_ERR_TEXT               "ZIS status - '%s' (name='%.16s', cmsRC='%d', description='%s', clientVersion='%d')\n"
#define ZCC_LOG_SERVICE_TYPE_ERR                    ZCC_LOG_SERVICE_TYPE_ERR_ID" "ZCC_LOG_SERVICE_TYPE_ERR_TEXT

#ifndef ZCC_LOG_PLUGIN_DATA_SERVICE_MSG_ID
#define ZCC_LOG_PLUGIN_DATA_SERVICE_MSG_ID          ZCC_LOG_MSG_PRFX"2017I"
#endif
#define ZCC_LOG_PLUGIN_DATA_SERVICE_MSG_TEXT        "Plugin=%s, found %d data service(s)\n"
#define ZCC_LOG_PLUGIN_DATA_SERVICE_MSG             ZCC_LOG_PLUGIN_DATA_SERVICE_MSG_ID" "ZCC_LOG_PLUGIN_DATA_SERVICE_MSG_TEXT

#ifndef ZCC_LOG_DATA_SERVICE_URI_MSG_ID
#define ZCC_LOG_DATA_SERVICE_URI_MSG_ID             ZCC_LOG_MSG_PRFX"2018I"
#endif
#define ZCC_LOG_DATA_SERVICE_URI_MSG_TEXT           "Installing dataservice %s to URI %s\n"
#define ZCC_LOG_DATA_SERVICE_URI_MSG                ZCC_LOG_DATA_SERVICE_URI_MSG_ID" "ZCC_LOG_DATA_SERVICE_URI_MSG_TEXT

#ifndef ZCC_LOG_DATA_SERVICE_URL_ERR_ID
#define ZCC_LOG_DATA_SERVICE_URL_ERR_ID             ZCC_LOG_MSG_PRFX"2019I"
#endif
#define ZCC_LOG_DATA_SERVICE_URL_ERR_TEXT           "** PANIC: Data service has no name, group name, or pattern **\n"
#define ZCC_LOG_DATA_SERVICE_URL_ERR                ZCC_LOG_DATA_SERVICE_URL_ERR_ID" "ZCC_LOG_DATA_SERVICE_URL_ERR_TEXT

/* Dataset JSON */

#ifndef ZCC_LOG_DATA_SET_READ_ERR_ID
#define ZCC_LOG_DATA_SET_READ_ERR_ID                ZCC_LOG_MSG_PRFX"2100S"
#endif
#define ZCC_LOG_DATA_SET_READ_ERR_TEXT              "Error reading DSN=%s, rc=%d\n"
#define ZCC_LOG_DATA_SET_READ_ERR                   ZCC_LOG_DATA_SET_READ_ERR_ID" "ZCC_LOG_DATA_SET_READ_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_OPEN_ERR_ID
#define ZCC_LOG_DATA_SET_OPEN_ERR_ID                ZCC_LOG_MSG_PRFX"2101S"
#endif
#define ZCC_LOG_DATA_SET_OPEN_ERR_TEXT              "FAILED TO OPEN FILE\n"
#define ZCC_LOG_DATA_SET_OPEN_ERR                   ZCC_LOG_DATA_SET_OPEN_ERR_ID" "ZCC_LOG_DATA_SET_OPEN_ERR_TEXT

#ifndef ZCC_LOG_ACB_REJECTED_ID
#define ZCC_LOG_ACB_REJECTED_ID                     ZCC_LOG_MSG_PRFX"2102S"
#endif
#define ZCC_LOG_ACB_REJECTED_TEXT                   "We were not passed an ACB.\n"
#define ZCC_LOG_ACB_REJECTED                        ZCC_LOG_ACB_REJECTED_ID" "ZCC_LOG_ACB_REJECTED_TEXT

#ifndef ZCC_LOG_CATALOG_MAX_SIZE_ERR_ID
#define ZCC_LOG_CATALOG_MAX_SIZE_ERR_ID             ZCC_LOG_MSG_PRFX"2103S"
#endif
#define ZCC_LOG_CATALOG_MAX_SIZE_ERR_TEXT           "CRITICAL ERROR: Catalog was wrong about maximum record size.\n"
#define ZCC_LOG_CATALOG_MAX_SIZE_ERR                ZCC_LOG_CATALOG_MAX_SIZE_ERR_ID" "ZCC_LOG_CATALOG_MAX_SIZE_ERR_TEXT

#ifndef ZCC_LOG_ACB_ERR_ID
#define ZCC_LOG_ACB_ERR_ID                          ZCC_LOG_MSG_PRFX"2104S"
#endif
#define ZCC_LOG_ACB_ERR_TEXT                        "Read 0 bytes with error: ACB=%08x, rc=%d, rplRC = %0x%06x\n"
#define ZCC_LOG_ACB_ERR                             ZCC_LOG_ACB_ERR_ID" "ZCC_LOG_ACB_ERR_TEXT

#ifndef ZCC_LOG_NEW_ACB_ERR_ID
#define ZCC_LOG_NEW_ACB_ERR_ID                      ZCC_LOG_MSG_PRFX"2104S"
#endif
#define ZCC_LOG_NEW_ACB_ERR_TEXT                    "New Error: ACB=%08x, rc=%d, rplRC = %02x%06x\n"
#define ZCC_LOG_NEW_ACB_ERR                         ZCC_LOG_NEW_ACB_ERR_ID" "ZCC_LOG_NEW_ACB_ERR_TEXT

#ifndef ZCC_LOG_RBA_EOF_ERR_ID
#define ZCC_LOG_RBA_EOF_ERR_ID                      ZCC_LOG_MSG_PRFX"2105S"
#endif
#define ZCC_LOG_RBA_EOF_ERR_TEXT                    "eof found after RBA %d\n"
#define ZCC_LOG_RBA_EOF_ERR                         ZCC_LOG_RBA_EOF_ERR_ID" "ZCC_LOG_RBA_EOF_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_PDS_ERR_ID
#define ZCC_LOG_DATA_SET_PDS_ERR_ID                 ZCC_LOG_MSG_PRFX"2106W"
#endif
#define ZCC_LOG_DATA_SET_PDS_ERR_TEXT               "Cannot print members. Selected dataset is not a PDS."
#define ZCC_LOG_DATA_SET_PDS_ERR                    ZCC_LOG_DATA_SET_PDS_ERR_ID" "ZCC_LOG_DATA_SET_PDS_ERR_TEXT

#ifndef ZCC_LOG_RECORD_LEN_MSG_ID
#define ZCC_LOG_RECORD_LEN_MSG_ID                   ZCC_LOG_MSG_PRFX"2107W"
#endif
#define ZCC_LOG_RECORD_LEN_MSG_TEXT                 "fallback for record length discovery\n"
#define ZCC_LOG_RECORD_LEN_MSG                      ZCC_LOG_RECORD_LEN_MSG_ID" "ZCC_LOG_RECORD_LEN_MSG_TEXT

#ifndef ZCC_LOG_RECORD_ERR_ID
#define ZCC_LOG_RECORD_ERR_ID                       ZCC_LOG_MSG_PRFX"2108W"
#endif
#define ZCC_LOG_RECORD_ERR_TEXT                     "Invalid record for dataset, recordLength=%d but max for dataset is %d\n"
#define ZCC_LOG_RECORD_ERR                          ZCC_LOG_RECORD_ERR_ID" "ZCC_LOG_RECORD_ERR_TEXT

#ifndef ZCC_LOG_UPDATE_RECORD_LEN_MSG_ID
#define ZCC_LOG_UPDATE_RECORD_LEN_MSG_ID            ZCC_LOG_MSG_PRFX"2109I"
#endif
#define ZCC_LOG_UPDATE_RECORD_LEN_MSG_TEXT          "UPDATE DATASET: record updated to have new length of %d which should match max length of %d, content:%s\n"
#define ZCC_LOG_UPDATE_RECORD_LEN_MSG               ZCC_LOG_UPDATE_RECORD_LEN_MSG_ID" "ZCC_LOG_UPDATE_RECORD_LEN_MSG_TEXT

#ifndef ZCC_LOG_ARRAY_FORMAT_ERR_ID
#define ZCC_LOG_ARRAY_FORMAT_ERR_ID                 ZCC_LOG_MSG_PRFX"2110W"
#endif
#define ZCC_LOG_ARRAY_FORMAT_ERR_TEXT               "Incorrectly formatted array!\n"
#define ZCC_LOG_ARRAY_FORMAT_ERR                    ZCC_LOG_UPDATE_RECORD_LEN_MSG_ID" "ZCC_LOG_UPDATE_RECORD_LEN_MSG_TEXT

#ifndef ZCC_LOG_DATA_SET_WRITE_ERR_ID
#define ZCC_LOG_DATA_SET_WRITE_ERR_ID               ZCC_LOG_MSG_PRFX"2111W"
#endif
#define ZCC_LOG_DATA_SET_WRITE_ERR_TEXT             "Error writing to dataset, rc=%d\n"
#define ZCC_LOG_DATA_SET_WRITE_ERR                  ZCC_LOG_DATA_SET_WRITE_ERR_ID" "ZCC_LOG_DATA_SET_WRITE_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_ALLOC_ERR_ID
#define ZCC_LOG_DATA_SET_ALLOC_ERR_ID               ZCC_LOG_MSG_PRFX"2112S"
#endif
#define ZCC_LOG_DATA_SET_ALLOC_ERR_TEXT             "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\', rc=%d sysRC=%d, sysRSN=0x%08X (%s)\n"
#define ZCC_LOG_DATA_SET_ALLOC_ERR                  ZCC_LOG_DATA_SET_ALLOC_ERR_ID" "ZCC_LOG_DATA_SET_ALLOC_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_DYNALLOC_ERR_ID
#define ZCC_LOG_DATA_SET_DYNALLOC_ERR_ID            ZCC_LOG_MSG_PRFX"2113S"
#endif
#define ZCC_LOG_DATA_SET_DYNALLOC_ERR_TEXT          "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\', rc=%d sysRC=%d, sysRSN=0x%08X (%s)\n"
#define ZCC_LOG_DATA_SET_DYNALLOC_ERR               ZCC_LOG_DATA_SET_DYNALLOC_ERR_ID" "ZCC_LOG_DATA_SET_DYNALLOC_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_WRITE_ERR2_ID
#define ZCC_LOG_DATA_SET_WRITE_ERR2_ID              ZCC_LOG_MSG_PRFX"2114W"
#endif
#define ZCC_LOG_DATA_SET_WRITE_ERR2_TEXT            "Error writing to dataset, rc=%02x%06x\n"
#define ZCC_LOG_DATA_SET_WRITE_ERR2                 ZCC_LOG_DATA_SET_WRITE_ERR_ID" "ZCC_LOG_DATA_SET_WRITE_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_JSON_ERR_ID
#define ZCC_LOG_DATA_SET_JSON_ERR_ID                ZCC_LOG_MSG_PRFX"2115W"
#endif
#define ZCC_LOG_DATA_SET_JSON_ERR_TEXT              "*** INTERNAL ERROR *** message is JSON, but not an object\n"
#define ZCC_LOG_DATA_SET_JSON_ERR                   ZCC_LOG_DATA_SET_JSON_ERR_ID" "ZCC_LOG_DATA_SET_JSON_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_BODY_ERR_ID
#define ZCC_LOG_DATA_SET_BODY_ERR_ID                ZCC_LOG_MSG_PRFX"2116W"
#endif
#define ZCC_LOG_DATA_SET_BODY_ERR_TEXT              "UPDATE DATASET: body was not JSON!\n"
#define ZCC_LOG_DATA_SET_BODY_ERR                   ZCC_LOG_DATA_SET_BODY_ERR_ID" "ZCC_LOG_DATA_SET_BODY_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_STOW_ERR_ID
#define ZCC_LOG_DATA_SET_STOW_ERR_ID                ZCC_LOG_MSG_PRFX"2117S"
#endif
#define ZCC_LOG_DATA_SET_STOW_ERR_TEXT              "error: stowReturnCode=%d, stowReasonCode=%d\n"
#define ZCC_LOG_DATA_SET_STOW_ERR                   ZCC_LOG_DATA_SET_STOW_ERR_ID" "ZCC_LOG_DATA_SET_STOW_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_NAME_ERR_ID
#define ZCC_LOG_DATA_SET_NAME_ERR_ID                ZCC_LOG_MSG_PRFX"2118S"
#endif
#define ZCC_LOG_DATA_SET_NAME_ERR_TEXT              "No entries for the dataset name found"
#define ZCC_LOG_DATA_SET_NAME_ERR                   ZCC_LOG_DATA_SET_NAME_ERR_ID" "ZCC_LOG_DATA_SET_NAME_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_MULTI_NAME_ERR_ID
#define ZCC_LOG_DATA_SET_MULTI_NAME_ERR_ID          ZCC_LOG_MSG_PRFX"2119S"
#endif
#define ZCC_LOG_DATA_SET_MULTI_NAME_ERR_TEXT        "More than one entry found for dataset name"
#define ZCC_LOG_DATA_SET_MULTI_NAME_ERR             ZCC_LOG_DATA_SET_MULTI_NAME_ERR_ID" "ZCC_LOG_DATA_SET_MULTI_NAME_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_NAME_SIZE_ERR_ID
#define ZCC_LOG_DATA_SET_NAME_SIZE_ERR_ID           ZCC_LOG_MSG_PRFX"2120S"
#endif
#define ZCC_LOG_DATA_SET_NAME_SIZE_ERR_TEXT         "DSN of size %d is too large.\n"
#define ZCC_LOG_DATA_SET_NAME_SIZE_ERR              ZCC_LOG_DATA_SET_NAME_SIZE_ERR_ID" "ZCC_LOG_DATA_SET_NAME_SIZE_ERR_TEXT

#ifndef ZCC_LOG_CATALOG_ENTRY_ERR_ID
#define ZCC_LOG_CATALOG_ENTRY_ERR_ID                ZCC_LOG_MSG_PRFX"2121W"
#endif
#define ZCC_LOG_CATALOG_ENTRY_ERR_TEXT              "Catalog Entry not found for \"%s\"\n"
#define ZCC_LOG_CATALOG_ENTRY_ERR                   ZCC_LOG_CATALOG_ENTRY_ERR_ID" "ZCC_LOG_CATALOG_ENTRY_ERR_TEXT

#ifndef ZCC_LOG_CATALOG_ENTRY_ERR_ID
#define ZCC_LOG_CATALOG_ENTRY_ERR_ID                ZCC_LOG_MSG_PRFX"2122W"
#endif
#define ZCC_LOG_CATALOG_ENTRY_ERR_TEXT              "Catalog Entry not found for \"%s\"\n"
#define ZCC_LOG_CATALOG_ENTRY_ERR                   ZCC_LOG_CATALOG_ENTRY_ERR_ID" "ZCC_LOG_CATALOG_ENTRY_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR_ID
#define ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR_ID         ZCC_LOG_MSG_PRFX"2123S"
#endif
#define ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR_TEXT       "Dynalloc RC = %d, reasonCode = %x\n"
#define ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR            ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR_ID" "ZCC_LOG_DATA_SET_DYNALLOC_RC_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_POINT_ERR_ID
#define ZCC_LOG_DATA_SET_POINT_ERR_ID               ZCC_LOG_MSG_PRFX"2124S"
#endif
#define ZCC_LOG_DATA_SET_POINT_ERR_TEXT             "point failed with RC = %08x\n"
#define ZCC_LOG_DATA_SET_POINT_ERR                  ZCC_LOG_DATA_SET_POINT_ERR_ID" "ZCC_LOG_DATA_SET_POINT_ERR_TEXT

#ifndef ZCC_LOG_DATA_SET_STREAM_MSG_ID
#define ZCC_LOG_DATA_SET_STREAM_MSG_ID              ZCC_LOG_MSG_PRFX"2125I"
#endif
#define ZCC_LOG_DATA_SET_STREAM_MSG_TEXT            "Streaming data for %s\n"
#define ZCC_LOG_DATA_SET_STREAM_MSG                 ZCC_LOG_DATA_SET_STREAM_MSG_ID" "ZCC_LOG_DATA_SET_STREAM_MSG_TEXT

#ifndef ZCC_LOG_PERCENT_DECODE_ERR_ID
#define ZCC_LOG_PERCENT_DECODE_ERR_ID               ZCC_LOG_MSG_PRFX"2126W"
#endif
#define ZCC_LOG_PERCENT_DECODE_ERR_TEXT             "Error: Percent seen without following 2 hex characters for '%s'\n"
#define ZCC_LOG_PERCENT_DECODE_ERR                  ZCC_LOG_PERCENT_DECODE_ERR_ID" "ZCC_LOG_PERCENT_DECODE_ERR_TEXT

/* HTTP File Service*/

#ifndef ZCC_LOG_CREATE_DIR_ERR_ID
#define ZCC_LOG_CREATE_DIR_ERR_ID                   ZCC_LOG_MSG_PRFX"2200W"
#endif
#define ZCC_LOG_CREATE_DIR_ERR_TEXT                 "Failed to create directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_CREATE_DIR_ERR                      ZCC_LOG_CREATE_DIR_ERR_ID" "ZCC_LOG_CREATE_DIR_ERR_TEXT

#ifndef ZCC_LOG_STAT_DIR_ERR_ID
#define ZCC_LOG_STAT_DIR_ERR_ID                     ZCC_LOG_MSG_PRFX"2201W"
#endif
#define ZCC_LOG_STAT_DIR_ERR_TEXT                   "Failed to stat directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_STAT_DIR_ERR                        ZCC_LOG_STAT_DIR_ERR_ID" "ZCC_LOG_STAT_DIR_ERR_TEXT

#ifndef ZCC_LOG_DELETE_DIR_ERR_ID
#define ZCC_LOG_DELETE_DIR_ERR_ID                   ZCC_LOG_MSG_PRFX"2202W"
#endif
#define ZCC_LOG_DELETE_DIR_ERR_TEXT                 "Failed to delete directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_DELETE_DIR_ERR                      ZCC_LOG_DELETE_DIR_ERR_ID" "ZCC_LOG_DELETE_DIR_ERR_TEXT

#ifndef ZCC_LOG_CHMOD_FILE_MODE_ERR_ID
#define ZCC_LOG_CHMOD_FILE_MODE_ERR_ID              ZCC_LOG_MSG_PRFX"2203W"
#endif
#define ZCC_LOG_CHMOD_FILE_MODE_ERR_TEXT            "Failed to chnmod file %s: illegal mode %s\n"
#define ZCC_LOG_CHMOD_FILE_MODE_ERR                 ZCC_LOG_CHMOD_FILE_MODE_ERR_ID" "ZCC_LOG_CHMOD_FILE_MODE_ERR_TEXT

#ifndef ZCC_LOG_CHMOD_FILE_ERR_ID
#define ZCC_LOG_CHMOD_FILE_ERR_ID                   ZCC_LOG_MSG_PRFX"2204W"
#endif
#define ZCC_LOG_CHMOD_FILE_ERR_TEXT                 "Failed to chnmod file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_CHMOD_FILE_ERR                      ZCC_LOG_CHMOD_FILE_ERR_ID" "ZCC_LOG_CHMOD_FILE_ERR_TEXT

#ifndef ZCC_LOG_STAT_FILE_ERR_ID
#define ZCC_LOG_STAT_FILE_ERR_ID                    ZCC_LOG_MSG_PRFX"2205W"
#endif
#define ZCC_LOG_STAT_FILE_ERR_TEXT                  "Failed to stat file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_STAT_FILE_ERR                       ZCC_LOG_STAT_FILE_ERR_ID" "ZCC_LOG_STAT_FILE_ERR_TEXT

#ifndef ZCC_LOG_DELETE_FILE_ERR_ID
#define ZCC_LOG_DELETE_FILE_ERR_ID                  ZCC_LOG_MSG_PRFX"2206W"
#endif
#define ZCC_LOG_DELETE_FILE_ERR_TEXT                "Failed to delete file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_DELETE_FILE_ERR                     ZCC_LOG_DELETE_FILE_ERR_ID" "ZCC_LOG_DELETE_FILE_ERR_TEXT

#ifndef ZCC_LOG_RENAME_DIR_ERR_ID
#define ZCC_LOG_RENAME_DIR_ERR_ID                   ZCC_LOG_MSG_PRFX"2207W"
#endif
#define ZCC_LOG_RENAME_DIR_ERR_TEXT                 "Failed to rename directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_RENAME_DIR_ERR                      ZCC_LOG_RENAME_DIR_ERR_ID" "ZCC_LOG_RENAME_DIR_ERR_TEXT

#ifndef ZCC_LOG_RENAME_FILE_ERR_ID
#define ZCC_LOG_RENAME_FILE_ERR_ID                  ZCC_LOG_MSG_PRFX"2208W"
#endif
#define ZCC_LOG_RENAME_FILE_ERR_TEXT                "Failed to rename file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_RENAME_FILE_ERR                     ZCC_LOG_RENAME_FILE_ERR_ID" "ZCC_LOG_RENAME_FILE_ERR_TEXT

#ifndef ZCC_LOG_COPY_DIR_ERR_ID
#define ZCC_LOG_COPY_DIR_ERR_ID                     ZCC_LOG_MSG_PRFX"2209W"
#endif
#define ZCC_LOG_COPY_DIR_ERR_TEXT                   "Failed to copy directory %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_COPY_DIR_ERR                        ZCC_LOG_COPY_DIR_ERR_ID" "ZCC_LOG_COPY_DIR_ERR_TEXT

#ifndef ZCC_LOG_COPY_FILE_ERR_ID
#define ZCC_LOG_COPY_FILE_ERR_ID                    ZCC_LOG_MSG_PRFX"2210W"
#endif
#define ZCC_LOG_COPY_FILE_ERR_TEXT                  "Failed to copy file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_COPY_FILE_ERR                       ZCC_LOG_COPY_FILE_ERR_ID" "ZCC_LOG_COPY_FILE_ERR_TEXT

#ifndef ZCC_LOG_OPEN_FILE_ERR_ID
#define ZCC_LOG_OPEN_FILE_ERR_ID                    ZCC_LOG_MSG_PRFX"2211W"
#endif
#define ZCC_LOG_OPEN_FILE_ERR_TEXT                  "Failed to open file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_OPEN_FILE_ERR                       ZCC_LOG_OPEN_FILE_ERR_ID" "ZCC_LOG_OPEN_FILE_ERR_TEXT

#ifndef ZCC_LOG_CLOSE_FILE_ERR_ID
#define ZCC_LOG_CLOSE_FILE_ERR_ID                   ZCC_LOG_MSG_PRFX"2212W"
#endif
#define ZCC_LOG_CLOSE_FILE_ERR_TEXT                 "Failed to close file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_CLOSE_FILE_ERR                      ZCC_LOG_CLOSE_FILE_ERR_ID" "ZCC_LOG_CLOSE_FILE_ERR_TEXT

#ifndef ZCC_LOG_CHANGE_TAG_ERR_ID
#define ZCC_LOG_CHANGE_TAG_ERR_ID                   ZCC_LOG_MSG_PRFX"2213W"
#endif
#define ZCC_LOG_CHANGE_TAG_ERR_TEXT                 "Failed to change tag file %s, (returnCode = 0x%x, reasonCode = 0x%x)\n"
#define ZCC_LOG_CHANGE_TAG_ERR                      ZCC_LOG_CHANGE_TAG_ERR_ID" "ZCC_LOG_CHANGE_TAG_ERR_TEXT

#ifndef ZCC_LOG_CHANGE_OWNER_ERR_ID
#define ZCC_LOG_CHANGE_OWNER_ERR_ID                 ZCC_LOG_MSG_PRFX"2214W"
#endif
#define ZCC_LOG_CHANGE_OWNER_ERR_TEXT               "Failed to change file owner %s, (returnCode = 0x%x, reasonCode = 0x% x)\n"
#define ZCC_LOG_CHANGE_OWNER_ERR                    ZCC_LOG_CHANGE_OWNER_ERR_ID" "ZCC_LOG_CHANGE_OWNER_ERR_TEXT

#ifndef ZCC_LOG_WRITE_FILE_ERR_ID
#define ZCC_LOG_WRITE_FILE_ERR_ID                   ZCC_LOG_MSG_PRFX"2215S"
#endif
#define ZCC_LOG_WRITE_FILE_ERR_TEXT                 "Error writing to file: return: %d, rsn: %d.\n"
#define ZCC_LOG_WRITE_FILE_ERR                      ZCC_LOG_WRITE_FILE_ERR_ID" "ZCC_LOG_WRITE_FILE_ERR_TEXT

#ifndef ZCC_LOG_DECODE_BASE64_ERR_ID
#define ZCC_LOG_DECODE_BASE64_ERR_ID                ZCC_LOG_MSG_PRFX"2216S"
#endif
#define ZCC_LOG_DECODE_BASE64_ERR_TEXT              "Error decoding BASE64.\n"
#define ZCC_LOG_DECODE_BASE64_ERR                   ZCC_LOG_DECODE_BASE64_ERR_ID" "ZCC_LOG_DECODE_BASE64_ERR_TEXT

#ifndef ZCC_LOG_CONVERT_EBCDIC_ERR_ID
#define ZCC_LOG_CONVERT_EBCDIC_ERR_ID               ZCC_LOG_MSG_PRFX"2217S"
#endif
#define ZCC_LOG_CONVERT_EBCDIC_ERR_TEXT             "Error converting to EBCDIC.\n"
#define ZCC_LOG_CONVERT_EBCDIC_ERR                  ZCC_LOG_CONVERT_EBCDIC_ERR_ID" "ZCC_LOG_CONVERT_EBCDIC_ERR_TEXT

#ifndef ZCC_LOG_AUTO_CONVERT_ERR_ID
#define ZCC_LOG_AUTO_CONVERT_ERR_ID                 ZCC_LOG_MSG_PRFX"2218S"
#endif
#define ZCC_LOG_AUTO_CONVERT_ERR_TEXT               "Failed to disable automatic conversion. Unexpected results may occur.\n"
#define ZCC_LOG_AUTO_CONVERT_ERR                    ZCC_LOG_AUTO_CONVERT_ERR_ID" "ZCC_LOG_AUTO_CONVERT_ERR_TEXT

#ifndef ZCC_LOG_UNSUPPORTED_ENCODING_ERR_ID
#define ZCC_LOG_UNSUPPORTED_ENCODING_ERR_ID         ZCC_LOG_MSG_PRFX"2219W"
#endif
#define ZCC_LOG_UNSUPPORTED_ENCODING_ERR_TEXT      "Unsupported encoding specified.\n"
#define ZCC_LOG_UNSUPPORTED_ENCODING_ERR            ZCC_LOG_UNSUPPORTED_ENCODING_ERR_ID" "ZCC_LOG_UNSUPPORTED_ENCODING_ERR_TEXT

/* HTTP Server */

#ifndef ZCC_LOG_SESSION_TOKEN_ERR_ID
#define ZCC_LOG_SESSION_TOKEN_ERR_ID                ZCC_LOG_MSG_PRFX"2250S"
#endif
#define ZCC_LOG_SESSION_TOKEN_ERR_TEXT              "Error: session token key not generated, RC = %d, RSN = %d\n"
#define ZCC_LOG_SESSION_TOKEN_ERR                   ZCC_LOG_SESSION_TOKEN_ERR_ID" "ZCC_LOG_SESSION_TOKEN_ERR_TEXT

#ifndef ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR_ID
#define ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR_ID           ZCC_LOG_MSG_PRFX"2251S"
#endif
#define ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR_TEXT         "Error: encoded session token buffer not allocated (size=%u, SLH=%p)\n"
#define ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR              ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR_ID" "ZCC_LOG_ENCODE_TOKEN_ALLOC_ERR_TEXT

#ifndef ZCC_LOG_TOKEN_ENCODING_ERR_ID
#define ZCC_LOG_TOKEN_ENCODING_ERR_ID               ZCC_LOG_MSG_PRFX"2252S"
#endif
#define ZCC_LOG_TOKEN_ENCODING_ERR_TEXT             "Error: session token encoding failed, RC = %d, RSN = %d\n"
#define ZCC_LOG_TOKEN_ENCODING_ERR                  ZCC_LOG_TOKEN_ENCODING_ERR_ID" "ZCC_LOG_TOKEN_ENCODING_ERR_TEXT

#ifndef ZCC_LOG_DECODE_TOKEN_ALLOC_ERR_ID
#define ZCC_LOG_DECODE_TOKEN_ALLOC_ERR_ID           ZCC_LOG_MSG_PRFX"2253S"
#endif
#define ZCC_LOG_DECODE_TOKEN_ALLOC_ERR_TEXT         "Error: decoded session token buffer not allocated (size=%u, SLH=%p)\n"
#define ZCC_LOG_DECODE_TOKEN_ALLOC_ERR              ZCC_LOG_DECODE_TOKEN_ALLOC_ERR_ID" "ZCC_LOG_DECODE_TOKEN_ALLOC_ERR_TEXT

#ifndef ZCC_LOG_TOKEN_DECODING_ERR_ID
#define ZCC_LOG_TOKEN_DECODING_ERR_ID               ZCC_LOG_MSG_PRFX"2254S"
#endif
#define ZCC_LOG_TOKEN_DECODING_ERR_TEXT             "Error: session token decoding failed, RC = %d, RSN = %d\n"
#define ZCC_LOG_TOKEN_DECODING_ERR                  ZCC_LOG_TOKEN_DECODING_ERR_ID" "ZCC_LOG_TOKEN_DECODING_ERR_TEXT

#ifndef ZCC_LOG_HTTP_METHOD_URI_ID
#define ZCC_LOG_HTTP_METHOD_URI_ID                  ZCC_LOG_MSG_PRFX"2255S"
#endif
#define ZCC_LOG_HTTP_METHOD_URI_TEXT                "httpserver: method='%s', URI='%s'\n"
#define ZCC_LOG_HTTP_METHOD_URI                     ZCC_LOG_HTTP_METHOD_URI_ID" "ZCC_LOG_HTTP_METHOD_URI_TEXT

#ifndef ZCC_LOG_ENCODE_CONVERSION_ERR_ID
#define ZCC_LOG_ENCODE_CONVERSION_ERR_ID            ZCC_LOG_MSG_PRFX"2256W"
#endif
#define ZCC_LOG_ENCODE_CONVERSION_ERR_TEXT          "Warning: encoding conversion was not disabled, return = %d, reason = %d\n"
#define ZCC_LOG_ENCODE_CONVERSION_ERR               ZCC_LOG_ENCODE_CONVERSION_ERR_ID" "ZCC_LOG_ENCODE_CONVERSION_ERR_TEXT

#ifndef ZCC_LOG_TEMPLATE_TAG_ERR_ID
#define ZCC_LOG_TEMPLATE_TAG_ERR_ID                 ZCC_LOG_MSG_PRFX"2257W"
#endif
#define ZCC_LOG_TEMPLATE_TAG_ERR_TEXT               "*** WARNING *** no template tag service function for %s\n"
#define ZCC_LOG_TEMPLATE_TAG_ERR                    ZCC_LOG_TEMPLATE_TAG_ERR_ID" "ZCC_LOG_TEMPLATE_TAG_ERR_TEXT

// jcsi.c

#ifndef ZCC_LOG_FILTER_ARG_ERR_ID
#define ZCC_LOG_FILTER_ARG_ERR_ID                   ZCC_LOG_MSG_PRFX"2300W"
#endif
#define ZCC_LOG_FILTER_ARG_ERR_TEXT                 "missing argument after -filter\n"
#define ZCC_LOG_FILTER_ARG_ERR                      ZCC_LOG_FILTER_ARG_ERR_ID" "ZCC_LOG_FILTER_ARG_ERR_TEXT

#ifndef ZCC_LOG_IGGCSI00_ERR_ID
#define ZCC_LOG_IGGCSI00_ERR_ID                     ZCC_LOG_MSG_PRFX"2301W"
#endif
#define ZCC_LOG_IGGCSI00_ERR_TEXT                   "missing argument after -filter\n"
#define ZCC_LOG_IGGCSI00_ERR                        ZCC_LOG_IGGCSI00_ERR_ID" "ZCC_LOG_IGGCSI00_ERR_TEXT

#ifndef ZCC_LOG_CSI_ERR_ID
#define ZCC_LOG_CSI_ERR_ID                          ZCC_LOG_MSG_PRFX"2302W"
#endif
#define ZCC_LOG_CSI_ERR_TEXT                        "CSI failed ret=%d, rc=%d rchex=%x\n"
#define ZCC_LOG_CSI_ERR                             ZCC_LOG_CSI_ERR_ID" "ZCC_LOG_CSI_ERR_TEXT

#ifndef ZCC_LOG_CSI_QUERY_ID
#define ZCC_LOG_CSI_QUERY_ID                        ZCC_LOG_MSG_PRFX"2303I"
#endif
#define ZCC_LOG_CSI_QUERY_TEXT                      "csi query for %s\n"
#define ZCC_LOG_CSI_QUERY                           ZCC_LOG_CSI_QUERY_ID" "ZCC_LOG_CSI_QUERY_TEXT

#ifndef ZCC_LOG_CSI_ENTRY_ERR_ID
#define ZCC_LOG_CSI_ENTRY_ERR_ID                    ZCC_LOG_MSG_PRFX"2304W"
#endif
#define ZCC_LOG_CSI_ENTRY_ERR_TEXT                  "entry has error\n"
#define ZCC_LOG_CSI_ENTRY_ERR                       ZCC_LOG_CSI_ENTRY_ERR_ID" "ZCC_LOG_CSI_ENTRY_ERR_TEXT

#ifndef ZCC_LOG_CSI_NO_ENTRY_ID
#define ZCC_LOG_CSI_NO_ENTRY_ID                     ZCC_LOG_MSG_PRFX"2305W"
#endif
#define ZCC_LOG_CSI_NO_ENTRY_TEXT                   "no entries, no look up failure either\n"
#define ZCC_LOG_CSI_NO_ENTRY                        ZCC_LOG_CSI_NO_ENTRY_ID" "ZCC_LOG_CSI_NO_ENTRY_TEXT

// JSON.C

#ifndef ZCC_LOG_JSON_WRITE_ERR_ID
#define ZCC_LOG_JSON_WRITE_ERR_ID                   ZCC_LOG_MSG_PRFX"2350W"
#endif
#define ZCC_LOG_JSON_WRITE_ERR_TEXT                 "JSON: write error, rc %d, return code %d, reason code %08X\n"
#define ZCC_LOG_JSON_WRITE_ERR                      ZCC_LOG_JSON_WRITE_ERR_ID" "ZCC_LOG_JSON_WRITE_ERR_TEXT

#ifndef ZCC_LOG_JSON_ATTEMPT_ERR_ID
#define ZCC_LOG_JSON_ATTEMPT_ERR_ID                 ZCC_LOG_MSG_PRFX"2351W"
#endif
#define ZCC_LOG_JSON_ATTEMPT_ERR_TEXT               "JSON: write error, too many attempts\n"
#define ZCC_LOG_JSON_ATTEMPT_ERR                    ZCC_LOG_JSON_ATTEMPT_ERR_ID" "ZCC_LOG_JSON_ATTEMPT_ERR_TEXT

#ifndef ZCC_LOG_JSON_PROP_MISSING_ID
#define ZCC_LOG_JSON_PROP_MISSING_ID                ZCC_LOG_MSG_PRFX"2352W"
#endif
#define ZCC_LOG_JSON_PROP_MISSING_TEXT              "JSON property '%s' not found in object at 0x%x\n"
#define ZCC_LOG_JSON_PROP_MISSING                   ZCC_LOG_JSON_PROP_MISSING_ID" "ZCC_LOG_JSON_PROP_MISSING_TEXT

#ifndef ZCC_LOG_JSON_PROP_TYPE_ERR_ID
#define ZCC_LOG_JSON_PROP_TYPE_ERR_ID               ZCC_LOG_MSG_PRFX"2353W"
#endif
#define ZCC_LOG_JSON_PROP_TYPE_ERR_TEXT             "JSON property '%s' has wrong type in object at 0x%x\n"
#define ZCC_LOG_JSON_PROP_TYPE_ERR                  ZCC_LOG_JSON_PROP_TYPE_ERR_ID" "ZCC_LOG_JSON_PROP_TYPE_ERR_TEXT

/* MTLSKT */

#ifndef ZCC_LOG_SET_SOCKOPT_ERR_ID
#define ZCC_LOG_SET_SOCKOPT_ERR_ID                  ZCC_LOG_MSG_PRFX"2400S"
#endif
#define ZCC_LOG_SET_SOCKOPT_ERR_TEXT                "set sockopt failed, level=0x%x, option=0x%x ret code %d reason 0x%x\n"
#define ZCC_LOG_SET_SOCKOPT_ERR                     ZCC_LOG_SET_SOCKOPT_ERR_ID" "ZCC_LOG_SET_SOCKOPT_ERR_TEXT

#ifndef ZCC_LOG_GET_HOSTNAME_ERR_ID
#define ZCC_LOG_GET_HOSTNAME_ERR_ID                 ZCC_LOG_MSG_PRFX"2401W"
#endif
#define ZCC_LOG_GET_HOSTNAME_ERR_TEXT               "getHostName V4 failure, returnCode %d reason code %d\n"
#define ZCC_LOG_GET_HOSTNAME_ERR                    ZCC_LOG_GET_HOSTNAME_ERR_ID" "ZCC_LOG_GET_HOSTNAME_ERR_TEXT

#ifndef ZCC_LOG_GET_SOCKOPT_ERR_ID
#define ZCC_LOG_GET_SOCKOPT_ERR_ID                  ZCC_LOG_MSG_PRFX"2402S"
#endif
#define ZCC_LOG_GET_SOCKOPT_ERR_TEXT                "get sockopt failed, ret code %d reason 0x%x\n"
#define ZCC_LOG_GET_SOCKOPT_ERR                     ZCC_LOG_GET_SOCKOPT_ERR_ID" "ZCC_LOG_GET_SOCKOPT_ERR_TEXT

#ifndef ZCC_LOG_BPXFCT_ERR_ID
#define ZCC_LOG_BPXFCT_ERR_ID                       ZCC_LOG_MSG_PRFX"2403S"
#endif
#define ZCC_LOG_BPXFCT_ERR_TEXT                     "BPXFCT failed, ret code %d reason 0x%x\n"
#define ZCC_LOG_BPXFCT_ERR                          ZCC_LOG_BPXFCT_ERR_ID" "ZCC_LOG_BPXFCT_ERR_TEXT

#ifndef ZCC_LOG_SOCKET_SD_ERR_ID
#define ZCC_LOG_SOCKET_SD_ERR_ID                    ZCC_LOG_MSG_PRFX"2404W"
#endif
#define ZCC_LOG_SOCKET_SD_ERR_TEXT                  "SD=%d out of range (> %d)\n"
#define ZCC_LOG_SOCKET_SD_ERR                       ZCC_LOG_SOCKET_SD_ERR_ID" "ZCC_LOG_SOCKET_SD_ERR_TEXT

/* PDS UTIL */

#ifndef ZCC_LOG_PDS_READ_ERR_EMPTY_ID
#define ZCC_LOG_PDS_READ_ERR_EMPTY_ID               ZCC_LOG_MSG_PRFX"2450W"
#endif
#define ZCC_LOG_PDS_READ_ERR_EMPTY_TEXT             "Error encountered on reading PDS member, returning empty list\n"
#define ZCC_LOG_PDS_READ_ERR_EMPTY                  ZCC_LOG_PDS_READ_ERR_EMPTY_ID" "ZCC_LOG_PDS_READ_ERR_EMPTY_TEXT

#ifndef ZCC_LOG_PDS_READ_ERR_CURRENT_ID
#define ZCC_LOG_PDS_READ_ERR_CURRENT_ID             ZCC_LOG_MSG_PRFX"2451W"
#endif
#define ZCC_LOG_PDS_READ_ERR_CURRENT_TEXT           "Error encountered reading PDS member, returning current list\n"
#define ZCC_LOG_PDS_READ_ERR_CURRENT                ZCC_LOG_PDS_READ_ERR_CURRENT_ID" "ZCC_LOG_PDS_READ_ERR_CURRENT_TEXT

/* Recovery.c */

#ifndef ZCC_LOG_RECOVERY_ERR_ID
#define ZCC_LOG_RECOVERY_ERR_ID                     ZCC_LOG_MSG_PRFX"2500W"
#endif
#define ZCC_LOG_RECOVERY_ERR_TEXT                   "error: recovery not established, rc=%d\n"
#define ZCC_LOG_RECOVERY_ERR                        ZCC_LOG_RECOVERY_ERR_ID" "ZCC_LOG_RECOVERY_ERR_TEXT

/* stcbase.c */

#ifndef ZCC_LOG_EVENT_SET_ERR_ID
#define ZCC_LOG_EVENT_SET_ERR_ID                    ZCC_LOG_MSG_PRFX"2550W"
#endif
#define ZCC_LOG_EVENT_SET_ERR_TEXT                  "*** WARNING FAILED TO BUILD EVENT SET\n"
#define ZCC_LOG_EVENT_SET_ERR                       ZCC_LOG_EVENT_SET_ERR_ID" "ZCC_LOG_EVENT_SET_ERR_TEXT

/* UTILS */

#ifndef ZCC_LOG_EMPTY_BUFFER_ERR_ID
#define ZCC_LOG_EMPTY_BUFFER_ERR_ID                 ZCC_LOG_MSG_PRFX"2600S"
#endif
#define ZCC_LOG_EMPTY_BUFFER_ERR_TEXT               "the buffer is empty at %x\n"
#define ZCC_LOG_EMPTY_BUFFER_ERR                    ZCC_LOG_EMPTY_BUFFER_ERR_ID" "ZCC_LOG_EMPTY_BUFFER_ERR_TEXT

#ifndef ZCC_LOG_SLH_MEM_ERR_ID
#define ZCC_LOG_SLH_MEM_ERR_ID                      ZCC_LOG_MSG_PRFX"2601W"
#endif
#define ZCC_LOG_SLH_MEM_ERR_TEXT                    "could not get any memory for SLH extension, extension size was 0x%x\n"
#define ZCC_LOG_SLH_MEM_ERR                         ZCC_LOG_SLH_MEM_ERR_ID" "ZCC_LOG_SLH_MEM_ERR_TEXT

#ifndef ZCC_LOG_BLOCK_SIZE_ALLOC_ERR_ID
#define ZCC_LOG_BLOCK_SIZE_ALLOC_ERR_ID             ZCC_LOG_MSG_PRFX"2602W"
#endif
#define ZCC_LOG_BLOCK_SIZE_ALLOC_ERR_TEXT           "cannot allocate above block size %d > %d mb %d bc %d\n"
#define ZCC_LOG_BLOCK_SIZE_ALLOC_ERR                ZCC_LOG_BLOCK_SIZE_ALLOC_ERR_ID" "ZCC_LOG_BLOCK_SIZE_ALLOC_ERR_TEXT

#ifndef ZCC_LOG_MALLOC_ALLOC_ERR_ID
#define ZCC_LOG_MALLOC_ALLOC_ERR_ID                 ZCC_LOG_MSG_PRFX"2603W"
#endif
#define ZCC_LOG_MALLOC_ALLOC_ERR_TEXT               "malloc failure for allocation of %d bytes\n"
#define ZCC_LOG_MALLOC_ALLOC_ERR                    ZCC_LOG_MALLOC_ALLOC_ERR_ID" "ZCC_LOG_MALLOC_ALLOC_ERR_TEXT


//XML


#ifndef ZCC_LOG_XML_PRINTER_MSG_ID
#define ZCC_LOG_XML_PRINTER_MSG_ID                  ZCC_LOG_MSG_PRFX"2650W"
#endif
#define ZCC_LOG_XML_PRINTER_MSG_TEXT                "makeXMLPrinter custom=%d full 0x%x byte 0x%x\n"
#define ZCC_LOG_XML_PRINTER_MSG                     ZCC_LOG_XML_PRINTER_MSG_ID" "ZCC_LOG_XML_PRINTER_MSG_TEXT

#ifndef ZCC_LOG_XML_MALLOC_ERR_ID
#define ZCC_LOG_XML_MALLOC_ERR_ID                   ZCC_LOG_MSG_PRFX"2651S"
#endif
#define ZCC_LOG_XML_MALLOC_ERR_TEXT                 "malloc fail in makeHttpXMLPrinter...\n"
#define ZCC_LOG_XML_MALLOC_ERR                      ZCC_LOG_XML_MALLOC_ERR_ID" "ZCC_LOG_XML_MALLOC_ERR_TEXT

#ifndef ZCC_LOG_XML_STACK_LIMIT_ERR_ID
#define ZCC_LOG_XML_STACK_LIMIT_ERR_ID              ZCC_LOG_MSG_PRFX"2652W"
#endif
#define ZCC_LOG_XML_STACK_LIMIT_ERR_TEXT            "XML Printer stack limit exceeded\n"
#define ZCC_LOG_XML_STACK_LIMIT_ERR                 ZCC_LOG_XML_STACK_LIMIT_ERR_ID" "ZCC_LOG_XML_STACK_LIMIT_ERR_TEXT

#ifndef ZCC_LOG_XML_NEG_STACK_ERR_ID
#define ZCC_LOG_XML_NEG_STACK_ERR_ID                ZCC_LOG_MSG_PRFX"2653W"
#endif
#define ZCC_LOG_XML_NEG_STACK_ERR_TEXT              "deep /negative XML stack size %d seen while starting level for %s\n"
#define ZCC_LOG_XML_NEG_STACK_ERR                   ZCC_LOG_XML_NEG_STACK_ERR_ID" "ZCC_LOG_XML_NEG_STACK_ERR_TEXT

#ifndef ZCC_LOG_BAOS_LIMIT_ERR_ID
#define ZCC_LOG_BAOS_LIMIT_ERR_ID                   ZCC_LOG_MSG_PRFX"2654S"
#endif
#define ZCC_LOG_BAOS_LIMIT_ERR_TEXT                 "PANIC, baos out of room\n"
#define ZCC_LOG_BAOS_LIMIT_ERR                      ZCC_LOG_BAOS_LIMIT_ERR_ID" "ZCC_LOG_BAOS_LIMIT_ERR_TEXT

#ifndef ZCC_LOG_XML_SYNTAX_ERR_ID
#define ZCC_LOG_XML_SYNTAX_ERR_ID                   ZCC_LOG_MSG_PRFX"2655S"
#endif
#define ZCC_LOG_XML_SYNTAX_ERR_TEXT                 "SYNTAX ERROR line=%d %s\n"
#define ZCC_LOG_XML_SYNTAX_ERR                      ZCC_LOG_XML_SYNTAX_ERR_ID" "ZCC_LOG_XML_SYNTAX_ERR_TEXT

#ifndef ZCC_LOG_XML_SANITY_ERR_ID
#define ZCC_LOG_XML_SANITY_ERR_ID                   ZCC_LOG_MSG_PRFX"2656I"
#endif
#define ZCC_LOG_XML_SANITY_ERR_TEXT                 "sanity check failed at %s\n"
#define ZCC_LOG_XML_SANITY_ERR                      ZCC_LOG_XML_SANITY_ERR_ID" "ZCC_LOG_XML_SANITY_ERR_TEXT

#ifndef ZCC_LOG_XML_UNKNOWN_STATE_ID
#define ZCC_LOG_XML_UNKNOWN_STATE_ID                ZCC_LOG_MSG_PRFX"2657W"
#endif
#define ZCC_LOG_XML_UNKNOWN_STATE_TEXT              "unknown state\n"
#define ZCC_LOG_XML_UNKNOWN_STATE                   ZCC_LOG_XML_UNKNOWN_STATE_ID" "ZCC_LOG_XML_UNKNOWN_STATE_TEXT

#ifndef ZCC_LOG_XML_UNKNOWN_TOKEN_ID
#define ZCC_LOG_XML_UNKNOWN_TOKEN_ID                ZCC_LOG_MSG_PRFX"2658W"
#endif
#define ZCC_LOG_XML_UNKNOWN_TOKEN_TEXT              "help unknown token\n"
#define ZCC_LOG_XML_UNKNOWN_TOKEN                   ZCC_LOG_XML_UNKNOWN_TOKEN_ID" "ZCC_LOG_XML_UNKNOWN_TOKEN_TEXT

//ZOS ACCOUNTS

#ifndef ZCC_LOG_METTLE_BPXGPN_ERR_ID
#define ZCC_LOG_METTLE_BPXGPN_ERR_ID                ZCC_LOG_MSG_PRFX"2700S"
#endif
#define ZCC_LOG_METTLE_BPXGPN_ERR_TEXT              "BPXGPN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXGPN_ERR                   ZCC_LOG_METTLE_BPXGPN_ERR_ID" "ZCC_LOG_METTLE_BPXGPN_ERR_TEXT

#ifndef ZCC_LOG_BPXGPN_ERR_ID
#define ZCC_LOG_BPXGPN_ERR_ID                       ZCC_LOG_MSG_PRFX"2701S"
#endif
#define ZCC_LOG_BPXGPN_ERR_TEXT                     "BPXGPN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXGPN_ERR                          ZCC_LOG_BPXGPN_ERR_ID" "ZCC_LOG_BPXGPN_ERR_TEXT

#ifndef ZCC_LOG_BPXGPN_MSG_ID
#define ZCC_LOG_BPXGPN_MSG_ID                       ZCC_LOG_MSG_PRFX"2702I"
#endif
#define ZCC_LOG_BPXGPN_MSG_TEXT                     "BPXGPN (%s) OK: returnVal: %d\n"
#define ZCC_LOG_BPXGPN_MSG                          ZCC_LOG_BPXGPN_MSG_ID" "ZCC_LOG_BPXGPN_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXGGN_ERR_ID
#define ZCC_LOG_METTLE_BPXGGN_ERR_ID                ZCC_LOG_MSG_PRFX"2703S"
#endif
#define ZCC_LOG_METTLE_BPXGGN_ERR_TEXT              "BPXGGN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXGGN_ERR                   ZCC_LOG_METTLE_BPXGGN_ERR_ID" "ZCC_LOG_METTLE_BPXGGN_ERR_TEXT

#ifndef ZCC_LOG_BPXGGN_ERR_ID
#define ZCC_LOG_BPXGGN_ERR_ID                       ZCC_LOG_MSG_PRFX"2704S"
#endif
#define ZCC_LOG_BPXGGN_ERR_TEXT                     "BPXGGN (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXGGN_ERR                          ZCC_LOG_BPXGGN_ERR_ID" "ZCC_LOG_BPXGGN_ERR_TEXT

#ifndef ZCC_LOG_BPXGGN_MSG_ID
#define ZCC_LOG_BPXGGN_MSG_ID                       ZCC_LOG_MSG_PRFX"2705I"
#endif
#define ZCC_LOG_BPXGGN_MSG_TEXT                     "BPXGGN (%s) OK: returnVal: %d\n"
#define ZCC_LOG_BPXGGN_MSG                          ZCC_LOG_BPXGGN_MSG_ID" "ZCC_LOG_BPXGGN_MSG_TEXT

//ZOS FILE

#ifndef ZCC_LOG_METTLE_BPXOPN_ERR_ID
#define ZCC_LOG_METTLE_BPXOPN_ERR_ID                ZCC_LOG_MSG_PRFX"2800S"
#endif
#define ZCC_LOG_METTLE_BPXOPN_ERR_TEXT              "BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXOPN_ERR                   ZCC_LOG_METTLE_BPXOPN_ERR_ID" "ZCC_LOG_METTLE_BPXOPN_ERR_TEXT

#ifndef ZCC_LOG_BPXOPN_ERR_ID
#define ZCC_LOG_BPXOPN_ERR_ID                       ZCC_LOG_MSG_PRFX"2801S"
#endif
#define ZCC_LOG_BPXOPN_ERR_TEXT                     "BPXOPN (%s, 0%o, 0%o) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXOPN_ERR                          ZCC_LOG_BPXOPN_ERR_ID" "ZCC_LOG_BPXOPN_ERR_TEXT

#ifndef ZCC_LOG_BPXOPN_MSG_ID
#define ZCC_LOG_BPXOPN_MSG_ID                       ZCC_LOG_MSG_PRFX"2802I"
#endif
#define ZCC_LOG_BPXOPN_MSG_TEXT                     "BPXOPN (%s, 0%o, 0%o) OK: returnValue: %d\n"
#define ZCC_LOG_BPXOPN_MSG                          ZCC_LOG_BPXOPN_MSG_ID" "ZCC_LOG_BPXOPN_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXRED_ERR_ID
#define ZCC_LOG_METTLE_BPXRED_ERR_ID                ZCC_LOG_MSG_PRFX"2803S"
#endif
#define ZCC_LOG_METTLE_BPXRED_ERR_TEXT              "BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXRED_ERR                   ZCC_LOG_METTLE_BPXRED_ERR_ID" "ZCC_LOG_METTLE_BPXRED_ERR_TEXT

#ifndef ZCC_LOG_BPXRED_ERR_ID
#define ZCC_LOG_BPXRED_ERR_ID                       ZCC_LOG_MSG_PRFX"2804S"
#endif
#define ZCC_LOG_BPXRED_ERR_TEXT                     "BPXRED FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXRED_ERR                          ZCC_LOG_BPXRED_ERR_ID" "ZCC_LOG_BPXRED_ERR_TEXT

#ifndef ZCC_LOG_BPXRED_MSG_ID
#define ZCC_LOG_BPXRED_MSG_ID                       ZCC_LOG_MSG_PRFX"2805I"
#endif
#define ZCC_LOG_BPXRED_MSG_TEXT                     "BPXRED OK: Read %d bytes\n"
#define ZCC_LOG_BPXRED_MSG                          ZCC_LOG_BPXRED_MSG_ID" "ZCC_LOG_BPXRED_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXWRT_ERR_ID
#define ZCC_LOG_METTLE_BPXWRT_ERR_ID                ZCC_LOG_MSG_PRFX"2806S"
#endif
#define ZCC_LOG_METTLE_BPXWRT_ERR_TEXT              "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXWRT_ERR                   ZCC_LOG_METTLE_BPXWRT_ERR_ID" "ZCC_LOG_METTLE_BPXWRT_ERR_TEXT

#ifndef ZCC_LOG_BPXWRT_ERR_ID
#define ZCC_LOG_BPXWRT_ERR_ID                       ZCC_LOG_MSG_PRFX"2807S"
#endif
#define ZCC_LOG_BPXWRT_ERR_TEXT                     "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXWRT_ERR                          ZCC_LOG_BPXWRT_ERR_ID" "ZCC_LOG_BPXWRT_ERR_TEXT

#ifndef ZCC_LOG_BPXWRT_MSG_ID
#define ZCC_LOG_BPXWRT_MSG_ID                       ZCC_LOG_MSG_PRFX"2808I"
#endif
#define ZCC_LOG_BPXWRT_MSG_TEXT                     "BPXWRT OK: Wrote %d bytes\n"
#define ZCC_LOG_BPXWRT_MSG                          ZCC_LOG_BPXWRT_MSG_ID" "ZCC_LOG_BPXWRT_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXWRT_ERR_ID
#define ZCC_LOG_METTLE_BPXWRT_ERR_ID                ZCC_LOG_MSG_PRFX"2809S"
#endif
#define ZCC_LOG_METTLE_BPXWRT_ERR_TEXT              "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXWRT_ERR                   ZCC_LOG_METTLE_BPXWRT_ERR_ID" "ZCC_LOG_METTLE_BPXWRT_ERR_TEXT

#ifndef ZCC_LOG_BPXWRT_ERR_ID
#define ZCC_LOG_BPXWRT_ERR_ID                       ZCC_LOG_MSG_PRFX"2810S"
#endif
#define ZCC_LOG_BPXWRT_ERR_TEXT                     "BPXWRT (fd: %d, file: %s, desired bytes: %d) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXWRT_ERR                          ZCC_LOG_BPXWRT_ERR_ID" "ZCC_LOG_BPXWRT_ERR_TEXT

#ifndef ZCC_LOG_BPXWRT_MSG_ID
#define ZCC_LOG_BPXWRT_MSG_ID                       ZCC_LOG_MSG_PRFX"2811I"
#endif
#define ZCC_LOG_BPXWRT_MSG_TEXT                     "BPXWRT OK: Wrote %d bytes\n"
#define ZCC_LOG_BPXWRT_MSG                          ZCC_LOG_BPXWRT_MSG_ID" "ZCC_LOG_BPXWRT_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXCLO_ERR_ID
#define ZCC_LOG_METTLE_BPXCLO_ERR_ID                ZCC_LOG_MSG_PRFX"2812S"
#endif
#define ZCC_LOG_METTLE_BPXCLO_ERR_TEXT              "BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXCLO_ERR                   ZCC_LOG_METTLE_BPXCLO_ERR_ID" "ZCC_LOG_METTLE_BPXCLO_ERR_TEXT

#ifndef ZCC_LOG_BPXCLO_ERR_ID
#define ZCC_LOG_BPXCLO_ERR_ID                       ZCC_LOG_MSG_PRFX"2813S"
#endif
#define ZCC_LOG_BPXCLO_ERR_TEXT                     "BPXCLO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXCLO_ERR                          ZCC_LOG_BPXCLO_ERR_ID" "ZCC_LOG_BPXCLO_ERR_TEXT

#ifndef ZCC_LOG_BPXCLO_MSG_ID
#define ZCC_LOG_BPXCLO_MSG_ID                       ZCC_LOG_MSG_PRFX"2814I"
#endif
#define ZCC_LOG_BPXCLO_MSG_TEXT                     "BPXCLO OK: returnValue: %d\n"
#define ZCC_LOG_BPXCLO_MSG                          ZCC_LOG_BPXCLO_MSG_ID" "ZCC_LOG_BPXCLO_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXCHR_ERR_ID
#define ZCC_LOG_METTLE_BPXCHR_ERR_ID                ZCC_LOG_MSG_PRFX"2815S"
#endif
#define ZCC_LOG_METTLE_BPXCHR_ERR_TEXT              "BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXCHR_ERR                   ZCC_LOG_METTLE_BPXCHR_ERR_ID" "ZCC_LOG_METTLE_BPXCHR_ERR_TEXT

#ifndef ZCC_LOG_BPXCHR_ERR_ID
#define ZCC_LOG_BPXCHR_ERR_ID                       ZCC_LOG_MSG_PRFX"2816S"
#endif
#define ZCC_LOG_BPXCHR_ERR_TEXT                     "BPXCHR FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXCHR_ERR                          ZCC_LOG_BPXCHR_ERR_ID" "ZCC_LOG_BPXCHR_ERR_TEXT

#ifndef ZCC_LOG_BPXCHR_MSG_ID
#define ZCC_LOG_BPXCHR_MSG_ID                       ZCC_LOG_MSG_PRFX"2817I"
#endif
#define ZCC_LOG_BPXCHR_MSG_TEXT                     "BPXCHR (%s) OK: returnValue: %d\n\n"
#define ZCC_LOG_BPXCHR_MSG                          ZCC_LOG_BPXCHR_MSG_ID" "ZCC_LOG_BPXCHR_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXCHM_ERR_ID
#define ZCC_LOG_METTLE_BPXCHM_ERR_ID                ZCC_LOG_MSG_PRFX"2818S"
#endif
#define ZCC_LOG_METTLE_BPXCHM_ERR_TEXT              "BPXCHM FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXCHM_ERR                   ZCC_LOG_METTLE_BPXCHM_ERR_ID" "ZCC_LOG_METTLE_BPXCHM_ERR_TEXT

#ifndef ZCC_LOG_BPXCHM_ERR_ID
#define ZCC_LOG_BPXCHM_ERR_ID                       ZCC_LOG_MSG_PRFX"2819S"
#endif
#define ZCC_LOG_BPXCHM_ERR_TEXT                     "BPXCHM FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXCHM_ERR                          ZCC_LOG_BPXCHM_ERR_ID" "ZCC_LOG_BPXCHM_ERR_TEXT

#ifndef ZCC_LOG_BPXCHM_MSG_ID
#define ZCC_LOG_BPXCHM_MSG_ID                       ZCC_LOG_MSG_PRFX"2820I"
#endif
#define ZCC_LOG_BPXCHM_MSG_TEXT                     "BPXCHM (%s) OK: returnValue: %d\n\n"
#define ZCC_LOG_BPXCHM_MSG                          ZCC_LOG_BPXCHM_MSG_ID" "ZCC_LOG_BPXCHM_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXREN_ERR_ID
#define ZCC_LOG_METTLE_BPXREN_ERR_ID                ZCC_LOG_MSG_PRFX"2821S"
#endif
#define ZCC_LOG_METTLE_BPXREN_ERR_TEXT              "BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXREN_ERR                   ZCC_LOG_METTLE_BPXREN_ERR_ID" "ZCC_LOG_METTLE_BPXREN_ERR_TEXT

#ifndef ZCC_LOG_BPXREN_ERR_ID
#define ZCC_LOG_BPXREN_ERR_ID                       ZCC_LOG_MSG_PRFX"2822S"
#endif
#define ZCC_LOG_BPXREN_ERR_TEXT                     "BPXREN FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXREN_ERR                          ZCC_LOG_BPXREN_ERR_ID" "ZCC_LOG_BPXREN_ERR_TEXT

#ifndef ZCC_LOG_BPXREN_MSG_ID
#define ZCC_LOG_BPXREN_MSG_ID                       ZCC_LOG_MSG_PRFX"2823I"
#endif
#define ZCC_LOG_BPXREN_MSG_TEXT                     "BPXREN (%s) OK: returnVal: %d\n"
#define ZCC_LOG_BPXREN_MSG                          ZCC_LOG_BPXREN_MSG_ID" "ZCC_LOG_BPXREN_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXUNL_ERR_ID
#define ZCC_LOG_METTLE_BPXUNL_ERR_ID                ZCC_LOG_MSG_PRFX"2824S"
#endif
#define ZCC_LOG_METTLE_BPXUNL_ERR_TEXT              "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXUNL_ERR                   ZCC_LOG_METTLE_BPXUNL_ERR_ID" "ZCC_LOG_METTLE_BPXUNL_ERR_TEXT

#ifndef ZCC_LOG_BPXUNL_ERR_ID
#define ZCC_LOG_BPXUNL_ERR_ID                       ZCC_LOG_MSG_PRFX"2825S"
#endif
#define ZCC_LOG_BPXUNL_ERR_TEXT                     "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXUNL_ERR                          ZCC_LOG_BPXUNL_ERR_ID" "ZCC_LOG_BPXUNL_ERR_TEXT

#ifndef ZCC_LOG_BPXUNL_MSG_ID
#define ZCC_LOG_BPXUNL_MSG_ID                       ZCC_LOG_MSG_PRFX"2826I"
#endif
#define ZCC_LOG_BPXUNL_MSG_TEXT                     "BPXUNL (%s) OK: returnVal: %d\n"
#define ZCC_LOG_BPXUNL_MSG                          ZCC_LOG_BPXUNL_MSG_ID" "ZCC_LOG_BPXUNL_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXUNL_ERR_ID
#define ZCC_LOG_METTLE_BPXUNL_ERR_ID                ZCC_LOG_MSG_PRFX"2827S"
#endif
#define ZCC_LOG_METTLE_BPXUNL_ERR_TEXT              "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXUNL_ERR                   ZCC_LOG_METTLE_BPXUNL_ERR_ID" "ZCC_LOG_METTLE_BPXUNL_ERR_TEXT

#ifndef ZCC_LOG_BPXUNL_ERR_ID
#define ZCC_LOG_BPXUNL_ERR_ID                       ZCC_LOG_MSG_PRFX"2828S"
#endif
#define ZCC_LOG_BPXUNL_ERR_TEXT                     "BPXUNL (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXUNL_ERR                          ZCC_LOG_BPXUNL_ERR_ID" "ZCC_LOG_BPXUNL_ERR_TEXT

#ifndef ZCC_LOG_BPXUNL_MSG_ID
#define ZCC_LOG_BPXUNL_MSG_ID                       ZCC_LOG_MSG_PRFX"2829I"
#endif
#define ZCC_LOG_BPXUNL_MSG_TEXT                     "BPXUNL (%s) OK: returnVal: %d\n"
#define ZCC_LOG_BPXUNL_MSG                          ZCC_LOG_BPXUNL_MSG_ID" "ZCC_LOG_BPXUNL_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXSTA_ERR_ID
#define ZCC_LOG_METTLE_BPXSTA_ERR_ID                ZCC_LOG_MSG_PRFX"2830S"
#endif
#define ZCC_LOG_METTLE_BPXSTA_ERR_TEXT              "BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXSTA_ERR                   ZCC_LOG_METTLE_BPXSTA_ERR_ID" "ZCC_LOG_METTLE_BPXSTA_ERR_TEXT

#ifndef ZCC_LOG_BPXSTA_ERR_ID
#define ZCC_LOG_BPXSTA_ERR_ID                       ZCC_LOG_MSG_PRFX"2831S"
#endif
#define ZCC_LOG_BPXSTA_ERR_TEXT                     "BPXSTA (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXSTA_ERR                          ZCC_LOG_BPXSTA_ERR_ID" "ZCC_LOG_BPXSTA_ERR_TEXT

#ifndef ZCC_LOG_BPXSTA_MSG_ID
#define ZCC_LOG_BPXSTA_MSG_ID                       ZCC_LOG_MSG_PRFX"2832I"
#endif
#define ZCC_LOG_BPXSTA_MSG_TEXT                     "BPXSTA (%s) OK: returnVal: %d, type: %s\n"
#define ZCC_LOG_BPXSTA_MSG                          ZCC_LOG_BPXSTA_MSG_ID" "ZCC_LOG_BPXSTA_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXLST_ERR_ID
#define ZCC_LOG_METTLE_BPXLST_ERR_ID                ZCC_LOG_MSG_PRFX"2833S"
#endif
#define ZCC_LOG_METTLE_BPXLST_ERR_TEXT              "BPXLST (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXLST_ERR                   ZCC_LOG_METTLE_BPXLST_ERR_ID" "ZCC_LOG_METTLE_BPXLST_ERR_TEXT

#ifndef ZCC_LOG_BPXLST_ERR_ID
#define ZCC_LOG_BPXLST_ERR_ID                       ZCC_LOG_MSG_PRFX"2834S"
#endif
#define ZCC_LOG_BPXLST_ERR_TEXT                     "BPXLST (%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXLST_ERR                          ZCC_LOG_BPXLST_ERR_ID" "ZCC_LOG_BPXLST_ERR_TEXT

#ifndef ZCC_LOG_BPXLST_MSG_ID
#define ZCC_LOG_BPXLST_MSG_ID                       ZCC_LOG_MSG_PRFX"2835I"
#endif
#define ZCC_LOG_BPXLST_MSG_TEXT                     "BPXLST (%s) OK: returnVal: %d, type: %s\n"
#define ZCC_LOG_BPXLST_MSG                          ZCC_LOG_BPXLST_MSG_ID" "ZCC_LOG_BPXLST_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXLCO_ERR_ID
#define ZCC_LOG_METTLE_BPXLCO_ERR_ID                ZCC_LOG_MSG_PRFX"2836S"
#endif
#define ZCC_LOG_METTLE_BPXLCO_ERR_TEXT              "BPXLCO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXLCO_ERR                   ZCC_LOG_METTLE_BPXLCO_ERR_ID" "ZCC_LOG_METTLE_BPXLCO_ERR_TEXT

#ifndef ZCC_LOG_BPXLCO_ERR_ID
#define ZCC_LOG_BPXLCO_ERR_ID                       ZCC_LOG_MSG_PRFX"2837S"
#endif
#define ZCC_LOG_BPXLCO_ERR_TEXT                     "BPXLCO FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXLCO_ERR                          ZCC_LOG_BPXLCO_ERR_ID" "ZCC_LOG_BPXLCO_ERR_TEXT

#ifndef ZCC_LOG_BPXLCO_MSG_ID
#define ZCC_LOG_BPXLCO_MSG_ID                       ZCC_LOG_MSG_PRFX"2838I"
#endif
#define ZCC_LOG_BPXLCO_MSG_TEXT                     "BPXLCO (%s) OK: returnValue: %d\n\n"
#define ZCC_LOG_BPXLCO_MSG                          ZCC_LOG_BPXLCO_MSG_ID" "ZCC_LOG_BPXLCO_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXOPD_ERR_ID
#define ZCC_LOG_METTLE_BPXOPD_ERR_ID                ZCC_LOG_MSG_PRFX"2839S"
#endif
#define ZCC_LOG_METTLE_BPXOPD_ERR_TEXT              "BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXOPD_ERR                   ZCC_LOG_METTLE_BPXOPD_ERR_ID" "ZCC_LOG_METTLE_BPXOPD_ERR_TEXT

#ifndef ZCC_LOG_BPXOPD_ERR_ID
#define ZCC_LOG_BPXOPD_ERR_ID                       ZCC_LOG_MSG_PRFX"2840S"
#endif
#define ZCC_LOG_BPXOPD_ERR_TEXT                     "BPXOPD (%s) FAILED: fd: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXOPD_ERR                          ZCC_LOG_BPXOPD_ERR_ID" "ZCC_LOG_BPXOPD_ERR_TEXT

#ifndef ZCC_LOG_BPXOPD_MSG_ID
#define ZCC_LOG_BPXOPD_MSG_ID                       ZCC_LOG_MSG_PRFX"2841I"
#endif
#define ZCC_LOG_BPXOPD_MSG_TEXT                     "BPXOPD (%s) OK: fd: %d\n"
#define ZCC_LOG_BPXOPD_MSG                          ZCC_LOG_BPXOPD_MSG_ID" "ZCC_LOG_BPXOPD_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXRDD_ERR_ID
#define ZCC_LOG_METTLE_BPXRDD_ERR_ID                ZCC_LOG_MSG_PRFX"2842S"
#endif
#define ZCC_LOG_METTLE_BPXRDD_ERR_TEXT              "BPXRDD (fd=%d, path=%s) %s: %d entries read, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXRDD_ERR                   ZCC_LOG_METTLE_BPXRDD_ERR_ID" "ZCC_LOG_METTLE_BPXRDD_ERR_TEXT

#ifndef ZCC_LOG_BPXRDD_ERR_ID
#define ZCC_LOG_BPXRDD_ERR_ID                       ZCC_LOG_MSG_PRFX"2843S"
#endif
#define ZCC_LOG_BPXRDD_ERR_TEXT                     "BPXRDD (fd=%d, path=%s) %s: %d entries read, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXRDD_ERR                          ZCC_LOG_BPXRDD_ERR_ID" "ZCC_LOG_BPXRDD_ERR_TEXT

#ifndef ZCC_LOG_BPXRDD_MSG_ID
#define ZCC_LOG_BPXRDD_MSG_ID                       ZCC_LOG_MSG_PRFX"2844I"
#endif
#define ZCC_LOG_BPXRDD_MSG_TEXT                     "BPXRDD (fd=%d, path=%s) OK: %d entries read\n"
#define ZCC_LOG_BPXRDD_MSG                          ZCC_LOG_BPXRDD_MSG_ID" "ZCC_LOG_BPXRDD_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXCLD_ERR_ID
#define ZCC_LOG_METTLE_BPXCLD_ERR_ID                ZCC_LOG_MSG_PRFX"2845S"
#endif
#define ZCC_LOG_METTLE_BPXCLD_ERR_TEXT              "BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXCLD_ERR                   ZCC_LOG_METTLE_BPXCLD_ERR_ID" "ZCC_LOG_METTLE_BPXCLD_ERR_TEXT

#ifndef ZCC_LOG_BPXCLD_ERR_ID
#define ZCC_LOG_BPXCLD_ERR_ID                       ZCC_LOG_MSG_PRFX"2846S"
#endif
#define ZCC_LOG_BPXCLD_ERR_TEXT                     "BPXCLD (fd=%d, path=%s) FAILED: returnValue: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXCLD_ERR                          ZCC_LOG_BPXCLD_ERR_ID" "ZCC_LOG_BPXCLD_ERR_TEXT

#ifndef ZCC_LOG_BPXCLD_MSG_ID
#define ZCC_LOG_BPXCLD_MSG_ID                       ZCC_LOG_MSG_PRFX"2847I"
#endif
#define ZCC_LOG_BPXCLD_MSG_TEXT                     "BPXCLD (fd=%d, path=%s) OK: returnValue: %d\n"
#define ZCC_LOG_BPXCLD_MSG                          ZCC_LOG_BPXCLD_MSG_ID" "ZCC_LOG_BPXCLD_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXMKD_ERR_ID
#define ZCC_LOG_METTLE_BPXMKD_ERR_ID                ZCC_LOG_MSG_PRFX"2848S"
#endif
#define ZCC_LOG_METTLE_BPXMKD_ERR_TEXT              "BPXMKD (%s, 0%o) FAILED: returnVal: %d, returnCode: %d, reasonCode: 0x%08x\n"
#define ZCC_LOG_METTLE_BPXMKD_ERR                   ZCC_LOG_METTLE_BPXMKD_ERR_ID" "ZCC_LOG_METTLE_BPXMKD_ERR_TEXT

#ifndef ZCC_LOG_BPXMKD_ERR_ID
#define ZCC_LOG_BPXMKD_ERR_ID                       ZCC_LOG_MSG_PRFX"2849S"
#endif
#define ZCC_LOG_BPXMKD_ERR_TEXT                     "BPXMKD (%s, 0%o) FAILED: returnVal: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXMKD_ERR                          ZCC_LOG_BPXMKD_ERR_ID" "ZCC_LOG_BPXMKD_ERR_TEXT

#ifndef ZCC_LOG_BPXMKD_MSG_ID
#define ZCC_LOG_BPXMKD_MSG_ID                       ZCC_LOG_MSG_PRFX"2850I"
#endif
#define ZCC_LOG_BPXMKD_MSG_TEXT                     "BPXMKD (%s,) OK: returnValue: %d\n"
#define ZCC_LOG_BPXMKD_MSG                          ZCC_LOG_BPXMKD_MSG_ID" "ZCC_LOG_BPXMKD_MSG_TEXT

#ifndef ZCC_LOG_METTLE_BPXRMD_ERR_ID
#define ZCC_LOG_METTLE_BPXRMD_ERR_ID                ZCC_LOG_MSG_PRFX"2851S"
#endif
#define ZCC_LOG_METTLE_BPXRMD_ERR_TEXT              "BPXRMD (%s) FAILED: returnVal: %d\n"
#define ZCC_LOG_METTLE_BPXRMD_ERR                   ZCC_LOG_METTLE_BPXRMD_ERR_ID" "ZCC_LOG_METTLE_BPXRMD_ERR_TEXT

#ifndef ZCC_LOG_BPXRMD_ERR_ID
#define ZCC_LOG_BPXRMD_ERR_ID                       ZCC_LOG_MSG_PRFX"2852S"
#endif
#define ZCC_LOG_BPXRMD_ERR_TEXT                     "BPXRMD (%s) FAILED: returnVal: %d, returnCode: %d, reasonCode: 0x%08x, strError: (%s)\n"
#define ZCC_LOG_BPXRMD_ERR                          ZCC_LOG_BPXRMD_ERR_ID" "ZCC_LOG_BPXRMD_ERR_TEXT

#ifndef ZCC_LOG_BPXRMD_MSG_ID
#define ZCC_LOG_BPXRMD_MSG_ID                       ZCC_LOG_MSG_PRFX"2853I"
#endif
#define ZCC_LOG_BPXRMD_MSG_TEXT                     "BPXRMD (%s) OK: returnValue: %d\n"
#define ZCC_LOG_BPXRMD_MSG                          ZCC_LOG_BPXRMD_MSG_ID" "ZCC_LOG_BPXRMD_MSG_TEXT


#endif /* MVD_H_ZCCLOGGING_H_ */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
