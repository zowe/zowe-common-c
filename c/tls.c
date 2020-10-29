/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdlib.h>
#include <errno.h>
#include "alloc.h"
#include "bpxnet.h"
#include "fdpoll.h"
#include "tls.h"

int tlsInit(TlsEnvironment **outEnv, TlsSettings *settings) {
  int rc = 0;
  TlsEnvironment *env = (TlsEnvironment *)safeMalloc(sizeof(*env), "Tls Environment");
  env->settings = settings;
  rc = rc || gsk_environment_open(&env->envHandle);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_SSLV2, GSK_PROTOCOL_SSLV2_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_SSLV3, GSK_PROTOCOL_SSLV3_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1, GSK_PROTOCOL_TLSV1_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1_1, GSK_PROTOCOL_TLSV1_1_OFF);
  rc = rc || gsk_attribute_set_enum(env->envHandle, GSK_PROTOCOL_TLSV1_2, GSK_PROTOCOL_TLSV1_2_ON);
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

static int waitTimeMs = 200;

static int secureSocketRecv(int fd, void *data, int len, char *userData) {
  int rc = recv(fd, data, len, 0);
  if (rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
    while (1) {
      PollItem item = {0};
      item.fd = fd;
      item.events = POLLERDNORM;
      int returnCode = 0;
      int reasonCode = 0;
      int status = fdPoll(&item, 0, 1, waitTimeMs, &returnCode, &reasonCode);
      if (status == -1) {
        continue;
      }
      if (item.revents & POLLERDNORM) {
        rc = recv(fd, data, len, 0);
        break;
      }
    }
  }
  return rc;
}

static int secureSocketSend(int fd, void *data, int len, char *userData) {
  int rc = send(fd, data, len, 0);
  if (rc == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
    while (1) {
      PollItem item = {0};
      item.fd = fd;
      item.events = POLLEWRNORM;
      int returnCode = 0;
      int reasonCode = 0;
      int status = fdPoll(&item, 0, 1, waitTimeMs, &returnCode, &reasonCode);
      if (status == -1) {
        continue;
      }
      if (item.revents & POLLRWRNORM) {
        rc = send(fd, data, len, 0);
        break;
      }
    }
  }
  return rc;
}

int tlsSocketInit(TlsEnvironment *env, TlsSocket **outSocket, int fd, bool isServer) {
  int   rc = 0;
  gsk_iocallback ioCallbacks = {secureSocketRecv, secureSocketSend, NULL, NULL, NULL, NULL};
  TlsSocket *socket = (TlsSocket*)safeMalloc(sizeof(TlsSocket), "Tls Socket");
  char *label = env->settings->label;
  char *ciphers = env->settings->ciphers;
  rc = rc || gsk_secure_socket_open(env->envHandle, &socket->socketHandle);
  rc = rc || gsk_attribute_set_numeric_value(socket->socketHandle, GSK_FD, fd);
  if (label) {
    rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_KEYRING_LABEL, label, 0);
  }
  rc = rc || gsk_attribute_set_enum(socket->socketHandle, GSK_SESSION_TYPE,
                                    isServer ? GSK_SERVER_SESSION : GSK_CLIENT_SESSION);
  if (ciphers) {
    rc = rc || gsk_attribute_set_buffer(socket->socketHandle, GSK_V3_CIPHER_SPECS_EXPANDED, ciphers, 0);
    rc = rc || gsk_attribute_set_enum(socket->socketHandle, GSK_V3_CIPHERS, GSK_V3_CIPHERS_CHAR4);
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
  if (rc == GSK_WOULD_BLOCK)
  return rc;
}

int tlsSocketClose(TlsSocket *socket) {
  int rc = 0;
  rc = rc || gsk_secure_socket_shutdown(socket->socketHandle);
  rc = rc || gsk_secure_socket_close(&socket->socketHandle);
  safeFree((char*)socket, sizeof(*socket));
  return rc;
}

const char *tlsStrError(int rc) {
  return gsk_strerror(rc);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/