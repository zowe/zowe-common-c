

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

/**
 * @file radmin.h
 * @brief Function declarations and data structures for R_admin wrapper code.
 */

#ifndef COMMON_H_RADMIN_H_
#define COMMON_H_RADMIN_H_

#ifdef METTLE
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

#include "zowetypes.h"
#include "zos.h"


/*
 * System mappings. These need to be in sync with the official doc.
 * Please refer to z/OS Security Server RACF Callable Services, chapter 2,
 * R_admin (IRRSEQ00): RACF administration API for more details.
 */

ZOWE_PRAGMA_PACK

typedef struct RadminVLText_tag {
  uint16_t length;
  char text[0];
} RadminVLText;

typedef struct RadminUserID_tag {
  uint8_t length;
  char value[8];
} RadminUserID;

/* R_admin run RACF command function */

#define RADMIN_CMD_MAX_COMMAND_LENGTH     4094

typedef struct RadminRACFCommand_tag {
  uint16_t length;
  char text[RADMIN_CMD_MAX_COMMAND_LENGTH];
} RadminRACFCommand;

typedef struct RadminCommandOutput_tag {
  struct RadminCommandOutput_tag * __ptr32 next;
  char eyecatcher[4];
#define RADMIN_COMMAND_OUTPUT_EYECATCHER_CMD  "RCMD"
#define RADMIN_COMMAND_OUTPUT_EYECATCHER_MSG  "RMSG"
  int8_t subpool;
  uint32_t totalLength : 24;
  uint32_t lastByteOffset;
  RadminVLText firstMessageEntry;
} RadminCommandOutput;

/* R_admin extract function */

typedef struct RadminXTRHeader_tag {
  char      eyecatcher[4];
  uint32_t  totalLength;
  int8_t    subpool;
  int8_t    version;
  char      reserved0[2];
  char      className[8];
  uint32_t  profileNameLength;
  char      reserved1[8];
  char      reserved2[4];
  int32_t   flag;
#define RADMIN_XTR_HDR_FLAG_BYPASS_AUTH_CHECK         0x80000000
#define RADMIN_XTR_HDR_FLAG_BASE_SEG_ONLY             0x40000000
#define RADMIN_XTR_HDR_FLAG_ENFORCE_FACILITY_CHECK    0x20000000
#define RADMIN_XTR_HDR_FLAG_NO_EXACT_MATCH            0x10000000
#define RADMIN_XTR_HDR_FLAG_GENERIC                   0x10000000
#define RADMIN_XTR_HDR_FLAG_UPPERCASE_INPUT           0x08000000
#define RADMIN_XTR_HDR_FLAG_PROFILE_NAME_ONLY         0x04000000
  uint32_t  segmentCount;
  char      reserved3[16];
} RadminXTRHeader;

#define RADMIN_XTR_MAX_PROFILE_LENGTH       246

typedef struct  RadminXTRParmList_tag {
  struct RadminXTRHeader_tag header;
  char profileName[RADMIN_XTR_MAX_PROFILE_LENGTH];
} RadminXTRParmList;

typedef RadminXTRParmList RadminXTROutput;

typedef struct RadminXTRSegmentDescriptor_tag {
  char      name[8];
  int32_t   flag;
  uint32_t  fieldCount;
  char      reserved0[4];
  uint32_t  firstFieldOffset;
  char      reserved1[16];
} RadminXTRSegmentDescriptor;

typedef struct RadminXTRFieldDescriptor_tag {
  char      name[8];
  int16_t   fieldType;
#define RADMIN_XTR_FIELD_TYPE_REPEAT_GRP_MEMBER     0x8000
#define RADMIN_XTR_FIELD_TYPE_FESERVED              0x4000
#define RADMIN_XTR_FIELD_TYPE_BOOLEAN               0x2000
#define RADMIN_XTR_FIELD_TYPE_REPEAT_FIELD_HEADER   0x1000
  char      reserved0[2];
  int32_t   flag;
#define RADMIN_XTR_FIELD_FLAG_BOOLEAN_VALUE         0x80000000
#define RADMIN_XTR_FIELD_FLAG_OUTPUT_ONLY           0x40000000
  union {
    uint32_t  repeatGroupCount;
    uint32_t  dataLength;
  };
  char      reserved1[4];
  union {
    uint32_t  subfieldCount;
    uint32_t  dataOffset;
  };
  char      reserved2[16];
} RadminXTRFieldDescriptor;

typedef struct RadminField_tag {
  char name[8];
  int8_t flag;
/*
 * Y flag:
 * - Boolean: Set the value to TRUE
 * - Text: Create or modify the field with the specified data
 * - Repeating: Create or replace the field with the specified data
 * A flag:
 * - Boolean: N/A
 * - Text: N/A
 * - Repeating: Add the specified data to the existing data, if any
 * D flag:
 * - Boolean: N/A
 * - Text: N/A
 * - Repeating: Delete the specified data from the existing data
 * N flag:
 * - Boolean: Set the value to FALSE
 * - Text: For alter functions, delete the field. Otherwise, N/A.
 * - Repeating: Remove all data from the field
 * I flag:
 * - All: Ignore the field entry
 */
#define RADMIN_FIELD_FLAG_Y     'Y'
#define RADMIN_FIELD_FLAG_A     'A'
#define RADMIN_FIELD_FLAG_D     'D'
#define RADMIN_FIELD_FLAG_N     'N'
#define RADMIN_FIELD_FLAG_I     'I'
  uint16_t dataLength;
  char data[0];
} RadminField;

typedef struct RadminSegment_tag {
  char name[8];
  int8_t flag;
/*
 * Y flag:
 * - For add functions, create the segment
 * - For alter functions, create or modify the segment
 * - For list functions, display the segment
 * N flag:
 * - For add functions, do not create the segment (This usage is optional.
 * You can simply omit the segment entry.)
 * - For alter functions, delete the segment
 * I flag:
 * - For add and alter functions, ignore this segment entry and any field
 * entries within it
 * - For resource list functions, ignore this segment entry (do not display
 * the segment)
 */
#define RADMIN_SEGMENT_FLAG_Y   'Y'
#define RADMIN_SEGMENT_FLAG_N   'N'
#define RADMIN_SEGMENT_FLAG_I   'I'
  uint16_t fieldCount;
  RadminField firstField[0];
} RadminSegment;

typedef struct RadminResParmList_tag {
  uint8_t classNameLength;
  char className[8];
  int8_t flags;
#define RADMIN_RES_FLAG_NORUN       0x80
#define RADMIN_RES_FLAG_RETCMD      0x40
#define RADMIN_RES_FLAG_ADDQUOT     0x20
  uint16_t errorEntryOffset;
  uint16_t segmentCount;
  RadminSegment segmentData[0];
} RadminResParmList;

typedef struct RadminGroupParmList_tag {
  uint8_t groupNameLength;
  char groupName[8];
  int8_t flags;
#define RADMIN_GROUP_FLAG_NORUN     0x80
#define RADMIN_GROUP_FLAG_RETCMD    0x40
#define RADMIN_GROUP_FLAG_ADDQUOT   0x20
  uint16_t errorEntryOffset;
  uint16_t segmentCount;
  RadminSegment segmentData[0];
} RadminGroupParmList;

typedef struct RadminConnectionParmList_tag {
  uint8_t userIDLength;
  char userID[8];
  int8_t flags;
#define RADMIN_CONNECTION_FLAG_NORUN        0x80
#define RADMIN_CONNECTION_FLAG_RETCMD       0x40
#define RADMIN_CONNECTION_FLAG_ADDQUOT      0x20
  uint16_t errorEntryOffset;
  uint16_t segmentCount;
  RadminSegment segmentData[0];
} RadminConnectionParmList;

/*
 * General wrapper data structures and definitions.
 */

/**
 * @brief Struct used to pass a caller's authorization information to R_admin.
 *
 * The following list shows the possible sources of the user ID or ACEE, in
 * the order in which they are searched:
 * 1. The userID parameter (supervisor state callers only)
 * 2. The ACEE parameter (supervisor state callers only)
 * 3. The user ID associated with the current task control block (TCB)
 * 4. The user ID associated with the current address space (ASXB)
 *
 */
typedef struct RadminCallerAuthInfo_tag {
  ACEE * __ptr32 acee; /**< Caller's ACEE or NULL. */
  RadminUserID userID; /**< Space-padded caller's user ID or blanks. */
} RadminCallerAuthInfo;

typedef struct RadminAPIStatus_tag {
  int safRC;
  int racfRC;
  int racfRSN;
} RadminAPIStatus;

typedef struct RadminStatus_tag {
  int reasonCode;
  RadminAPIStatus apiStatus;
} RadminStatus;

#define RC_RADMIN_OK                  0
#define RC_RADMIN_INSUFF_SPACE        2
#define RC_RADMIN_NONZERO_USER_RC     4
#define RC_RADMIN_STATUS_DATA_NULL    8
#define RC_RADMIN_BAD_INPUT           9
#define RC_RADMIN_SYSTEM_ERROR        10

/*
 * Specific wrapper data structures and definitions.
 */

#pragma map(radminExtractUserProfiles, "RADMXUP")
#pragma map(radminExtractBasicUserProfileInfo, "RADMXBUP")
#pragma map(radminExtractBasicGenresProfileInfo, "RADMXBRP")
#pragma map(radminExtractBasicGroupProfileInfo, "RADMXBGP")
#pragma map(radminExtractGenresAccessList, "RADMXPAL")
#pragma map(radminExtractGroupAccessList, "RADMXGAL")
#pragma map(radminPerformResAction, "RADMPREA")
#pragma map(radminPerformGroupAction, "RADMPGRA")
#pragma map(radminPerformConnectionAction, "RADMPCNA")
#pragma map(radminRunRACFCommand, "RADMRCMD")

#define RADMIN_XTR_MAX_USER_ID_LENGTH       8
#define RADMIN_XTR_MAX_GROUP_LENGTH         8
#define RADMIN_XTR_MAX_CONNECTION_LENGTH    17
#define RADMIN_XTR_MAX_GENRES_LENGTH        246
#define RADMIN_XTR_MAX_CLASS_LENGTH         8
#define RADMIN_XTR_MAX_GROUP_LENGTH         8

typedef enum RadminProfileType_tag {
  RADMIN_PROFILE_TYPE_USER,
  RADMIN_PROFILE_TYPE_GROUP,
  RADMIN_PROFILE_TYPE_CONNECTION,
  RADMIN_PROFILE_TYPE_GENRES
} RadminProfileType;

typedef int (RadminProfileVisitor)(const RadminXTROutput *group,
                                   void *userData);

/**
 * @brief A generic function for extracting RACF profiles.
 *
 * The profiles are iterated in alphabetical order. When the function is
 * used to iterate general resources, the discrete one are returned first,
 * followed by the generic profiles.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param type The type of the profile to be extracted, see the
 * RadminProfileType enum.
 * @param profileName The first profile used for iteration. Use NULL to iterate
 * from the beginning.
 * @param className The class name used to extract general resource profiles.
 * For other types can be NULL.
 * @param profilesToExtract The number of profiles to iterate over (extract).
 * @param profileVisitor The visitor function called for each profile entry.
 * It must not be NULL.
 * @param userData The data passed to the visitor function.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractProfiles(
    RadminCallerAuthInfo callAuthInfo,
    RadminProfileType type,
    const char *profileName,
    const char *className,
    size_t profilesToExtract,
    RadminProfileVisitor *profileVisitor,
    void *userData,
    RadminStatus *status
);


typedef struct RadminBasicUserPofileInfo_tag {
  char userID[8];
  char defaultGroup[8];
  char name[20];
} RadminBasicUserPofileInfo;


/**
 * @brief The function retrieves basic user profile information into
 * a user-provided buffer.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param startUserID The first user ID used for iteration. Use NULL to iterate
 * from the beginning.
 * @param profilesToExtract The number of profiles to iterate over (extract).
 * @param result User-provided buffer for storing extracted profile info.
 * It must not be NULL.
 * @param profilesExtracted The number of profiles returned in @p result.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractBasicUserProfileInfo(
    RadminCallerAuthInfo callAuthInfo,
    const char *startUserID,
    size_t profilesToExtract,
    RadminBasicUserPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
);

typedef struct RadminBasicGenresPofileInfo_tag {
  char profile[RADMIN_XTR_MAX_GENRES_LENGTH];
  char owner[8];
} RadminBasicGenresPofileInfo;

/**
 * @brief The function retrieves basic general resource information into
 * a user-provided buffer.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param class Class from which the profiles are to be extracted.
 * @param startProfile The first profile used for iteration. Use NULL to iterate
 * from the beginning.
 * @param profilesToExtract The number of profiles to iterate over (extract).
 * @param result User-provided buffer for storing extracted profile info.
 * It must not be NULL.
 * @param profilesExtracted The number of profiles returned in @p result.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractBasicGenresProfileInfo(
    RadminCallerAuthInfo callAuthInfo,
    const char *class,
    const char *startProfile,
    size_t profilesToExtract,
    RadminBasicGenresPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
);

typedef struct RadminBasicGroupPofileInfo_tag {
  char group[RADMIN_XTR_MAX_GROUP_LENGTH];
  char owner[8];
  char superiorGroup[8];
} RadminBasicGroupPofileInfo;

/**
 * @brief The function retrieves basic group information into
 * a user-provided buffer.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param startProfile The first profile used for iteration. Use NULL to iterate
 * from the beginning.
 * @param profilesToExtract The number of profiles to iterate over (extract).
 * @param result User-provided buffer for storing extracted profile info.
 * It must not be NULL.
 * @param profilesExtracted The number of profiles returned in @p result.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractBasicGroupProfileInfo(
    RadminCallerAuthInfo callAuthInfo,
    const char *startProfile,
    size_t profilesToExtract,
    RadminBasicGroupPofileInfo *result,
    size_t *profilesExtracted,
    RadminStatus *status
);

typedef struct RadminAccessListEntry_tag {
  char id[8];
  char accessType[8];
} RadminAccessListEntry;

/**
 * @brief The function extracts the access list of a certain profile.
 *
 * All parameters listed must be provided.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param class The class of the profile to used.
 * @param profile The profile used for access list retrieval.
 * @param result User-provided buffer for storing extracted access list/
 * @param resultCapacity The capacity of the user-provided buffer.
 * @param accessListSize The number of access list entries retrieved.
 * @param status This data structure is filled with status information in case
 * of a failure.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractGenresAccessList(
    RadminCallerAuthInfo callAuthInfo,
    const char *class,
    const char *profile,
    RadminAccessListEntry *result,
    size_t resultCapacity,
    size_t *accessListSize,
    RadminStatus *status
);

/**
 * @brief The function extracts the access list of a certain group.
 *
 * All parameters listed must be provided.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param group The group used for access list retrieval.
 * @param result User-provided buffer for storing extracted access list/
 * @param resultCapacity The capacity of the user-provided buffer.
 * @param accessListSize The number of access list entries retrieved.
 * @param status This data structure is filled with status information in case
 * of a failure.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminExtractGroupAccessList(
    RadminCallerAuthInfo callAuthInfo,
    const char *group,
    RadminAccessListEntry *result,
    size_t resultCapacity,
    size_t *accessListSize,
    RadminStatus *status
);

#define RSN_RADMIN_XTR_PROFILE_TOO_LONG     1
#define RSN_RADMIN_XTR_CLASS_TOO_LONG       2
#define RSN_RADMIN_XTR_VISITOR_NULL         3
#define RSN_RADMIN_XTR_RESULT_NULL          4
#define RSN_RADMIN_XTR_PROFILES_XTRED_NULL  5
#define RSN_RADMIN_XTR_CLASS_NULL           6
#define RSN_RADMIN_XTR_PROFILE_NULL         7

typedef int (RadminResResultHandler)(RadminAPIStatus status,
                                     const RadminCommandOutput *result,
                                     void *userData);

typedef enum RadminResAction_tag {
  RADMIN_RES_ACTION_NA,
  RADMIN_RES_ACTION_ADD_GENRES,
  RADMIN_RES_ACTION_DELETE_GENRES,
  RADMIN_RES_ACTION_ALTER_GENRES,
  RADMIN_RES_ACTION_LIST_GENRES,
  RADMIN_RES_ACTION_ADD_DS,
  RADMIN_RES_ACTION_DELETE_DS,
  RADMIN_RES_ACTION_ALTER_DS,
  RADMIN_RES_ACTION_LIST_DS,
  RADMIN_RES_ACTION_PERMIT,
  RADMIN_RES_ACTION_PROHIBIT
} RadminResAction;

/**
 * @brief The function performs general resource actions and calls
 * a user-provided callback function.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param action Action to be performed. See the RadminAction enum.
 * @param actionParmList Parm list used to pass action related information.
 * See z/OS Security Server RACF Callable Services for details on how to build
 * a correct parm list.
 * @param userHandler User-provided callback function.
 * @param userHandlerData Data passed to the user handler.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminPerformResAction(
    RadminCallerAuthInfo callAuthInfo,
    RadminResAction action,
    RadminResParmList * __ptr32 actionParmList,
    RadminResResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
);

#define RSN_RADMIN_RES_PARMLIST_NULL      1
#define RSN_RADMIN_RES_USER_HANDLER_NULL  2
#define RSN_RADMIN_RES_UNKNOWN_ACTION     3

typedef int (RadminGroupResultHandler)(RadminAPIStatus status,
                                       const RadminCommandOutput *result,
                                       void *userData);

typedef enum RadminGroupAction_tag {
  RADMIN_GROUP_ACTION_NA,
  RADMIN_GROUP_ACTION_ADD_GROUP,
  RADMIN_GROUP_ACTION_DELETE_GROUP,
  RADMIN_GROUP_ACTION_ALTER_GROUP,
  RADMIN_GROUP_ACTION_LIST_GROUP
} RadminGroupAction;

/**
 * @brief The function performs group actions and calls
 * a user-provided callback function.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param action Action to be performed. See the RadminGroupAction enum.
 * @param actionParmList Parm list used to pass action related information.
 * See z/OS Security Server RACF Callable Services for details on how to build
 * a correct parm list.
 * @param userHandler User-provided callback function.
 * @param userHandlerData Data passed to the user handler.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminPerformGroupAction(
    RadminCallerAuthInfo callAuthInfo,
    RadminGroupAction action,
    RadminGroupParmList * __ptr32 actionParmList,
    RadminGroupResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
);

#define RSN_RADMIN_GROUP_PARMLIST_NULL            1
#define RSN_RADMIN_GROUP_USER_HANDLER_NULL        2
#define RSN_RADMIN_GROUP_UNKNOWN_ACTION           3

typedef int (RadminConnectionResultHandler)(
    RadminAPIStatus status,
    const RadminCommandOutput *result,
    void *userData
);

typedef enum RadminConnectionAction_tag {
  RADMIN_CONNECTION_ACTION_NA,
  RADMIN_CONNECTION_ACTION_ADD_CONNECTION,
  RADMIN_CONNECTION_ACTION_REMOVE_CONNECTION
} RadminConnectionAction;

/**
 * @brief The function performs group connection actions and calls
 * a user-provided callback function.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param action Action to be performed. See the RadminConnectionAction
 * enum.
 * @param actionParmList Parm list used to pass action related information.
 * See z/OS Security Server RACF Callable Services for details on how to build
 * a correct parm list.
 * @param userHandler User-provided callback function.
 * @param userHandlerData Data passed to the user handler.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminPerformConnectionAction(
    RadminCallerAuthInfo callAuthInfo,
    RadminConnectionAction action,
    RadminConnectionParmList * __ptr32 actionParmList,
    RadminConnectionResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
);

#define RSN_RADMIN_CONNECTION_PARMLIST_NULL       1
#define RSN_RADMIN_CONNECTION_USER_HANDLER_NULL   2
#define RSN_RADMIN_CONNECTION_UNKNOWN_ACTION      3

typedef int (RadminCommandResultHandler)(RadminAPIStatus status,
                                         const RadminCommandOutput *result,
                                         void *userData);
/**
 * @brief The function executes the specified RACF command.
 *
 * @param callAuthInfo This structure should contain a space-padded user ID,
 * an ACEE, both or none. They're the RACF call authorization.
 * @param command Command to be executed.
 * @param userHandler User-provided callback function.
 * @param userHandlerData Data passed to the user handler.
 * @param status This data structure is filled with status information in case
 * of a failure. It must not be NULL.
 *
 * @return One of the RC_RADMIN_xx return codes.
 */
int radminRunRACFCommand(
    RadminCallerAuthInfo callAuthInfo,
    const char *command,
    RadminCommandResultHandler *userHandler,
    void *userHandlerData,
    RadminStatus *status
);

#define RSN_RADMIN_CMD_COMMAND_NULL       1
#define RSN_RADMIN_CMD_COMMAND_TOO_LONG   2
#define RSN_RADMIN_CMD_USER_HANDLER_NULL  3

ZOWE_PRAGMA_PACK_RESET

#endif /* COMMON_H_RADMIN_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

