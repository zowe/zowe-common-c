/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include "alloc.h"
#include "bpxnet.h"
#include "fdpoll.h"
#include "tls.h"
#include "zos.h"

int getClientCertificate(gsk_handle soc_handle, char *clientCertificate, unsigned int clientCertificateBufferSize, unsigned int *clientCertificateLength) {

  int rc = 0;

  if (clientCertificate == NULL || clientCertificateBufferSize <= 0) {
    return -1;
  }

  memset(clientCertificate, 0, clientCertificateBufferSize);
  *clientCertificateLength = 0;

  gsk_cert_data_elem *gskCertificateArray = NULL;
  int gskCertificateArrayElementCount = 0; 

  rc = gsk_attribute_get_cert_info(soc_handle, GSK_PARTNER_CERT_INFO, &gskCertificateArray, &gskCertificateArrayElementCount);

  if (rc != 0) {
    return rc;
  }

  for (int i = 0; i < gskCertificateArrayElementCount; i++) {
    gsk_cert_data_elem *tmp = &gskCertificateArray[i];
    if (tmp->cert_data_id == CERT_BODY_DER) {
      if (clientCertificateBufferSize >= tmp->cert_data_l) {
        memcpy(clientCertificate, tmp->cert_data_p, tmp->cert_data_l);
        *clientCertificateLength = tmp->cert_data_l;
      } else {
        rc = -1; /* tls rc are all positive */
      }
      break;
    }
  }

  gsk_free_cert_data(gskCertificateArray, gskCertificateArrayElementCount);

  return rc;
}

static int isTLSV13Enabled(TlsSettings *settings) {
  ECVT *ecvt = getECVT();
  if ((ecvt->ecvtpseq > 0x1020300) && (settings->maxTls == NULL || !strcmp(settings->maxTls, "TLSv1.3"))) {
    return true;
  }
  /*
    Default to false for versions lower than 2.3 and when set to anything other than TLSV1.3.
  */
  return false;
}

int tlsInit(TlsEnvironment **outEnv, TlsSettings *settings) {
  int rc = 0;
  TlsEnvironment *env = (TlsEnvironment *)safeMalloc(sizeof(*env), "Tls Environment");
  if (!env) {
    return TLS_ALLOC_ERROR;
  }
  env->settings = settings;
  rc = rc || gsk_environment_open(&env->envHandle);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_SSLV2, GSK_PROTOCOL_SSLV2_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_SSLV3, GSK_PROTOCOL_SSLV3_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1, GSK_PROTOCOL_TLSV1_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1_1, GSK_PROTOCOL_TLSV1_1_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1_2, GSK_PROTOCOL_TLSV1_2_ON);
  /*
    We will treat not set as allowing TLSv1.3.
  */
  if (isTLSV13Enabled(settings)) {
    rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1_3, GSK_PROTOCOL_TLSV1_3_ON);
  }
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_SERVER_EPHEMERAL_DH_GROUP_SIZE, GSK_SERVER_EPHEMERAL_DH_GROUP_SIZE_2048);

#ifdef DEV_DO_NOT_VALIDATE_CLIENT_CERTIFICATES
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_CLIENT_AUTH_TYPE, GSK_CLIENT_AUTH_PASSTHRU_TYPE);
#endif

  rc = rc || gsk_attribute_set_buffer(env->envHandle, GSK_KEYRING_FILE, settings->keyring, 0);
  if (settings->stash) {
    rc = rc || gsk_attribute_set_buffer(env->envHandle, GSK_KEYRING_STASH_FILE, settings->stash, 0);
  }
  if (settings->password) {
    rc = rc || gsk_attribute_set_buffer(env->envHandle, GSK_KEYRING_PW, settings->password, 0);
  }
  rc = rc || gsk_environment_init(env->envHandle);
  if (rc == 0) {
  *outEnv = env;
  } else {
    safeFree((char*)env, sizeof(*env));
    *outEnv = NULL;
  }
  return rc;
}

int tlsDestroy(TlsEnvironment *env) {
  int rc = 0;
  rc = gsk_environment_close(env->envHandle);
  safeFree((char*)env, sizeof(*env));
  return rc;
}

static const int WAIT_TIME_MS = 200;

static int secureSocketRecv(int fd, void *data, int len, char *userData) {
  int rc = 0;
  while (1) {
    rc = recv(fd, data, len, 0);
    if (rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      PollItem item = {0};
      item.fd = fd;
      item.events = POLLERDNORM;
      int returnCode = 0;
      int reasonCode = 0;
      int status = fdPoll(&item, 0, 1, WAIT_TIME_MS, &returnCode, &reasonCode);
      // continue in any case
      continue;
    } else {
      break;
    }
  }
  return rc;
}

static int secureSocketSend(int fd, void *data, int len, char *userData) {
  int rc = 0;
  while (1) {
    rc = send(fd, data, len, 0);
    if (rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      PollItem item = {0};
      item.fd = fd;
      item.events = POLLEWRNORM;
      int returnCode = 0;
      int reasonCode = 0;
      int status = fdPoll(&item, 0, 1, WAIT_TIME_MS, &returnCode, &reasonCode);
      // continue in any case
      continue;
    } else {
      break;
    }
  }
  return rc;
}
 
int tlsSocketInit(TlsEnvironment *env, TlsSocket **outSocket, int fd, bool isServer) {
  int rc = 0;
  gsk_iocallback ioCallbacks = {secureSocketRecv, secureSocketSend, NULL, NULL, NULL, NULL};
  TlsSocket *socket = (TlsSocket*)safeMalloc(sizeof(TlsSocket), "Tls Socket");
  if (!socket) {
    return TLS_ALLOC_ERROR;
  }
  char *label = env->settings->label;
  char *ciphers = env->settings->ciphers;
  char *keyshares = env->settings->keyshares;
  rc = rc || gsk_secure_socket_open(env->envHandle, &socket->socketHandle);
  rc = rc || gsk_attribute_set_numeric_value(socket->socketHandle, GSK_FD, fd);
  if (label) {
    rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_KEYRING_LABEL, label, 0);
  }
  rc = rc || gsk_attribute_set_enum(socket->socketHandle, GSK_SESSION_TYPE, isServer ? GSK_SERVER_SESSION_WITH_CL_AUTH : GSK_CLIENT_SESSION);
  if (ciphers) {
    rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_V3_CIPHER_SPECS_EXPANDED, ciphers, 0);
    rc = rc || gsk_attribute_set_enum(socket->socketHandle, GSK_V3_CIPHERS, GSK_V3_CIPHERS_CHAR4);
  }
  /*
    To be safe,
  */
  if (isTLSV13Enabled(env->settings)) {
    if (keyshares) {
     /*   
       Only TLS 1.3 needs this.
     */
      if (isServer) {
        rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_SERVER_TLS_KEY_SHARES, keyshares, 0);
      } else {
        rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_CLIENT_TLS_KEY_SHARES, keyshares, 0);
      }
    }
  }
  rc = rc || gsk_attribute_set_callback(socket->socketHandle, GSK_IO_CALLBACK, &ioCallbacks);
  rc = rc || gsk_secure_socket_init(socket->socketHandle);
  if (rc == 0) {
    *outSocket = socket;
  } else {
    safeFree((char*)socket, sizeof(*socket));
    *outSocket = NULL;
  }
  return rc;
}

int tlsRead(TlsSocket *socket, const char *buf, int size, int *outLength) {
  int rc = gsk_secure_socket_read(socket->socketHandle, (char *)buf, size, outLength);
  return rc;
}

int tlsWrite(TlsSocket *socket, const char *buf, int size, int *outLength) {
  int rc = gsk_secure_socket_write(socket->socketHandle, (char *)buf, size, outLength);
  return rc;
}

int tlsSocketClose(TlsSocket *socket) {
  int rc = 0;
  rc = gsk_secure_socket_close(&socket->socketHandle);
  safeFree((char*)socket, sizeof(*socket));
  return rc;
}

const char *tlsStrError(int rc) {
  if (rc < 0 ) {
    switch (rc) {
      case TLS_ALLOC_ERROR:
        return "Failed to allocate memory";
      default:
        return "Unknown error";
    }
  }
  // GSK error codes are positive
  return gsk_strerror(rc);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
