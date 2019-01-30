

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_DISCOVERY__
#define __ZOWE_DISCOVERY__ 1

#include "crossmemory.h"

#pragma pack(packed)

typedef struct ERLYFragment_tag{
  char unmapped1[0x04];
  char eyecatcher[4];
  char unmapped2[0x4C];
  /* OFFSET 0x54 */
  char  erlymodn[8];        /* early module name */
} ERLYFragment;

#pragma pack(reset)

#define AHL "GTF Trace" 
#define AMT "MQ"
#define ANT "Copy Services (PPRC,XRC,Flash)"
#define APS "Unknown"
#define ARC "HSM"
#define ATB "APPC"
#define ATR "Resource Recovery Services"B
#define AUI "Guardium S-TAP for IMS" 
#define AUZ "Guardium S-TAP for DB2"
#define AWB "Storage Workbench" 
#define AZF "MultiFactor Auth for z/OS"
#define BLS "IPCS" 
#define BPX "USS System Services"A
#define CBC "C/C++ Compiler"
#define CBD "HCD and IODF" 
#define CBR "Something about TAPE" 
#define CEE "Language Environment"
#define CQM "Query Monitor"
#define CSQ "MQ for z/OS"
#define CSV "Contents Supervision"
#define CVA "VTOC Access" A
#define DFH "CICS"
#define DFS "IMS"
#define DSN "DB2"
#define EDC "Language Environment" 
#define EDG "RMM" 
#define ERB "RMF" 
#define EZA "TCPIP"
#define EZB "TCPIP"
#define EZY "TCPIP"
#define IAZ "JES ?" 
#define ICH "RACF"
#define IDA "VSAM" 
#define IDC "Access Method Services" 
#define IEA "ZOS System Services" 
#define IEC "ZOS System Services" 
#define IEE "ZOS System Services" 
#define IEF "ZOS System Services" 
#define IEW "Binder/Linker" 
#define IFA "Logrec ???"
#define IGD "SMS"
#define IGF "Machine Check" 
#define IGG "ICF Catalog Services" 
#define IGV "Virtual Storage Manager" 
#define IHA "Devices or SMS"
#define IKJ "TSO"
#define IKT "TSO/VTAM"
#define INM "Interactive Data Transmission Facility (what?)"
#define INT "Interactive Data Transmission Facility (what?)"
#define IOS "IO Subsystem" 
#define IRA "Unknown" 
#define IRD "FICON/ESCON Switch Stuff" 
#define IRR "SAF" 
#define IRX "REXX" 
#define ISG "Unknown" 
#define IST "VTAM"
#define ITT "Component Trace" 
#define IVT "Unknown" 
#define IWM "WLM"
#define IXC "CF"
#define IXG "Logstreams (and events??)"
#define IXL "Cross-system Extended Services" 
#define IXZ "JES-plex" 

#define SOFTWARE_TYPE_IMS      0x0001
#define SOFTWARE_TYPE_DB2      0x0002
#define SOFTWARE_TYPE_MQ       0x0004
#define SOFTWARE_TYPE_CICS     0x0008

#define SOFTWARE_TYPE_ALL_SIMPLE  0xFFFFFFFE
#define SOFTWARE_TYPE_ALL         0xFFFFFFFF

#define SOFTWARE_FLAGS_SUBSYSTEM    0x0001   /* In the MVS Sense                                */
#define SOFTWARE_FLAGS_OLTP         0x0002   /* DB2, IMS, CICS, WAS, Natural (??)               */
#define SOFTWARE_FLAGS_MONITORING   0x0004   /* OMEGAMON, MXI                                   */
#define SOFTWARE_FLAGS_ZOS_PART     0x0008   /* VTAM, PCAUTH, DUMPSRV, SMSPDSE                  */
#define SOFTWARE_FLAGS_VTAM_SERVER  0x0010   /* runs a application that you can log into        */
#define SOFTWARE_FLAGS_TCPIP_SERVER 0x0020   /* runs TCPIP services for clients                 */
#define SOFTWARE_FLAGS_XCF_PEER     0x0040   /* participates in XCF (coupling-facility) peering */

typedef struct SoftwareInstance_tag{
  char            eyecatcher[8]; /* SUBSINFO */
  uint64          type;          /* only 64 types right now, i know, i know */
  uint64          flags;         
  char           *bestName;
  char           *jobname;       /* filled if an address space */
  ASCB           *ascb;          /* filled with copy if address space */
  ShortLivedHeap *detailSLH;
  /* These 3 ONLY filled for subsystems that are on the SSCT chain, obviously */
  SSCT           *ssctCopy;
  int             ssctsuseLength;
  char           *ssctsuseCopy;
  /* End SSCT Fields */
  void           *specificData;
  struct SoftwareInstance_tag *next;
} SoftwareInstance;

#define TN3270_HAS_V6_ADDRESS 0x0001
#define TN3270_IS_TSO         0x0002
#define TN3270_IS_ISPF        0x0004

typedef struct TN3270Info_tag{
  char            eyecatcher[8];   /* "3270INFO" */
  int             flags;
  int             reserved;
  char            clientLUName[8];
  char            networkAddress[16];      /* binary */
  char            *addressString;
  char            vtamApplicationJobname[8];  /* jobname of VTAM listener   */
  int             vtamApplicationASID;        /* asid of VTAM listener      */
  int             softwareType;               /* type of software instance  */
  int             softwareSubtype;            /* Sub type optional and only unique within type */
  char            ispfPanelid[8];             /* only filled if TSO/ISPF    */
  struct TN3270Info_tag *next;
} TN3270Info;

#define MAX_ASID_ACDEBS 32

typedef struct DiscoveryASIDInfo_tag{
  int            asid;
  hashtable     *ddnameTable;     /* DDNAME to a big list */
  int            acdebCount;
  char           lunames[MAX_ASID_ACDEBS][8];
  Addr31         acdebs[MAX_ASID_ACDEBS];
} DiscoveryASIDInfo;

#define STCK_SECOND 4096000000

#define DEFAULT_SSCT_INTERVAL (300*STCK_SECOND)
#define DEFAULT_TSB_INTERVAL  (10*STCK_SECOND)

typedef struct TSBInfo_tag{
  ASCB             *ascb;
  IKJTSB           *tsb;
  IKTTSBX          *tsbx;
} TSBInfo;


typedef void SubsystemVisitor(struct DiscoveryContext_tag *context,
                              SSCT *ssctChain,
                              GDA *gda,
                              int subsystemTypeMask,
                              char *specificBestName,
                              void *visitorData);

typedef void StartedTaskVisitor(struct DiscoveryContext_tag *context,
                                ASCB *stcASCB,
                                void *visitorData);

typedef struct ZOSModel_tag {
  uint64             slowScanExpiry;
  uint64             lastSlowScan;
  ShortLivedHeap    *slowScanSLH;
  uint64             tsbScanExpiry;
  uint64             lastTSBScan;
  ShortLivedHeap    *tsbScanSLH;
  SoftwareInstance  *imsSubsystems;
  SoftwareInstance  *db2Subsystems;
  SoftwareInstance  *mqSubsystems;
  SoftwareInstance  *cicsSubsystems;
  SoftwareInstance  *wasSubsystems;

  SoftwareInstance  *softwareInstances;  /* part of the slow scan - anyhting that is NOT in the above subsystems, as yet */
  
  hashtable         *tsbTable;           /* moved here for DiscoveryContext after TSB Scan */
  
  SubsystemVisitor  *subsystemVisitor;
  StartedTaskVisitor*startedTaskVisitor;
  void *visitorsData;

  CrossMemoryServerName privilegedServerName;

} ZOSModel;

typedef struct DiscoveryContext_tag{
  char               eyecatcher[8]; /* EYECATCHER */
  ShortLivedHeap    *slh;
  SoftwareInstance  *imsSubsystems;
  SoftwareInstance  *db2Subsystems;
  SoftwareInstance  *mqSubsystems;
  SoftwareInstance  *cicsSubsystems;
  SoftwareInstance  *wasSubsystems;
  SoftwareInstance  *softwareInstances; /* anything that is not one of the above */
  TN3270Info        *tn3270Infos;
  int                ssctTraceLevel;
  int                imsTraceLevel;
  int                db2TraceLevel;
  int                cicsTraceLevel;
  int                vtamTraceLevel;
  int                tn3270TraceLevel;
  int                networkTraceLevel;
  int                addressSpaceTraceLevel;
  ZOSModel          *model;
  CrossMemoryServerName privilegedServerName;
} DiscoveryContext;

ZOSModel *makeZOSModel(CrossMemoryServerName *privilegedServerName);
ZOSModel *makeZOSModel2(CrossMemoryServerName *privilegedServerName,
                        SubsystemVisitor *subsystemVisitor,
                        StartedTaskVisitor *stcVisitor,
                        void *visitorsData);

/* The model argument allows a discovery context of faster moving data to find data relative to slower moving
   data to reduce resource consumption.
   */

DiscoveryContext *makeDiscoveryContext(ShortLivedHeap *slh, ZOSModel *model);
void discoverSubsystems(DiscoveryContext *context, int subsystemTypeMask, char *specificBestName);
void freeDiscoveryContext(DiscoveryContext *context);

SoftwareInstance *zModelGetSoftware(ZOSModel *model);
hashtable *zModelGetTSBs(ZOSModel *model);

#define SESSION_KEY_TYPE_ALL  0               /* no filtering */
#define SESSION_KEY_TYPE_ASID 1
#define SESSION_KEY_TYPE_LUNAME 2
#define SESSION_KEY_TYPE_IP4_NUMERIC 3
#define SESSION_KEY_TYPE_IP4_STRING  4
#define SESSION_KEY_TYPE_IP6_STRING  5

int findSessions(DiscoveryContext *context,
                 int               sessionKeyType,
                 int               numericKeyValue,
                 char             *stringKeyValue);


char *getSoftwareTypeName(TN3270Info *info);
char *getSoftwareSubtypeName(TN3270Info *info);

#endif




/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

