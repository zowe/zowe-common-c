

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_UNIXFILE__
#define __ZOWE_UNIXFILE__

#include "zowetypes.h"

#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#endif

#ifndef __ZOWE_OS_ZOS
#include <stdio.h>
#endif

#ifdef __ZOWE_OS_WINDOWS
#include <windows.h>
#endif

/*
  "Unix" files are regular character-oriented files that
  have on Unix the classic fopen/fread/fwrite/fclose/etc API.
  They are emumalated on other platforms.
 */


typedef struct UnixFileStream_tag {
  char *pathname;
  int fd;
  int bufferSize;
  int bufferFill;
  int bufferPos;
  int eofKnown; /* could be at EOF and not know it yet.  0 bytes left, but not tested yet */
  char *buffer;
  int isDirectory;
#ifndef __ZOWE_OS_ZOS
  FILE *internalFile;
#endif
#ifdef __ZOWE_OS_WINDOWS
  HANDLE          hFind;
  WIN32_FIND_DATA findData;
  int  hasMoreEntries;
# endif
#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
  DIR  *dir;
#endif
} UnixFile;

#ifdef __ZOWE_OS_ZOS

#define BPXOPN_OPTION_NOLARGEFILE       0x00000800
#define BPXOPN_OPTION_LARGEFILE         0x00000400 /* ignored */
#define BPXOPN_OPTION_ASYNC_SIGNALS     0x00000200
#define BPXOPN_OPTION_FORCE_SYNC_UPDATE 0x00000100
#define BPXOPN_OPTION_CREATE_IF_NON_EXISTENT 0x000000C0 /* causes failure if file already exists */
#define BPXOPN_OPTION_CREATE                 0x00000080
#define BPXOPN_OPTION_EXCLUSIVE              0x00000040
#define BPXOPN_OPTION_NOT_CONTROLLING_TTY    0x00000020
#define BPXOPN_OPTION_TRUNCATE               0x00000010
#define BPXOPN_OPTION_APPEND                 0x00000008 /* set offset to EOF on write */
#define BPXOPN_OPTION_NON_BLOCKING           0x00000004 /* you better be a good unixer and MVS-er if you set this */
#define BPXOPN_OPTION_READ_WRITE             0x00000003
#define BPXOPN_OPTION_READ_ONLY              0x00000002
#define BPXOPN_OPTION_WRITE_ONLY             0x00000001

#define BPXOPN_MODE_SET_UID_ON_EXEC              0x0800
#define BPXOPN_MODE_SET_GID_ON_EXEC              0x0400
#define BPXOPN_MODE_ISVTX                        0x0200  /* ?, read the book, jeez */
#define BPXOPN_MODE_USER_READ_PERMISSION         0x0100
#define BPXOPN_MODE_USER_WRITE_PERMISSION        0x0080
#define BPXOPN_MODE_USER_SEARCH_EXEC_PERMISSION  0x0040
#define BPXOPN_MODE_GROUP_READ_PERMISSION        0x0020
#define BPXOPN_MODE_GROUP_WRITE_PERMISSION       0x0010
#define BPXOPN_MODE_GROUP_SEARCH_EXEC_PERMISSION 0x0008 
#define BPXOPN_MODE_OTHER_READ_PERMISSION        0x0004
#define BPXOPN_MODE_OTHER_WRITE_PERMISSION       0x0002
#define BPXOPN_MODE_OTHER_SEARCH_EXEC_PERMISSION 0x0001

#define BPXSTA_FILETYPE_DIRECTORY         1
#define BPXSTA_FILETYPE_CHARACTER_SPECIAL 2
#define BPXSTA_FILETYPE_REGULAR           3
#define BPXSTA_FILETYPE_FIFO              4
#define BPXSTA_FILETYPE_SYMLINK           5
#define BPXSTA_FILETYPE_BLOCK_SPECIAL     6
#define BPXSTA_FILETYPE_SOCKET            7

ZOWE_PRAGMA_PACK

/*
                 BPXYDIRE   ,
                 ** BPXYDIRE: Mapping of directory entry
                 **  Used By: RDD
                 * LA     RegOne,buffer            RegOne->BPX1RDD buffer and 1st DIRE
                 * USING  DIRE,RegOne              Addressability to DIRE
     DIRE                 DSECT ,
     DIRENTINFO           DS    0X     Fixed length information
     DIRENTLEN            DS    H      Entry length
     DIRENTNAML           DS    H      Name length
     DIRENTNAME           DS    0C     Name
     * LR     RegTwo,RegOne            RegTwo->DIRE
     * LA     RegTwo,4(RegTwo)         RegTwo->start of name
     * SLR    RegThree,RegThree        Clear register
     * ICM    RegThree,3,DIRENTNAML    Load name length
     * ALR    RegTwo,RegThree          RegTwo->end of name+1
     * USING  DIRENTPFSDATA,RegTwo     Addressability to DIRENTPFSDATA
     DIRENTPFSDATA        DSECT ,      Physical file system-specific data
     DIRENTPFSINO         DS    CL4    File Serial Number = st_ino
     DIRENTPFSOTHER       DS    0C     Other PFS specific data
                          ORG   DIRENTPFSDATA
     DIRENTPLUSATTR       DS    0C     ReaddirPlus Attr
     *
     * ICM    RegThree,3,DIRENTLEN     Load entry length
     * ALR    RegOne,RegThree          RegOne->Next DIRE in buffer
     * BCT    Return_Value,Back_to_process_next_DIRE
     ** BPXYDIRE End
 */

typedef struct BPXYDIRE_tag{
  short    entryLength;
  short    nameLength;
  char     name[200];   /* not really 200, it's name followed by other stuff */
} BPXYDIRE;


typedef struct BPXYSTAT_tag{
  char     eyecatcher[4];
  short    length;
  short    version;
  /* the following four, collectively, are the "mode" */
  char     fileType;
  char     flags1;
  char     flags2;
  char     flags3;
  int      inode;   /* file serial number, within what?? */
  int      deviceID;
  int      numberOfLinks;
  int      ownerUID;
  int      ownerGID;
  int64    fileSize;
  int      lastAccessTime;  /* unix seconds for these time valus */
  int      lastModficationTime;
  int      lastFileStatusChangeTime;
  short    majorNumber; /* if special */
  short    minorNumber;
  int      auditorAuditInfo;
  int      userAuditInfo;
  int      blockSize;
  int      creationTime;
  int      racfFileID[4];
  int      reserved1;
  short    ccsid;  /* 0000 no tag, FFFF binary */
  short    fileTaggingTags;
  int      reserved2[2];
  int64    blocks;
  uint32_t reservedAttributeFlags: 24;
  uint8_t  attributeFlags;
  #define BPXYSTAT_ATTR_SHARELIB    0x10
  #define BPXYSTAT_ATTR_NOSHAREAS   0x08
  #define BPXYSTAT_ATTR_APFAUTH     0x04
  #define BPXYSTAT_ATTR_PROGCTL     0x02
  #define BPXYSTAT_ATTR_EXTLINK     0x01
  /* 0x10 shared lib 
     0x08 no shareas
     0x04 APF
     0x02 Prog Control
     0x01 SymLink
     */
  int      referenceTime;
  int      fileID[2];
  char     fileFormat;
  char     ifspFlag2;  /* ACL suport */
  char     reserved3[2];
  int      microSecondsOfFullCTime;
  char     securityLabel[8];
  char     reserved4[4];
  /* end of ver 1 */
  int64    accessTime2;
  int64    modificationTime2;
  int64    fileCreationTime2;
  int64    referenceTime2;
  int64    reserved5;
  char     reserved6[16];
} BPXYSTAT;

/* BPXYATT FLAG 1 */
#define ATTMODECHG            0x80      /* Change mode */
#define ATTOWNERCHG           0X40      /* Change owner */
#define ATTSETGEN             0X20      /* Set general attributes */
#define ATTTRUNC              0X10      /* Truncate size */
#define ATTATIMECHG           0x08      /* Change the Atime */
#define ATTATIMETOD           0X04      /* Change the Atime to the Current Time */
#define ATTMTIMECHG           0X02      /* Change the Mtime */
#define ATTMTIMETOD           0X01      /* Change the Mtime to the Current Time */

/* BPXYATT FLAG 2 */
#define ATTMAAUDIT            0x80      /* Modify auditor audit info */
#define ATTMUAUDIT            0X40      /* Modify user audit info */
#define ATTCTIMECHG           0X20      /* Change the Ctime */
#define ATTCTIMETOD           0X10      /* Change the Ctime to the Current Time */
#define ATTREFTIMECHG         0x08      /* Change the RefTime */
#define ATTREFTIMETOD         0X04      /* Change the RefTime to the Current Time */
#define ATTFILEFMTCHG         0X02      /* Change file format */

/* BPXYATT FLAG 3 */
#define ATTCHARSETIDCHG       0X40      /* Change file tag */
#define ATTLP64TIMES          0X20      /* Use 64-bit times */
#define ATTSECLABELCHG        0X10      /* Set seclabel */

/* BPXYATT ATTGENMASK FLAGS */
#define ATTNODELFILESMASK     0X20      /* Files should not be deleted */
#define ATTSHARELIBMASK       0X10      /* Shared library */
#define ATTNOSHAREASMASK      0x08      /* No shereas flag */
#define ATTAPFAUTHMASK        0X04      /* APF authorized flag */
#define ATTPROGCTLMASK        0X02      /* Program controlled flag */

/* BPXYATT ATTGEN VALUE */
#define ATTNODELFILES         0X20      /* Files should not be deleted */
#define ATTSHARELIB           0X10      /* Shared library */
#define ATTNOSHAREAS          0x08      /* No shereas flag */
#define ATTAPFAUTH            0X04      /* APF authorized flag */
#define ATTPROGCTL            0X02      /* Program controlled flag */

/* FILE TAGS */
#define TAG_ON_FIRST_WRITE    0x4000    /* File is tagged on first write */
#define FILE_PURE_TEXT        0x8000    /* File is pure text data */

typedef struct BPXYATT_tag {
  char    eyecatcher[4];
  int     version:16;
  char    reserved1[2];
  int     flag1:8;
  int     flag2:8;
  int     flag3:8;
  int     flag4:8;
  /* START OF BPXYMODE */
  int     fileType:8;
  int     modeFlag1:8;
  int     modeFlag2:8;
  int     modeFlag3:8;
  /* End of BPXYMODE */
  int     ownerUID;
  int     ownerGID;
  int     attGenMaskFlagsO:24; 
  int     attGenMaskFlagsV:8;
  int     attGenValueFlagsO:24; 
  int     attGenValueFlagsV:8;
  int     sizeFirstWord;
  int     sizeSecondWord;
  int     lastAccessTime;
  int     lastModificationTime;
  int     auditorAuditInfo;
  int     userAuditInfo;
  int     lastFileStatusChangeTime;
  int     referenceTime;
  /* End of Version 1 */
  int     fileFormat:8;
  int     reserved2:24;
  int     fileTagCCSID:16;
  int     fileTagFlags:16;
  char    reserved3[8];
  /* End of Version 2 */
  int64   accessTime2;
  int64   modificationTime2;
  int64   metadataTime;
  int64   referenceTime2;
  char    securityLabel[8];
  char    reserved4[8];
  /* End of Version 3*/
} BPXYATT;

typedef BPXYSTAT FileInfo;
typedef BPXYDIRE DirectoryEntry;
typedef BPXYATT  FileAttributes;

#define FILE_OPTION_READ_ONLY              BPXOPN_OPTION_READ_ONLY
#define FILE_OPTION_NOLARGEFILE            BPXOPN_OPTION_NOLARGEFILE
#define FILE_OPTION                        BPXOPN_OPTION_LARGEFILE
#define FILE_OPTION_ASYNC_SIGNALS          BPXOPN_OPTION_ASYNC_SIGNALS 
#define FILE_OPTION_FORCE_SYNC_UPDATE      BPXOPN_OPTION_FORCE_SYNC_UPDATE 
#define FILE_OPTION_CREATE_IF_NON_EXISTENT BPXOPN_OPTION_CREATE_IF_NON_EXISTENT
#define FILE_OPTION_CREATE                 BPXOPN_OPTION_CREATE 
#define FILE_OPTION_EXCLUSIVE              BPXOPN_OPTION_EXCLUSIVE
#define FILE_OPTION_NOT_CONTROLLING_TTY    BPXOPN_OPTION_NOT_CONTROLLING_TTY
#define FILE_OPTION_TRUNCATE               BPXOPN_OPTION_TRUNCATE 
#define FILE_OPTION_APPEND                 BPXOPN_OPTION_APPEND
#define FILE_OPTION_NON_BLOCKING           BPXOPN_OPTION_NON_BLOCKING
#define FILE_OPTION_READ_WRITE             BPXOPN_OPTION_READ_WRITE
#define FILE_OPTION_READ_ONLY              BPXOPN_OPTION_READ_ONLY
#define FILE_OPTION_WRITE_ONLY             BPXOPN_OPTION_WRITE_ONLY



ZOWE_PRAGMA_PACK_RESET

#elif defined(__ZOWE_OS_WINDOWS) /* end __ZOWE_OS_ZOS */

typedef struct _stat FileInfo;

/* Directory Entry is synthetic and works just like Unix and BPX services */
typedef struct DirectoryEntry_tag{
  short    entryLength;
  short    nameLength;
  char     name[200];   /* not really 200, it's name followed by */
} DirectoryEntry;

#define FILE_OPTION_NOLARGEFILE       0x00000800
#define FILE_OPTION_LARGEFILE         0x00000400 /* ignored */
#define FILE_OPTION_ASYNC_SIGNALS     0x00000200
#define FILE_OPTION_FORCE_SYNC_UPDATE 0x00000100
#define FILE_OPTION_CREATE_IF_NON_EXISTENT 0x000000C0
#define FILE_OPTION_CREATE                 0x00000080
#define FILE_OPTION_EXCLUSIVE              0x00000040
#define FILE_OPTION_NOT_CONTROLLING_TTY    0x00000020
#define FILE_OPTION_TRUNCATE               0x00000010
#define FILE_OPTION_APPEND                 0x00000008 /* set offset to EOF on write */
#define FILE_OPTION_NON_BLOCKING           0x00000004 /* you better be a good unixer and MVS-er if you set this */
#define FILE_OPTION_READ_WRITE             0x00000003
#define FILE_OPTION_READ_ONLY              0x00000002
#define FILE_OPTION_WRITE_ONLY             0x00000001

#elif defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)

#include <sys/stat.h>

typedef struct stat FileInfo;

/* Directory Entry is synthetic and works just like BPX services */
typedef struct DirectoryEntry_tag{
  short    entryLength;
  short    nameLength;
  char     name[200];   /* not really limited to 200 */
} DirectoryEntry;


/* #define FILE_OPTION_NOLARGEFILE    Not sure what this is, doesn't seem to exist on Linux   */
/* #define FILE_OPTION_LARGEFILE         O_LARGEFILE   Tricky to use; see man open(2) */
#define FILE_OPTION_ASYNC_SIGNALS     O_ASYNC
#define FILE_OPTION_FORCE_SYNC_UPDATE O_SYNC
#define FILE_OPTION_CREATE_IF_NON_EXISTENT (O_CREAT | O_EXCL)
#define FILE_OPTION_CREATE                 O_CREAT
#define FILE_OPTION_EXCLUSIVE              O_EXCL
#define FILE_OPTION_NOT_CONTROLLING_TTY    O_NOCTTY
#define FILE_OPTION_TRUNCATE               O_TRUNC
#define FILE_OPTION_APPEND                 O_APPEND /* set offset to EOF on write */
#define FILE_OPTION_NON_BLOCKING           O_NONBLOCK /* you better be a good unixer and MVS-er if you set this */
#define FILE_OPTION_READ_WRITE             O_RDWR
#define FILE_OPTION_READ_ONLY              O_RDONLY
#define FILE_OPTION_WRITE_ONLY             O_WRONLY

/* These OUGHT to be FILE_MODE_foo eventually. */

#define BPXOPN_MODE_USER_READ_PERMISSION          S_IRUSR
#define BPXOPN_MODE_USER_WRITE_PERMISSION         S_IWUSR
#define BPXOPN_MODE_USER_SEARCH_EXEC_PERMISSION   S_IXUSR
#define BPXOPN_MODE_GROUP_READ_PERMISSION         S_IRGRP
#define BPXOPN_MODE_GROUP_WRITE_PERMISSION        S_IWGRP
#define BPXOPN_MODE_GROUP_SEARCH_EXEC_PERMISSION  S_IXGRP
#define BPXOPN_MODE_OTHER_READ_PERMISSION         S_IROTH
#define BPXOPN_MODE_OTHER_WRITE_PERMISSION        S_IWOTH
#define BPXOPN_MODE_OTHER_SEARCH_EXEC_PERMISSION  S_IXOTH

#endif

/*
  Call setFileTrace(1) to turn on, setFileTrace(0) to turn off.
  Returns prior value.
 */
int setFileTrace(int toWhat);


/* fileOpen is a lot like fopen() but supports a few more options 
   
   the "options" are is an OR of FILE_OPTION_x listed above

   the "mode" is reserved and must be zero

   return code is -1 for fail, else the FD
   
   reason code is perhaps platform specific debugging info
 */

UnixFile *fileOpen(const char *filename, int options, int mode, int bufferSize, int *returnCode, int *reasonCode);

int fileRead(UnixFile *file, char *buffer, int desiredBytes, 
             int *returnCode, int *reasonCode);

int fileWrite(UnixFile *file, const char *buffer, int desiredBytes,
              int *returnCode, int *reasonCode);

int fileGetChar(UnixFile *file, int *returnCode, int *reasonCode);

int fileCopy(const char *existingFile, const char *newFile, int *retCode, int *resCode);

int fileRename(const char *oldFileName, const char *newFileName, int *returnCode, int *reasonCode);

int fileDelete(const char *fileName, int *returnCode, int *reasonCode);

/* FileInfo is a generic, opaque typedef that contains a data structure that can give the following 
   info about a file 

   int64 fileInfoSize(FileInfo *f);
   int   fileInfoCCSID(FileInfo *f)
   int   fileInfoIsDirectory(FileInfo *f)
*/

int fileInfo(const char *filename, FileInfo *fileInfo, int *returnCode, int *reasonCode);

/* Same as fileInfo but does not follow symbolic link */
int symbolicFileInfo(const char *filename, FileInfo *fileInfo, int *returnCode, int *reasonCode);

#ifdef __ZOWE_OS_ZOS
int fileChangeTag(const char *fileName, int *returnCode, int *reasonCode, int ccsid);

#define F_CONTROL_CVT 13
#define F_CVT_SETCVTOFF     0      /* Set Off */
#define F_CVT_SETCVTON      1      /* Set On */
#define F_CVT_SETAUTOCVTON  2      /* Set On if AUTOCVT(YES) */
#define F_CVT_QUERYCVT      3      /* Query current mode */
#define F_CVT_SETCVTALL     4      /* Unicode Services enabled */
#define F_CVT_SETCVTAUTOALL 5      /* Unicode Services enabled if AUTOCVT(ALL) */

typedef struct F_CVT_tag {
  int cmd;
  short pccsid; /* program */
  short fccsid; /* file */
} F_CVT;

/* Actions */
#define F_SET_LOCK      6            /* Set Lock */
#define F_GET_LOCK      5            /* Get Lock */

/* Lock Type */
#define F_READ_LOCK     1            /* Read Lock */
#define F_WRITE_LOCK    2            /* Write Lock */
#define F_UNLOCK        3            /* Unlock */

/* Whence Values */
#define F_SEEK_SET      0            /* Start at beginning of file */
#define F_SEEK_CURR     1            /* Start at current location of file */
#define F_SEEK_END      2            /* Start at end of file */

/* Length Values */
#define F_WHENCE_TO_END  0            /* Locked file is from whence to end of the file */

typedef struct F_LOCK_TAG {
  short l_type;
  short l_whence;
  int64 l_start;
  int64 l_len;
  unsigned int l_pid;
} F_LOCK;

int fileDisableConversion(UnixFile *file, int *returnCode, int *reasonCode);
int fileSetLock(UnixFile *file, int *returnCode, int *reasonCode);
int fileGetLock(UnixFile *file, int *returnCode, int *reasonCode, int *isLocked);
int fileUnlock(UnixFile *file, int *returnCode, int *reasonCode);

#define USS_MAX_PATH_LENGTH 1023
#define USS_MAX_FILE_NAME   255

#endif

int fileInfoIsDirectory(const FileInfo *info);
int64 fileInfoSize(const FileInfo *info);
#if defined(__ZOWE_OS_LINUX) || defined(__ZOWE_OS_AIX)
int setFileInfoCCSID(int ccsid);
#endif
int fileInfoCCSID(const FileInfo *info);
int fileInfoUnixCreationTime(const FileInfo *info);
int fileUnixMode(const FileInfo *info);
int fileEOF(const UnixFile *file);
int fileGetINode(const FileInfo *file);
int fileGetDeviceID(const FileInfo *file);
int fileClose(UnixFile *file, int *returnCode, int *reasonCode);

int directoryMake(const char *pathName, int mode, int *returnCode, int *reasonCode);
int directoryDelete(const char *pathName, int *returnCode, int *reasonCode);
int directoryDeleteRecursive(const char *pathName, int *retCode, int *resCode);
int directoryChangeTagRecursive(const char *pathName, char *type,
          char *codepage, int recursive, char * pattern, 
          int *retCode, int *resCode);
int directoryCopy(const char *existingPathName, const char *newPathName, int *retCode, int *resCode);
int directoryRename(const char *oldDirName, const char *newDirName, int *returnCode, int *reasonCode);
int directoryChangeModeRecursive(const char *pathName, int flag,
               int mode, const char * compare, int *retCode, int *resCode);
UnixFile *directoryOpen(const char *directoryName, int *returnCode, int *reasonCode);
int directoryRead(UnixFile *directory, char *entryBuffer, int entryBufferLength, int *returnCode, int *reasonCode);
int directoryClose(UnixFile *directory, int *returnCode, int *reasonCode);

int setUmask(int mask);
int getUmask();

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

