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
  // https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.2.0/com.ibm.zos.v2r2.gska100/csdcwh.htm
#define TLS_NULL_WITH_NULL_NULL "0000" // No encryption or message authentication and RSA key exchange
#define TLS_RSA_WITH_NULL_MD5 "0001" // No encryption with MD5 message authentication and RSA key exchange
#define TLS_RSA_WITH_NULL_SHA "0002" // No encryption with SHA-1 message authentication and RSA key exchange
#define TLS_RSA_EXPORT_WITH_RC4_40_MD5 "0003" // 40-bit RC4 encryption with MD5 message authentication and RSA (export) key exchange
#define TLS_RSA_WITH_RC4_128_MD5 "0004" // 128-bit RC4 encryption with MD5 message authentication and RSA key exchange
#define TLS_RSA_WITH_RC4_128_SHA "0005" // 128-bit RC4 encryption with SHA-1 message authentication and RSA key exchange
#define TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5 "0006" // 40-bit RC2 encryption with MD5 message authentication and RSA (export) key exchange
#define TLS_RSA_WITH_DES_CBC_SHA "0009" // 56-bit DES encryption with SHA-1 message authentication and RSA key exchange
#define TLS_RSA_WITH_3DES_EDE_CBC_SHA "000A" // 168-bit Triple DES encryption with SHA-1 message authentication and RSA key exchange
#define TLS_DH_DSS_WITH_DES_CBC_SHA "000C" // 56-bit DES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA "000D" // 168-bit Triple DES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_RSA_WITH_DES_CBC_SHA "000F" // 56-bit DES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA "0010" // 168-bit Triple DES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_DES_CBC_SHA "0012" // 56-bit DES encryption with SHA-1message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA "0013" // 168-bit Triple DES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_RSA_WITH_DES_CBC_SHA "0015" // 56-bit DES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA "0016" // 168-bit Triple DES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_RSA_WITH_AES_128_CBC_SHA "002F" // 128-bit AES encryption with SHA-1 message authentication and RSA key exchange
#define TLS_DH_DSS_WITH_AES_128_CBC_SHA "0030" // 128-bit AES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_RSA_WITH_AES_128_CBC_SHA "0031" // 128-bit AES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_AES_128_CBC_SHA "0032" // 128-bit AES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA "0033" // 128-bit AES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_RSA_WITH_AES_256_CBC_SHA "0035" // 256-bit AES encryption with SHA-1 message authentication and RSA key exchange
#define TLS_DH_DSS_WITH_AES_256_CBC_SHA "0036" // 256-bit AES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_RSA_WITH_AES_256_CBC_SHA "0037" // 256-bit AES encryption with SHA-1 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_AES_256_CBC_SHA "0038" // 256-bit AES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA "0039" // 256-bit AES encryption with SHA-1 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_RSA_WITH_NULL_SHA256 "003B" // No encryption with SHA-256 message authentication and RSA key exchange
#define TLS_RSA_WITH_AES_128_CBC_SHA256 "003C" // 128-bit AES encryption with SHA-256 message authentication and RSA key exchange
#define TLS_RSA_WITH_AES_256_CBC_SHA256 "003D" // 256-bit AES encryption with SHA-256 message authentication and RSA key exchange
#define TLS_DH_DSS_WITH_AES_128_CBC_SHA256 "003E" // 128-bit AES encryption with SHA-256 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_RSA_WITH_AES_128_CBC_SHA256 "003F" // 128-bit AES encryption with SHA-256 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_AES_128_CBC_SHA256 "0040" // 128-bit AES encryption with SHA-256 message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 "0067" // 128-bit AES encryption with SHA-256 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DH_DSS_WITH_AES_256_CBC_SHA256 "0068" // 256-bit AES encryption with SHA-256 message authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_RSA_WITH_AES_256_CBC_SHA256 "0069" // 256-bit AES encryption with SHA-256 message authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_AES_256_CBC_SHA256 "006A" // 256-bit AES encryption with SHA-256 message authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 "006B" // 256-bit AES encryption with SHA-256 message authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_RSA_WITH_AES_128_GCM_SHA256 "009C" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and RSA key exchange
#define TLS_RSA_WITH_AES_256_GCM_SHA384 "009D" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and RSA key exchange
#define TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 "009E" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 "009F" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and ephemeral Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DH_RSA_WITH_AES_128_GCM_SHA256 "00A0" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DH_RSA_WITH_AES_256_GCM_SHA384 "00A1" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and fixed Diffie-Hellman key exchange signed with an RSA certificate
#define TLS_DHE_DSS_WITH_AES_128_GCM_SHA256 "00A2" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DHE_DSS_WITH_AES_256_GCM_SHA384 "00A3" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and ephemeral Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_DSS_WITH_AES_128_GCM_SHA256 "00A4" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_DH_DSS_WITH_AES_256_GCM_SHA384 "00A5" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and fixed Diffie-Hellman key exchange signed with a DSA certificate
#define TLS_ECDH_ECDSA_WITH_NULL_SHA "C001" // NULL encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_RC4_128_SHA "C002" // 128-bit RC4 encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA "C003" // 168-bit Triple DES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA "C004" // 128-bit AES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA "C005" // 256-bit AES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_NULL_SHA "C006" // NULL encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_RC4_128_SHA "C007" // 128-bit RC4 encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA "C008" // 168-bit Triple DES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA "C009" // 128-bit AES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA "C00A" // 256-bit AES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_RSA_WITH_NULL_SHA "C00B" // NULL encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_RC4_128_SHA "C00C" // 128-bit RC4 encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA "C00D" // 168-bit Triple DES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA "C00E" // 128-bit AES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA "C00F" // 256-bit AES encryption with SHA-1 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_NULL_SHA "C010" // NULL encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_RC4_128_SHA "C011" // 128-bit RC4 encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA "C012" // 168-bit Triple DES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA "C013" // 128-bit AES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA "C014" // 256-bit AES encryption with SHA-1 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 "C023" // 128-bit AES encryption with SHA-256 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 "C024" // 256-bit AES encryption with SHA-384 message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256 "C025" // 128-bit AES encryption with SHA-256 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384 "C026" // 256-bit AES encryption with SHA-384 message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 "C027" // 128-bit AES encryption with SHA-256 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384 "C028" // 256-bit AES encryption with SHA-384 message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256 "C029" // 128-bit AES encryption with SHA-256 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384 "C02A" // 256-bit AES encryption with SHA-384 message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 "C02B" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 "C02C" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and ephemeral ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256 "C02D" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384 "C02E" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and fixed ECDH key exchange signed with an ECDSA certificate
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 "C02F" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 "C030" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and ephemeral ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256 "C031" // 128-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384 "C032" // 256-bit AES in Galois Counter Mode encryption with 128-bit AEAD message authentication and fixed ECDH key exchange signed with an RSA certificate
#define TLS_AES_128_GCM_SHA256 "1301"
#define TLS_AES_256_GCM_SHA384 "1302"
#define TLS_CHACHA20_POLY1305_SHA256 "1303"
  char *ciphers;
#define TLS_X25519 "0029"
#define TLS_SECP256R1 "0023"
#define TLS_SECP521R1 "0025"
  char *keyshares;
  /*
     TLSv1.3 isn't supported on some zos versions. Having it
     enabled causes issues.
     TODO: Find out why it isn't negotiating 1.2.
  */
  char *maxTls;
  char *minTls;
#define TLS_CURVE_RESERVED_DEFAULT "0000"
#define TLS_CURVE_SECT163K1 "0001"
#define TLS_CURVE_SECT163R1 "0002"
#define TLS_CURVE_SECT163R2 "0003"
#define TLS_CURVE_SECT193R1 "0004"
#define TLS_CURVE_SECT193R2 "0005"
#define TLS_CURVE_SECT233K1 "0006"
#define TLS_CURVE_SECT233R1 "0007"
#define TLS_CURVE_SECT239K1 "0008"
#define TLS_CURVE_SECT283K1 "0009"
#define TLS_CURVE_SECT283R1 "0010"
#define TLS_CURVE_SECT409K1 "0011"
#define TLS_CURVE_SECT409R1 "0012"
#define TLS_CURVE_SECT571K1 "0013"
#define TLS_CURVE_SECT571R1 "0014"
#define TLS_CURVE_SECP160K1 "0015"
#define TLS_CURVE_SECP160R1 "0016"
#define TLS_CURVE_SECP160R2 "0017"
#define TLS_CURVE_SECP192K1 "0018"
#define TLS_CURVE_PRIME192V "0019"
#define TLS_CURVE_SECP224K1 "0020"
#define TLS_CURVE_SECP224R1 "0021"
#define TLS_CURVE_SECP256K1 "0022"
#define TLS_CURVE_SECP384R1 "0024"
#define TLS_CURVE_PRIME256V1 "0023"
#define TLS_CURVE_SECP521R1 "0025"
#define TLS_CURVE_BRAINPOOLP256R1 "0026"
#define TLS_CURVE_BRAINPOOLP384R1 "0027"
#define TLS_CURVE_BRAINPOOLP512R1 "0028"
#define TLS_CURVE_X25519 "0029"
#define TLS_CURVE_X448 "0030"
#define TLS_CURVE_BRAINPOOLP256R1TLS13 "0031"
#define TLS_CURVE_BRAINPOOLP384R1TLS13 "0032"
#define TLS_CURVE_BRAINPOOLP512R1TLS13 "0033"
  char *curves;
} TlsSettings;

typedef struct TlsEnvironment_tag {
  gsk_handle envHandle;
  TlsSettings *settings;
} TlsEnvironment;

typedef struct TlsSocket_tag {
  gsk_handle socketHandle;
} TlsSocket;

typedef struct CipherMap_tag {
  const char* name;
  const char* suiteId;  //number string
} CipherMap;

#define TLS_IANA_CIPHER_MAP(ianaCipherMap)\
  static const CipherMap ianaCipherMap[] = {\
    {"TLS_NULL_WITH_NULL_NULL", TLS_NULL_WITH_NULL_NULL},\
    {"TLS_RSA_WITH_NULL_MD5", TLS_RSA_WITH_NULL_MD5},\
    {"TLS_RSA_WITH_NULL_SHA", TLS_RSA_WITH_NULL_SHA},\
    {"TLS_RSA_EXPORT_WITH_RC4_40_MD5", TLS_RSA_EXPORT_WITH_RC4_40_MD5},\
    {"TLS_RSA_WITH_RC4_128_MD5", TLS_RSA_WITH_RC4_128_MD5},\
    {"TLS_RSA_WITH_RC4_128_SHA", TLS_RSA_WITH_RC4_128_SHA},\
    {"TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5", TLS_RSA_EXPORT_WITH_RC2_CBC_40_MD5},\
    {"TLS_RSA_WITH_DES_CBC_SHA", TLS_RSA_WITH_DES_CBC_SHA},\
    {"TLS_RSA_WITH_3DES_EDE_CBC_SHA", TLS_RSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_DH_DSS_WITH_DES_CBC_SHA", TLS_DH_DSS_WITH_DES_CBC_SHA},\
    {"TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA", TLS_DH_DSS_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_DH_RSA_WITH_DES_CBC_SHA", TLS_DH_RSA_WITH_DES_CBC_SHA},\
    {"TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA", TLS_DH_RSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_DHE_DSS_WITH_DES_CBC_SHA", TLS_DHE_DSS_WITH_DES_CBC_SHA},\
    {"TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA", TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_DHE_RSA_WITH_DES_CBC_SHA", TLS_DHE_RSA_WITH_DES_CBC_SHA},\
    {"TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA", TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_RSA_WITH_AES_128_CBC_SHA", TLS_RSA_WITH_AES_128_CBC_SHA},\
    {"TLS_DH_DSS_WITH_AES_128_CBC_SHA", TLS_DH_DSS_WITH_AES_128_CBC_SHA},\
    {"TLS_DH_RSA_WITH_AES_128_CBC_SHA", TLS_DH_RSA_WITH_AES_128_CBC_SHA},\
    {"TLS_DHE_DSS_WITH_AES_128_CBC_SHA", TLS_DHE_DSS_WITH_AES_128_CBC_SHA},\
    {"TLS_DHE_RSA_WITH_AES_128_CBC_SHA", TLS_DHE_RSA_WITH_AES_128_CBC_SHA},\
    {"TLS_RSA_WITH_AES_256_CBC_SHA", TLS_RSA_WITH_AES_256_CBC_SHA},\
    {"TLS_DH_DSS_WITH_AES_256_CBC_SHA", TLS_DH_DSS_WITH_AES_256_CBC_SHA},\
    {"TLS_DH_RSA_WITH_AES_256_CBC_SHA", TLS_DH_RSA_WITH_AES_256_CBC_SHA},\
    {"TLS_DHE_DSS_WITH_AES_256_CBC_SHA", TLS_DHE_DSS_WITH_AES_256_CBC_SHA},\
    {"TLS_DHE_RSA_WITH_AES_256_CBC_SHA", TLS_DHE_RSA_WITH_AES_256_CBC_SHA},\
    {"TLS_RSA_WITH_NULL_SHA256", TLS_RSA_WITH_NULL_SHA256},\
    {"TLS_RSA_WITH_AES_128_CBC_SHA256", TLS_RSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_RSA_WITH_AES_256_CBC_SHA256", TLS_RSA_WITH_AES_256_CBC_SHA256},\
    {"TLS_DH_DSS_WITH_AES_128_CBC_SHA256", TLS_DH_DSS_WITH_AES_128_CBC_SHA256},\
    {"TLS_DH_RSA_WITH_AES_128_CBC_SHA256", TLS_DH_RSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_DHE_DSS_WITH_AES_128_CBC_SHA256", TLS_DHE_DSS_WITH_AES_128_CBC_SHA256},\
    {"TLS_DHE_RSA_WITH_AES_128_CBC_SHA256", TLS_DHE_RSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_DH_DSS_WITH_AES_256_CBC_SHA256", TLS_DH_DSS_WITH_AES_256_CBC_SHA256},\
    {"TLS_DH_RSA_WITH_AES_256_CBC_SHA256", TLS_DH_RSA_WITH_AES_256_CBC_SHA256},\
    {"TLS_DHE_DSS_WITH_AES_256_CBC_SHA256", TLS_DHE_DSS_WITH_AES_256_CBC_SHA256},\
    {"TLS_DHE_RSA_WITH_AES_256_CBC_SHA256", TLS_DHE_RSA_WITH_AES_256_CBC_SHA256},\
    {"TLS_RSA_WITH_AES_128_GCM_SHA256", TLS_RSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_RSA_WITH_AES_256_GCM_SHA384", TLS_RSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_DHE_RSA_WITH_AES_128_GCM_SHA256", TLS_DHE_RSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_DHE_RSA_WITH_AES_256_GCM_SHA384", TLS_DHE_RSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_DH_RSA_WITH_AES_128_GCM_SHA256", TLS_DH_RSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_DH_RSA_WITH_AES_256_GCM_SHA384", TLS_DH_RSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_DHE_DSS_WITH_AES_128_GCM_SHA256", TLS_DHE_DSS_WITH_AES_128_GCM_SHA256},\
    {"TLS_DHE_DSS_WITH_AES_256_GCM_SHA384", TLS_DHE_DSS_WITH_AES_256_GCM_SHA384},\
    {"TLS_DH_DSS_WITH_AES_128_GCM_SHA256", TLS_DH_DSS_WITH_AES_128_GCM_SHA256},\
    {"TLS_DH_DSS_WITH_AES_256_GCM_SHA384", TLS_DH_DSS_WITH_AES_256_GCM_SHA384},\
    {"TLS_ECDH_ECDSA_WITH_NULL_SHA", TLS_ECDH_ECDSA_WITH_NULL_SHA},\
    {"TLS_ECDH_ECDSA_WITH_RC4_128_SHA", TLS_ECDH_ECDSA_WITH_RC4_128_SHA},\
    {"TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA", TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA", TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA},\
    {"TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA", TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_NULL_SHA", TLS_ECDHE_ECDSA_WITH_NULL_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_RC4_128_SHA", TLS_ECDHE_ECDSA_WITH_RC4_128_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA", TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA", TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA", TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA},\
    {"TLS_ECDH_RSA_WITH_NULL_SHA", TLS_ECDH_RSA_WITH_NULL_SHA},\
    {"TLS_ECDH_RSA_WITH_RC4_128_SHA", TLS_ECDH_RSA_WITH_RC4_128_SHA},\
    {"TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA", TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_ECDH_RSA_WITH_AES_128_CBC_SHA", TLS_ECDH_RSA_WITH_AES_128_CBC_SHA},\
    {"TLS_ECDH_RSA_WITH_AES_256_CBC_SHA", TLS_ECDH_RSA_WITH_AES_256_CBC_SHA},\
    {"TLS_ECDHE_RSA_WITH_NULL_SHA", TLS_ECDHE_RSA_WITH_NULL_SHA},\
    {"TLS_ECDHE_RSA_WITH_RC4_128_SHA", TLS_ECDHE_RSA_WITH_RC4_128_SHA},\
    {"TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA", TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA},\
    {"TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA", TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA},\
    {"TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA", TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA},\
    {"TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256", TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384", TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384},\
    {"TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256", TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384", TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384},\
    {"TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256", TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384", TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384},\
    {"TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256", TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256},\
    {"TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384", TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384},\
    {"TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256", TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384", TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256", TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384", TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256", TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384", TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256", TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256},\
    {"TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384", TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384},\
    {"TLS_AES_128_GCM_SHA256", TLS_AES_128_GCM_SHA256},\
    {"TLS_AES_256_GCM_SHA384", TLS_AES_256_GCM_SHA384},\
    {"TLS_CHACHA20_POLY1305_SHA256", TLS_CHACHA20_POLY1305_SHA256},\
    {0, NULL}\
};

  typedef struct CurveMap_tag {
  const char* name;
  const char* groupId;  //number string
} CurveMap;

#define TLS_IANA_CURVE_MAP(ianaCurveMap)\
  static const CurveMap ianaCurveMap[] = {\
    {"secp192r1",TLS_CURVE_PRIME192V},\
    {"NIST P-192",TLS_CURVE_PRIME192V},\
    {"prime192v",TLS_CURVE_PRIME192V},\
    {"secp224r1", TLS_CURVE_SECP224R1},\
    {"NIST P-224", TLS_CURVE_SECP224R1},\
    {"NIST P-256",TLS_CURVE_PRIME256V1},\
    {"secp256r1",TLS_CURVE_PRIME256V1},\
    {"prime256v1",TLS_CURVE_PRIME256V1},\
    {"NIST P-384", TLS_CURVE_SECP384R1},\
    {"secp384r1", TLS_CURVE_SECP384R1},\
    {"NIST P-521", TLS_CURVE_SECP521R1},\
    {"secp521r1", TLS_CURVE_SECP521R1},\
    {"x25519", TLS_CURVE_X25519},\
    {"x448", TLS_CURVE_X448},\
    {0, NULL}\
};

/*
  Currently, only the curves mentioned https://www.ibm.com/docs/en/zos/3.1.0?topic=programming-cipher-suite-definitions#csdcwh__tttcsd
  are supported, if any curves are added in the future they should be added to the above array.

    {"NIST K-163", TLS_CURVE_SECT163K1},\
    {"NIST K-283", TLS_CURVE_SECT283K1},\
    {"brainpoolP256r1", TLS_CURVE_BRAINPOOLP256R1},\
    {"sect163k1", TLS_CURVE_SECT163K1},\
    {"NIST K-163", TLS_CURVE_SECT163K1},\
    {"sect163r2", TLS_CURVE_SECT163R2},\
    {"NIST B-163", TLS_CURVE_SECT163R2},\
    {"sect233k1", TLS_CURVE_SECT233K1},\
    {"NIST K-233", TLS_CURVE_SECT233K1},\
    {"sect233r1", TLS_CURVE_SECT233R1},\
    {"NIST K-233", TLS_CURVE_SECT233R1},\
    {"sect283k1", TLS_CURVE_SECT283K1},\
    {"NIST K-283", TLS_CURVE_SECT283K1},\
    {"sect283r1", TLS_CURVE_SECT283R1},\
    {"NIST B-283", TLS_CURVE_SECT283R1},\
    {"sect409k1", TLS_CURVE_SECT409K1},\
    {"NIST K-409", TLS_CURVE_SECT409K1},\
    {"sect409r1", TLS_CURVE_SECT409R1},\
    {"NIST B-409", TLS_CURVE_SECT409R1},\
    {"sect571k1", TLS_CURVE_SECT571K1},\
    {"NIST K-571", TLS_CURVE_SECT571K1},\
    {"sect571r1", TLS_CURVE_SECT571R1},\
    {"NIST B-571", TLS_CURVE_SECT571R1},\
    {"sect163r1", TLS_CURVE_SECT163R1},\
    {"sect193r1", TLS_CURVE_SECT193R1},\
    {"sect193r2", TLS_CURVE_SECT193R2},\
    {"sect239k1", TLS_CURVE_SECT239K1},\
    {"secp160k1", TLS_CURVE_SECP160K1},\
    {"secp160r1", TLS_CURVE_SECP160R1},\
    {"secp160r2", TLS_CURVE_SECP160R2},\
    {"secp192k1", TLS_CURVE_SECP192K1},\
    {"secp224k1", TLS_CURVE_SECP224K1},\
    {"secp256k1", TLS_CURVE_SECP256K1},\
    {"brainpoolP256r1", TLS_CURVE_BRAINPOOLP256R1},\
    {"brainpoolP384r1", TLS_CURVE_BRAINPOOLP384R1},\
    {"brainpoolP512r1", TLS_CURVE_BRAINPOOLP512R1},\
    {"brainpoolP256r1tls13", TLS_CURVE_BRAINPOOLP256R1TLS13},\
    {"brainpoolP384r1tls13", TLS_CURVE_BRAINPOOLP384R1TLS13},\
    {"brainpoolP512r1tls13", TLS_CURVE_BRAINPOOLP512R1TLS13},\
*/

int tlsInit(TlsEnvironment **outEnv, TlsSettings *settings);
int tlsDestroy(TlsEnvironment *env);
int tlsSocketInit(TlsEnvironment *env, TlsSocket **outSocket, int fd, bool isServer);
int tlsSocketClose(TlsSocket *socket);
int tlsRead(TlsSocket *socket, const char *buf, int size, int *outLength);
int tlsWrite(TlsSocket *socket, const char *buf, int size, int *outLength);
const char *tlsStrError(int rc);
int getClientCertificate(gsk_handle soc_handle, char *clientCertificate, unsigned int clientCertificateBufferSize, unsigned int *clientCertificateLength);

#define TLS_ALLOC_ERROR (-1)

#endif // __TLS_H__

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
