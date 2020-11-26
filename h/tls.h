/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __TLS_H__
#define __TLS_H__

#ifdef METTLE
#error Metal C not supported
#endif // METTLE

#ifdef USE_RS_SSL
#error ZOWE TLS incompatible with RS SSL
#endif // USE_RS_SSL

#include <stdbool.h>
#include <gskssl.h>

typedef struct TlsSettings_tag {
  // certificate collection, one of:
  // - SAF key ring, e.g. user_id/key_ring_name or key_ring_name
  // - unix file path to gskkyman key database, e.g. /path/to/db.kdb
  // - PKCS #11 token in form of *TOKEN*/token_name, e.g. *TOKEN*/my.pkcs11.token
  // - unix file path to PKCS #12 file, e.g. /path/to/cert.p12
  char *keyring;
  // unix file path of stash file with password for .kdb or .p12 file
  char *stash;
  // password for .kdb or .p12 file
  char *password;
  // certificate label
  char *label;
  // candidate ciphers, list of 4-digit hexadecimal cipher specs, e.g. 000A000D001000130016
  // https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0/com.ibm.zos.v2r1.gska100/csdcwh.htm
  char *ciphers;
} TlsSettings;

typedef struct TlsEnvironment_tag {
  gsk_handle envHandle;
  TlsSettings *settings;
} TlsEnvironment;

typedef struct TlsSocket_tag {
  gsk_handle socketHandle;
} TlsSocket;

int tlsInit(TlsEnvironment **outEnv, TlsSettings *settings);
int tlsDestroy(TlsEnvironment *env);
int tlsSocketInit(TlsEnvironment *env, TlsSocket **outSocket, int fd, bool isServer);
int tlsSocketClose(TlsSocket *socket);
int tlsRead(TlsSocket *socket, const char *buf, int size, int *outLength);
int tlsWrite(TlsSocket *socket, const char *buf, int size, int *outLength);
const char *tlsStrError(int rc);

#define TLS_ALLOC_ERROR (-1)

#endif // __TLS_H__

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/