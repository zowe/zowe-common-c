

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"

#ifdef USE_RS_SSL
#include "rs_ssl.h"
#endif
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif

#ifdef _LP64
#pragma linkage(BPX4SOC,OS)
#pragma linkage(BPX4CON,OS)
#pragma linkage(BPX4GHN,OS)
#pragma linkage(BPX4SLP,OS)
#pragma linkage(BPX4BND,OS)
#pragma linkage(BPX4LSN,OS)
#pragma linkage(BPX4ACP,OS)
#pragma linkage(BPX4SEL,OS)
#pragma linkage(BPX4OPT,OS)
#pragma linkage(BPX4GNM,OS)
#pragma linkage(BPX4STO,OS)
#pragma linkage(BPX4RFM,OS)
#pragma linkage(BPX4HST,OS)
#pragma linkage(BPX4IOC,OS)
#pragma linkage(BPX4RED,OS)
#pragma linkage(BPX4WRT,OS)
#pragma linkage(BPX4FCT,OS)
#pragma linkage(BPX4CLO,OS)
#pragma linkage(BPX4GAI,OS)
#pragma linkage(BPX4FAI,OS)

#define BPXSOC BPX4SOC
#define BPXCON BPX4CON
#define BPXGHN BPX4GHN
#define BPXSLP BPX4SLP
#define BPXBND BPX4BND
#define BPXLSN BPX4LSN
#define BPXACP BPX4ACP
#define BPXSEL BPX4SEL
#define BPXOPT BPX4OPT
#define BPXGNM BPX4GNM
#define BPXSTO BPX4STO
#define BPXRFM BPX4RFM
#define BPXHST BPX4HST
#define BPXIOC BPX4IOC
#define BPXRED BPX4RED
#define BPXWRT BPX4WRT
#define BPXFCT BPX4FCT
#define BPXCLO BPX4CLO
#define BPXGAI BPX4GAI
#define BPXFAI BPX4FAI

#else

#pragma linkage(BPX1SOC,OS)
#pragma linkage(BPX1CON,OS)
#pragma linkage(BPX1GHN,OS)
#pragma linkage(BPX1SLP,OS)
#pragma linkage(BPX1BND,OS)
#pragma linkage(BPX1LSN,OS)
#pragma linkage(BPX1ACP,OS)
#pragma linkage(BPX1SEL,OS)
#pragma linkage(BPX1OPT,OS)
#pragma linkage(BPX1GNM,OS)
#pragma linkage(BPX1STO,OS)
#pragma linkage(BPX1RFM,OS)
#pragma linkage(BPX1HST,OS)
#pragma linkage(BPX1IOC,OS)
#pragma linkage(BPX1RED,OS)
#pragma linkage(BPX1WRT,OS)
#pragma linkage(BPX1FCT,OS)
#pragma linkage(BPX1CLO,OS)
#pragma linkage(BPX1GAI,OS)
#pragma linkage(BPX1FAI,OS)


#define BPXSOC BPX1SOC
#define BPXCON BPX1CON
#define BPXGHN BPX1GHN
#define BPXSLP BPX1SLP
#define BPXBND BPX1BND
#define BPXLSN BPX1LSN
#define BPXACP BPX1ACP
#define BPXSEL BPX1SEL
#define BPXOPT BPX1OPT
#define BPXGNM BPX1GNM
#define BPXSTO BPX1STO
#define BPXRFM BPX1RFM
#define BPXHST BPX1HST
#define BPXIOC BPX1IOC
#define BPXRED BPX1RED
#define BPXWRT BPX1WRT
#define BPXFCT BPX1FCT
#define BPXCLO BPX1CLO
#define BPXGAI BPX1GAI
#define BPXFAI BPX1FAI

#endif

/* xlclang and clang what prototypes, these are incomplete, but quiet the compiler */
int BPXSOC();
int BPXCON();
int BPXGHN();
int BPXSLP();
int BPXCHR();
int BPXBND();
int BPXLSN();
int BPXACP();
int BPXSEL();
int BPXOPT();
int BPXGNM();
int BPXSTO();
int BPXRFM();
int BPXHST();
int BPXIOC();
int BPXRED();
int BPXWRT();
int BPXFCT();
int BPXCLO();
int BPXGAI();
int BPXFAI();

#define SOCK_SO_REUSEADDR 0x00000004
#define SOCK_SO_SNDBUF    0x00001001
#define SOCK_SO_RCVBUF    0x00001002
#define SOCK_SO_SNDLOWAT  0x00001003
#define SOCK_SO_RCVLOWAT  0x00001004
#define SOCK_SO_SNDTIMEO  0x00001005
#define SOCK_SO_RCVTIMEO  0x00001006

#ifndef SOL_SOCKET
#define SOL_SOCKET  0x0000FFFF
#endif

#ifndef TCP_NODELAY
#define TCP_NODELAY 0x00000001
#endif

static int socketTrace = 0;

#ifndef Addr_Info

/* the netdb.h version of addrinfo uses opaque types for ai_addr that prevent
 * functions like AddrInfo_convert from working */

#ifndef socklen_t
typedef unsigned int socklen_t;
#endif

#ifndef AF_UNSPEC
#define AF_UNSPEC       0       /* matches sys/socket.h */
#endif

/* confirmed applicability by looking at SYS1.MACLIB(EZBREHST) */
typedef struct addrinfo
{
#define AI_PASSIVE      (1 << 0)
#define AI_CANONNAMEOK  (1 << 1)
#define AI_NUMERICHOST  (1 << 2)
#define AI_NUMERICSERV  (1 << 3)
#define AI_V4MAPPED     (1 << 4)
#define AI_ALL          (1 << 5)
#define AI_ADDRCONFIG   (1 << 6)
#define AI_EXTFLAGS     (1 << 7)
  int              ai_flags;     /* AI_PASSIVE, AI_CANONNAME */
  int              ai_family;    /* PF_xxx */
  int              ai_socktype;  /* SOCK_xxx */
  int              ai_protocol;  /* 0 or IPPROTO_xxx */
  socklen_t        ai_addrlen;   /* length of ai_addr */
  int              __ai_reserved;/* reserved */
  __pad31( __ai_canonname_r,4 )  /* 31-bit padding */
  char            *ai_canonname; /* canonical name for hostname */
  __pad31( __ai_addr_r,4 )       /* 31-bit padding */
  struct socketAddr_tag *ai_addr; /* binary address consistent with sockaddr_in(6) */
  __pad31( __ai_next_r,4 )       /* 31-bit padding */
  struct addrinfo *ai_next;      /* next structure in list */
  #if __EDC_TARGET >= __EDC_LE410C
    int            ai_eflags;    /* source IP addr pref */
  #endif  /* __EDC_TARGET >= __EDC_LE410C */
} Addr_Info;

#endif /* Addr_Info undefined */

static int addrInfoTrace = 0;
int setAddrInfoTrace(int toWhat) {
  int was = addrInfoTrace;
#ifndef METTLE
  addrInfoTrace = toWhat;
#endif
  return was;
}

AddrInfoList getAddressInfoList(const char* nodeName, const char* serviceName,
                                char** out_canonicalName,
                                int *out_rc, int *out_rsn)
{
  Addr_Info *addrInfoList = NULL;
  int rv = 0;
  int nodeNameLen = 0;
  int serviceNameLen = 0;
  int canonicalNameLen = 0;
  /* "To get the most useful set of IP addresses available for the
   *  requested host name, applications that are enabled for IPv6
   *  processing should specify AI_V4MAPPED, AI_ALL, and AI_ADDRCONFIG
   *  in the ai_flags field; and AF_UNSPEC for the ai_family field in the
   *  input Addr_Info structure pointed to by the Hints_Ptr parameter.
   *
   *  When the stack has IPv6 capability, requests that are coded with
   *  AF_UNSPEC are treated as if the request is for AF_INET6, and all
   *  addresses are returned using sock_inet6_sockaddr structures (with the
   *  IPv4 addresses mapped appropriately, based on the AI_V4MAPPED setting).
   *  If there is no IPv6 capability, IPv4 addresses are returned in
   *  sock_inet_sockaddr structures.
   *
   *  This frees the application, to some extent, from having to decide what
   *  format works for the stack." */
  Addr_Info hints;
  memset(&hints, 0x00, sizeof(Addr_Info));

  hints.ai_flags = AI_V4MAPPED | AI_ALL | AI_ADDRCONFIG | AI_CANONNAMEOK;
  hints.ai_family = AF_UNSPEC; /* AF_UNSPEC is defined to 0 in sys/socket.h */

  Addr_Info *hintsPtr = &hints;

  do {
    if ((NULL == nodeName) && (NULL == serviceName)) {
      break;
    }
    if (nodeName) {
      nodeNameLen = strlen(nodeName);
    }
    if (serviceName) {
      serviceNameLen = strlen(serviceName);
    }

    if (addrInfoTrace) {
      printf("about to call BPXGAI...\n");
    }

    BPXGAI(nodeName,
           &nodeNameLen,
           serviceName,
           &serviceNameLen,
           &hintsPtr,
           &addrInfoList,
           &canonicalNameLen, /* length of ai_canonname in first returned list entry */
           &rv,
           out_rc,
           out_rsn);
    if (addrInfoTrace) {
      printf("BPXGAI returned rv:%d, rc:%d, rsn:0x%x, ailist:0x%p, cnl=%d\n",
             rv, *out_rc, *out_rsn, addrInfoList, canonicalNameLen);
    }

    if ((0 == *out_rc) && (0 < canonicalNameLen) && (NULL != out_canonicalName))
    {
      Addr_Info *firstAddrInfo = (Addr_Info*) addrInfoList;
      char* name = (char*) safeMalloc(1 + canonicalNameLen, "name");
      memcpy(name, firstAddrInfo->ai_canonname, canonicalNameLen);
      name[canonicalNameLen] = '\0';
      *out_canonicalName = name;
      if (addrInfoTrace) {
        printf("getAddressInfoList returning canonicalName: %s\n", name);
      }
    }

  } while(0);

  return (AddrInfoList) addrInfoList;
}

static int data6_isV4mappable(const struct ipV6Data in_data6)
{
  int ansiStatus = ANSI_FALSE;

  do {
    /**
     *
  Hinden                      Standards Track                    [Page 10]
  RFC 4291              IPv6 Addressing Architecture         February 2006

  2.5.5.2.  IPv4-Mapped IPv6 Address

  A second type of IPv6 address that holds an embedded IPv4 address is
  defined.  This address type is used to represent the addresses of
  IPv4 nodes as IPv6 addresses.  The format of the "IPv4-mapped IPv6
  address" is as follows:

  |                80 bits               | 16 |      32 bits        |
  +--------------------------------------+--------------------------+
  |0000..............................0000|FFFF|    IPv4 address     |
  +--------------------------------------+----+---------------------+
     */
    const unsigned char v4mappedPrefix[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                              0x00, 0x00, 0xFF, 0xFF};
    if (0 == memcmp(&(v4mappedPrefix[0]),
                    &(in_data6.addrData[0]),
                    sizeof(v4mappedPrefix)))
    {
      if (addrInfoTrace) {
        printf("data6_isV4mappable TRUE because prefix matched addrData content\n");
      }
      ansiStatus = ANSI_TRUE;
    }

  } while(0);

  return ansiStatus;
}

static int InetAddr_isV4mappable(const InetAddr *inetAddr)
{
  int ansiStatus = ANSI_FALSE;

  do {
    if (NULL == inetAddr)
      break;

    switch(inetAddr->type)
    {
      case AF_INET:
      {
        ansiStatus = ANSI_TRUE;
        break;
      }

      case AF_INET6:
      {
        ansiStatus = data6_isV4mappable(inetAddr->data.data6);
        break;
      }
    }
  } while(0);

  return ansiStatus;
}

static int SocketAddress_isV4mappable(const SocketAddress *socketAddress)
{
  int ansiStatus = ANSI_FALSE;

  do {
    if (NULL == socketAddress)
      break;

    int ifam = (int) socketAddress->family;
    switch(ifam) {
      case AF_INET:
      {
        ansiStatus = ANSI_TRUE;
        break;
      }

      case AF_INET6:
      {
        ansiStatus = data6_isV4mappable(socketAddress->data6);
        break;
      }
    }
  } while(0);

  return ansiStatus;
}

/**
 * @brief       Output a string representation of an IP address to a caller-supplied buffer.
 *
 * @param[in]     socketAddress A handle returned by getAddressInfoList
 * @param[in,out] strbuf        Caller-supplied output buffer
 * @param[in,out] len           Pointer to caller-supplied int.
 *                                On input, specify available size in inout_strbuf.
 *                                On output:
 *                                  If ANSI_OK, length of output including null terminator
 *                                  If ANSI_FAILED due to insufficient space, required length
 *
 * @return      ANSI_OK if a string was composed and output to the buffer.
 *              ANSI_FAILED if a string was not composed or the output space was too small.
 */
int SocketAddress_toString(const SocketAddress *in_socketAddress,
                           char* inout_strbuf, int* inout_len)
{
  int ansiStatus = ANSI_FAILED;
  /**
   * As described in RFC 4291, the preferred form is "x:x:x:x:x:x:x:x", where
   * Each x is a 16-bit section (dubbed a 'hextet') that can be represented
   * using up to four hexadecimal digits, with the sections separated by colons.
   *
   * Longest form would be:
   * "1234:5678:90ab:cdef:1234:5678:90ab:cdef[null]" so 40 bytes
   *
   * If we include a suffix for the port number (colon plus <=5 digits), the
   * max length is 46 bytes.  64-byte buffer gives us some flexibility around
   * formatting.
   *
   * Conventions for shortening:
   * - omit leading zeroes from each hextet
   * - omit hextets with all-zeroes
   *    - double-colon "::" represents any continuuous string of
   *      two or more hextets that are all zeroes, but this can
   *      only be done once without ambiguous results
   *
   * For now, we'll ignore the shortening conventions and strive for
   * accuracy of both AF_INET and AF_INET6 forms.
   *
   * One exception could be AF_INET6 addresses known to be V4-mapped. TBD.
   */
  do {
    if ((NULL == in_socketAddress) || (NULL == inout_strbuf) || (NULL == inout_len)) {
      break;
    }
    int available = *inout_len;
    int required = 0;
    char tmpbuf[64] = {0};
    int ifam = (int) in_socketAddress->family;

    switch(ifam)
    {
      case AF_INET:
      {
        const unsigned char* data = &(in_socketAddress->v4Addr[0]);
        sprintf(tmpbuf, "%d.%d.%d.%d", data[0], data[1], data[2], data[3]);
        required = 1 + strlen(tmpbuf);
        break;
      }

      case AF_INET6:
      {
        if (SocketAddress_isV4mappable(in_socketAddress)) {
          /* mapped V4 format - take bytes from indexes 12, 13, 14, 15 */
          const unsigned char* data = &(in_socketAddress->data6.addrData[12]);
          sprintf(tmpbuf, "::ffff:%d.%d.%d.%d", data[0], data[1], data[2], data[3]);
          required = 1 + strlen(tmpbuf);
        } else {
          /* IPv6 format */
          unsigned short us1=0, us2=0, us3=0, us4=0, us5=0, us6=0, us7=0, us8=0;
          const char* data = &(in_socketAddress->data6.addrData[0]);
          us1 = (data[0] << 8) + data[1];
          us2 = (data[2] << 8) + data[3];
          us3 = (data[4] << 8) + data[5];
          us4 = (data[6] << 8) + data[7];
          us5 = (data[8] << 8) + data[9];
          us6 = (data[10] << 8) + data[11];
          us7 = (data[12] << 8) + data[13];
          us8 = (data[14] << 8) + data[15];
          sprintf(tmpbuf, "%x:%x:%x:%x:%x:%x:%x:%x",
                  us1, us2, us3, us4, us5, us6, us7, us8);
          required = 1 + strlen(tmpbuf);
        }
        break;
      }
    }

    if (!required) {
      /* we didn't compose, break and return failed */
      break;
    }

    if (available < required) {
      *inout_len = required;
      break;
    }

    memcpy(inout_strbuf, tmpbuf, required);
    *inout_len = required;
    ansiStatus = ANSI_OK;

  } while(0);

  return ansiStatus;
}

int AddrInfo_isV4mappable(const void *in_addrInfo)
{
  int ansiStatus = ANSI_FALSE;
  const Addr_Info *addrInfo = (const Addr_Info *) in_addrInfo;

  do {
    if (NULL == addrInfo)
      break;

    switch (addrInfo->ai_family)
    {
      case AF_INET:
      {
        if (addrInfoTrace) {
          printf("AddrInfo_isV4mappable TRUE because AF_INET\n");
        }
        ansiStatus = ANSI_TRUE;
        break;
      }

      case AF_INET6:
      {
        SocketAddress *tmpSocketAddr = (SocketAddress*) (addrInfo->ai_addr);
        ansiStatus = SocketAddress_isV4mappable(tmpSocketAddr);
        break;
      }
    }
  } while(0);

  return ansiStatus;
}

int AddrInfo_convert(const void *in_addrInfo,
                     SocketAddress **out_socketAddress, InetAddr **out_inetAddr)
{
  int ansiStatus = ANSI_FAILED;
  SocketAddress *socketAddress = NULL;
  InetAddr *inetAddr = NULL;

  const Addr_Info *addrInfo = (const Addr_Info *) in_addrInfo;

  do {
    if ((NULL == addrInfo) ||
        ((NULL == out_socketAddress) && (NULL == out_inetAddr)))
    {
      break;
    }

    int family = addrInfo->ai_family;

    if (out_socketAddress) {
      socketAddress = (SocketAddress*) safeMalloc31(sizeof(SocketAddress), "socketAddress");
      memcpy(socketAddress, addrInfo->ai_addr, addrInfo->ai_addrlen);
      *out_socketAddress = socketAddress;

      if (addrInfoTrace) {
        printf("AddrInfo_convert output SocketAddress len=%d family=%d\n",
               (int) (socketAddress->length), (int) (socketAddress->family));
        dumpbuffer((char*)socketAddress, socketAddress->length);
      }
    }

    if (out_inetAddr) {
      inetAddr = (InetAddr*) safeMalloc31(sizeof(InetAddr), "inetAddr");
      inetAddr->type = addrInfo->ai_addr->family;
      inetAddr->port = (int) addrInfo->ai_addr->port;
      if (AF_INET6 == family) {
        memcpy(&(inetAddr->data), &(addrInfo->ai_addr->data6), sizeof(addrInfo->ai_addr->data6));
      } else {
        memcpy(&(inetAddr->data), &(addrInfo->ai_addr->v4Address), sizeof(addrInfo->ai_addr->v4Address));
      }
      *out_inetAddr = inetAddr;

      if (addrInfoTrace) {
        printf("AddrInfo_convert output InetAddr family=%d port=%d\n",
               inetAddr->type, inetAddr->port);
        dumpbuffer((char*)inetAddr, sizeof(InetAddr));
      }
    }

    ansiStatus = ANSI_OK;

  } while(0);

  return ansiStatus;
}

int AddrInfo_convert_IPv4(const void *in_addrInfo,
                          SocketAddress **out_socketAddress, InetAddr **out_inetAddr)
{
  int ansiStatus = ANSI_FAILED;
  SocketAddress *socketAddress = NULL;
  InetAddr *inetAddr = NULL;

  const Addr_Info *addrInfo = (const Addr_Info *) in_addrInfo;

  do {
    if ((NULL == addrInfo) ||
        ((NULL == out_socketAddress) && (NULL == out_inetAddr)))
    {
      break;
    }

    if (ANSI_FALSE == AddrInfo_isV4mappable(addrInfo)) {
      break;
    }

    char* sourceData = NULL;

    switch(addrInfo->ai_family)
    {
      case AF_INET:
      {
        sourceData = &(addrInfo->ai_addr->v4Addr[0]);
        if (addrInfoTrace) {
          printf("AddrInfo_convert_IPv4 from AF_INET source data\n");
        }
        break;
      }

      case AF_INET6:
      {
        sourceData = &(addrInfo->ai_addr->data6.addrData[12]); //see data6_isV4mappable
        if (addrInfoTrace) {
          printf("AddrInfo_convert_IPv4 from AF_INET6 source data\n");
        }
        break;
      }
    }

    if (NULL == sourceData)
      break;

    if (out_socketAddress) {
      socketAddress = (SocketAddress*) safeMalloc31(sizeof(SocketAddress), "socketAddress");
      socketAddress->length = 14; /* see SOCK_SIN#LEN in BPXYSOCK */
      socketAddress->family = AF_INET;

      memcpy(&(socketAddress->v4Addr[0]), sourceData, 4);

      *out_socketAddress = socketAddress;

      if (addrInfoTrace) {
        printf("AddrInfo_convert_IPv4 output SocketAddress len=%d family=%d\n",
               (int) (socketAddress->length), (int) (socketAddress->family));
        dumpbuffer((char*)socketAddress, socketAddress->length);
      }
    }

    if (out_inetAddr) {
      inetAddr = (InetAddr*) safeMalloc31(sizeof(InetAddr), "inetAddr");
      inetAddr->type = AF_INET;

      memcpy(&(inetAddr->data.data4.addrBytes), sourceData, 4);

      *out_inetAddr = inetAddr;

      if (addrInfoTrace) {
        printf("AddrInfo_convert_IPv4 output InetAddr family=%d port=%d\n",
               inetAddr->type, inetAddr->port);
        dumpbuffer((char*)inetAddr, sizeof(InetAddr));
      }
    }

    ansiStatus = ANSI_OK;

  } while(0);

  return ansiStatus;
}

/**
 * @brief       Count the AddrInfo entries in addrInfoList
 *
 * @detail      Walk the linked list to determine its length.
 *              If addrInfoList is NULL, returns 0.
 *              If addrInfoList is non-NULL, returns >= 1.
 *              DO NOT pass an address that wasn't returned by getAddressInfoList.
 *
 * @param[in]     addrInfoList  A handle returned by getAddressInfoList
 * @param[out]    v4mappable    The number of entries that are either AF_INET or
 *                              mappable from AF_INET6 to AF_INET.
 */
int AddrInfoList_count(AddrInfoList addrInfoList, int *out_v4mappable)
{
  const int ADDRINFOLIST_COUNT_MAX = 100; /* prevent infinite loops */
  int count = 0;
  int v4mappable = 0;

  do {
    if (NULL == addrInfoList)
      break; /* count = 0 */

    count++; /* count = 1 */

    Addr_Info *tmpAddrInfo = (Addr_Info*) addrInfoList;
    if (AddrInfo_isV4mappable(tmpAddrInfo)) {
      ++v4mappable;
    }

    while((NULL != tmpAddrInfo->ai_next) &&
          (count < ADDRINFOLIST_COUNT_MAX))
    {
      ++count;
      tmpAddrInfo = tmpAddrInfo->ai_next;
      if (AddrInfo_isV4mappable(tmpAddrInfo)) {
        ++v4mappable;
      }
    }

  } while(0);

  if (out_v4mappable) {
    *out_v4mappable = v4mappable;
  }

  if (addrInfoTrace) {
    printf("AddrInfoList_count returning %d (%d v4mappable) for AddrInfoList @ 0x%p\n",
           count, v4mappable, addrInfoList);
  }

  return count;
}

/**
 * @brief       Return the AddrInfo handle at the specified index
 *
 * @detail      Walk the linked list to the specified index, and return
 *              the associated AddrInfo handle
 *
 * @param[in]     addrInfoList    A handle returned by getAddressInfoList
 * @param[in]     index           A value between 0 and count-1 (see AddrInfoList_count)
 * @param[in,out] out_AddrInto    A pointer to an opaque AddrInfo is returned in this location.
 *
 * @return      ANSI_OK if an entry was found/returned from the specified index.
 *              ANSI_FAILED if the index was >= the number of items in the list.
 */
int AddrInfoList_getAtIndex(AddrInfoList addrInfoList, int index,
                            void* *out_AddrInfo)
{
  int ansiStatus = ANSI_FAILED;
  SocketAddress *socketAddress = NULL;
  InetAddr *inetAddr = NULL;

  do {
    if (addrInfoTrace) {
      printf("AddrInfoList_getAtIndex entry (addrInfoList=0x%p, index=%d, outAddrInfo=0x%p)\n",
             addrInfoList, index, out_AddrInfo);
    }

    if ((NULL == addrInfoList) ||
        (0 > index) ||
        (NULL == out_AddrInfo))
    {
      break;
    }

    Addr_Info *firstAddrInfo = (Addr_Info*) addrInfoList;
    Addr_Info *tmpAddrInfo = firstAddrInfo;
    int count = 0;
    int family = 0;

    do {
      if (index == count) {
        family = tmpAddrInfo->ai_family;
        if ((AF_INET6 == family) || (AF_INET == family)) {
          ansiStatus = ANSI_OK;
        }
        break;
      }
      count++;
      tmpAddrInfo = tmpAddrInfo->ai_next;
    } while (NULL != tmpAddrInfo);

    if (ANSI_OK != ansiStatus)
      break;

    *out_AddrInfo = tmpAddrInfo;

  } while(0);

  return ansiStatus;
}


void freeAddressInfoList(AddrInfoList addrInfoList)
{
  int rv=0, rc=0, rsn=0;
  if (addrInfoList) {
    BPXFAI(&addrInfoList,
           &rv,
           &rc,
           &rsn);
    if (addrInfoTrace) {
      printf("BPXFAI returned rv:%d, rc: %d, rsn:0x%x\n", rv, rc, rsn);
    }
  }
}

int setSocketTrace(int toWhat) {
  int was = socketTrace;
#ifndef METTLE
  socketTrace = toWhat;
#endif
  return was;
}


unsigned int sleep(unsigned int seconds){
  int returnValue;
  int *returnValuePtr;
  
#ifndef _LP64
  returnValuePtr = (int*) (0x80000000 | ((int)&returnValue));
#else
  returnValuePtr = &returnValue;
#endif
  BPXSLP(seconds,returnValuePtr); 
  return returnValue;
}

void bpxSleep(int seconds)
{
  sleep(seconds);
}

int socketInit(char *uniqueName){
  /* do nothing for now */
  return 0;
}

static SocketAddress *makeSocketAddrIPv4(InetAddr *addr,
                                         unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc31(sizeof(SocketAddress),"SocketAddress");
  memset(address,0,sizeof(SocketAddress));
  if (socketTrace){
    printf("socket address at 0x%x\n",address);
  }
  address->length = 14; /* see SOCK_SIN#LEN in BPXYSOCK */
  address->family = AF_INET;
  address->port = port;
  if (addr){
    address->v4Address = addr->data.data4.addrBytes;
  }
  if (socketTrace){
    printf("makeSocketAddrIPv4: returning socket address at 0x%x\n",address);
  }
  return address;
}

SocketAddress *makeSocketAddr(InetAddr *addr,
                              unsigned short port)
{
  if ((NULL == addr) || (AF_INET == addr->type)) {
    return makeSocketAddrIPv4(addr,port);
  } else {
    return makeSocketAddrIPv6(addr,port);
  }
}

SocketAddress *makeSocketAddrIPv6(InetAddr *addr, unsigned short port){
  SocketAddress *address = (SocketAddress*)safeMalloc31(sizeof(SocketAddress),"SocketAddress");
  memset(address,0,sizeof(SocketAddress));
  if (socketTrace){
    printf("socket address at 0x%x\n",address);
  }
  address->length = 0; /* BPXYSOCK: "Specifically for AF_INET6, SOCK_LEN can either be defined as zero or SOCK#LEN+SOCK_SIN6#LEN" */
  address->family = AF_INET6;
  address->port = port;
  if (addr){
    if (AF_INET != addr->type) {   /* Copy IPv6 address */
      address->data6 = addr->data.data6;
    } else {     /* Create IPv6 address from IPv4 address */
      address->datamap.addrType = ADDR_TYPE_MAPPED;
      address->datamap.addrData = addr->data.data4.addrBytes;
    }
  }
  if (socketTrace){
    printf("makeSocketAddrIPv6: returning socket address at 0x%x\n",address);
  }
  return address;
}


/* SD or, on windows the handle */
int getSocketDebugID(Socket *s){
  return s->sd;
}


/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/
#define SO_ERROR    0x1007

Socket *tcpClient3(SocketAddress *socketAddress,
         int connectTimeoutInMillis,
         int tlsFlags,
         int *returnCode,
         int *reasonCode) {
/* BPX1SOC,(Domain,
              Type,                                                  
              Protocol,
              Dimension,
              Socket_vector,
              Return_value,
              Return_code,
              Reason_code)
    */
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int family = (int) socketAddress->family;
  int socketAddrSize = (AF_INET6 == family) ? SOCKET_ADDRESS_SIZE_IPV6 : SOCKET_ADDRESS_SIZE_IPV4;

  BPXSOC(family,
         SOCTYPE_STREAM,
         IPPROTO_TCP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace){
    printf("BPXSOC returnValue %d returnCode %d reasonCode %d\n",
           returnValue,*returnCode,*reasonCode);
    printf("socketVector[0] = %d\n",socketVector[0]);
  }
  if (returnValue != 0){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;
    if (connectTimeoutInMillis >= 0){
      Socket tempSocket;
      tempSocket.sd = socketVector[0];

      setSocketBlockingMode(&tempSocket, TRUE, returnCode, reasonCode);
      BPXCON(socketVector,
              &socketAddrSize,
              socketAddress,
              &returnValue,
              returnCode,
              reasonCodePtr);
      if (socketTrace) {
        printf("BPXCON returnValue %d returnCode %x reasonCode %x\n",
                returnValue, *returnCode, *reasonCode);
      }
      /* EINPROGRESS is the expected return code here but we are just being careful by checking
         for EWOULDBLOCK as well
      */
      if (returnValue){
        returnValue = 0;
        *returnCode  = 0;
        *reasonCode  = 0;
        int status = tcpStatus(&tempSocket, connectTimeoutInMillis, 1, returnCode, reasonCode);
        if (status == SD_STATUS_TIMEOUT) {
          int sd = socketVector[0];
          if (socketTrace) {
            printf("Failed to connect socket, will clean up sd=%d\n", sd);
          }
          returnValue = 0;
          *returnCode = 0;
          *reasonCode = 0;
          BPXCLO(&sd,
                  &returnValue,
                  returnCode,
                  reasonCodePtr);
          if (socketTrace) {
            printf("BPXCLO for time out connect returnValue %d returnCode %d reasonCode %d\n",
                    returnValue, *returnCode, *reasonCode);
          }
	
          return NULL;
        }
        *returnCode  = 0;
        *reasonCode  = 0;
        setSocketBlockingMode(&tempSocket, FALSE, returnCode, reasonCode);
        int optionLength = sizeof(int);
        int optionData = 0;
        *returnCode  = 0;
        *reasonCode  = 0;
        getSocketOption(&tempSocket, SO_ERROR, &optionLength, (char *)&optionData, returnCode, reasonCode);
        if( optionData > 0 )
        {
          if (socketTrace){
            printf("Failed to connect socket, returnCode %d\n", optionData);
          }
          *returnCode = optionData;
          returnValue = -1;
        }
        else
        {
          returnValue = 0;
        }
      } else{
        /* all was good on 1st try, but why aren't we setting blocking mode here?
           seems inconsistent */
      }
    }
    else{
      BPXCON(socketVector,
              &socketAddrSize,
              socketAddress,
              &returnValue,
              returnCode,
              reasonCodePtr);
      if (socketTrace){
        printf("BPXCON returnValue %d returnCode %x reasonCode %x\n",
           returnValue,*returnCode,*reasonCode);
      }
    }
    if (returnValue){
      int sd = socketVector[0];

      if (socketTrace){
        printf("Failed to connect socket, will clean up sd=%d\n",sd);
      }
      *returnCode  = 0;
      *reasonCode  = 0;
      BPXCLO(&sd,
          &returnValue,
          returnCode,
          reasonCodePtr);
      if (socketTrace){
        printf("BPXCLO for failed connect returnValue %d returnCode %d reasonCode %d\n",
          returnValue,*returnCode,*reasonCode);
      }
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->sd = socketVector[0];
      socket->pipeOutputSD = 0;  /* nothing, not a reference to STDIN */
      snprintf(socket->debugName,SOCKET_DEBUG_NAME_LENGTH,"SD=%d",socket->sd);
      socket->isServer = 0;
      socket->tlsFlags = tlsFlags;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_TCP;
      return socket;
    }
  }                    
}

Socket *tcpClient2(SocketAddress *socketAddress,
       int connectTimeoutInMillis,
       int *returnCode, /* errnum */
       int *reasonCode) { /* errnum - JR's */
  return tcpClient3(socketAddress,
                    connectTimeoutInMillis,
                    0,
                    returnCode,
                    reasonCode);
}

Socket *tcpClient(SocketAddress *socketAddress,
       int *returnCode, /* errnum */
       int *reasonCode){ /* errnum - JR's */
  return tcpClient2(socketAddress,-1,returnCode,reasonCode);
}

Socket *makePipeBasedSyntheticSocket(int protocol, int inputFD, int outputFD){
  Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
  socket->sd = inputFD;
  socket->pipeOutputSD = outputFD;
  snprintf(socket->debugName,SOCKET_DEBUG_NAME_LENGTH,"PIPE(%d,%d)",inputFD,outputFD);
  socket->isServer = 0;
  socket->tlsFlags = 0;
  socket->protocol = protocol;
  return socket;
}

void socketFree(Socket *socket){
  safeFree((char*)socket,sizeof(Socket));
}

Socket *udpPeer(SocketAddress *socketAddress, 
                int *returnCode, /* errnum */
                int *reasonCode){  /* errnum - JR's */
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  
  int family = (int) socketAddress->family;

  BPXSOC(family,
         SOCTYPE_DATAGRAM,
         IPPROTO_UDP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace){
    printf("BPXSOC returnValue %d returnCode %d reasonCode %d\n",
	   returnValue,*returnCode,*reasonCode);
    printf("socketVector[0] = %d\n",socketVector[0]);
  }
  if (returnValue != 0){
    if (socketTrace){
      printf("Failed to create socket\n");
    }
    return NULL;
  } else{
    int sd = socketVector[0];
    int socketAddressSize = (AF_INET6 == family) ? SOCKET_ADDRESS_SIZE_IPV6 : SOCKET_ADDRESS_SIZE_IPV4;

    BPXBND(&sd,
           &socketAddressSize,
           socketAddress,
           &returnValue,
           returnCode,
           reasonCodePtr);
    if (socketTrace) {
      printf("BPXBND returnValue %d returnCode %x reasonCode %x\n",
             returnValue, *returnCode, *reasonCode);
    }
    if (returnValue != 0){
      return NULL;
    } else{
      Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
      socket->sd = socketVector[0];
      sprintf(socket->debugName,"SD=%d",socket->sd);
      socket->isServer = 0;
      /* manual says returnCode and value only meaningful if return value = -1 */
      *returnCode = 0;
      *reasonCode = 0;
      socket->protocol = IPPROTO_UDP;
      return socket;
    }
  }
}

/* Forward decl */
static int setSocketReuseAddr(int sd, int *returnCode, int *reasonCode);

Socket *tcpServer2(InetAddr *addr,
                   int port,
                   int tlsFlags,
                   int *returnCode,
                   int *reasonCode)
{
  int socketVector[2];
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int status;
  int *reasonCodePtr;
  int inet = 0;
  if (addr != NULL)
  {
    inet = addr->type;
  }
  else
  {
    inet = AF_INET6;
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSOC(inet,
         SOCTYPE_STREAM,
         IPPROTO_TCP,
         1,
         socketVector,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (returnValue != 0) {
    if (*returnCode == EAFNOSUPPORT && (inet == AF_INET6)){
      /* Fall back to IPv4 - see https://www-304.ibm.com/servers/resourcelink/svc00100.nsf/pages/zosv2r3sc273663/$file/hale001_v2r3.pdf */
      if (socketTrace) {
        printf("IPv6 is not enabled on this system.  Trying an IPv4 socket.\n");
      }
      inet = AF_INET;
      *returnCode = 0; *reasonCode = 0;
      status = BPXSOC(inet,
                      SOCTYPE_STREAM,
                      IPPROTO_TCP,
                      1,
                      socketVector,
                      &returnValue,
                      returnCode,
                      reasonCodePtr);
    }
    if (returnValue != 0) {
      if (socketTrace) {
        printf("Failed to create socket\n");
      }
      return NULL;
    } else {
      if (socketTrace) {
        printf("Socket created successfully!\n");
      }
    }
  }
  if (returnValue == 0) {
    int sd = socketVector[0];
    int socketAddressSize = (inet == AF_INET6) ? SOCKET_ADDRESS_SIZE_IPV6 : SOCKET_ADDRESS_SIZE_IPV4;
    int inetAddressSize = sizeof(InetAddr);
    SocketAddress *socketAddress = (inet == AF_INET6) ? makeSocketAddrIPv6(addr, (unsigned short)port) : makeSocketAddr(addr, (unsigned short)port);


    if (socketTrace){
      printf("server SocketAddress = %d\n",sd);
      dumpbuffer((char*)socketAddress,sizeof(SocketAddress));
      dumpbuffer((char*)addr,sizeof(InetAddr));
    }
    setSocketReuseAddr(sd,
                       returnCode,
                       reasonCodePtr);
    if (returnValue != 0){
      if (socketTrace){
        printf("Failed to set SO_REUSEADDR errno=%d reason=0x%x\n",
               *returnCode,*reasonCode);
      }
    }
    BPXBND(&sd,
           &socketAddressSize,
           socketAddress,
           &returnValue,
           returnCode,
           reasonCodePtr);
    if (returnValue != 0){
      if (socketTrace){
        printf("Failed to bind server socket errno=%d reason=0x%x\n",
               *returnCode,*reasonCode);
      }
      safeFree31((char*)socketAddress,sizeof(SocketAddress));
      return NULL;
    } else{
      int backlogQueueLength = 100;
      BPXLSN(&sd,
             &backlogQueueLength,
             &returnValue,
             returnCode,
             reasonCodePtr);
      if (returnValue){
        if (socketTrace){
          printf("Failed to start server socket listen errno=%d reason=0x%x\n",
                 *returnCode,*reasonCode);
        }
        return NULL;
      } else {
        Socket *socket = (Socket*)safeMalloc(sizeof(Socket),"Socket");
        /* manual says returnCode and value only meaningful if return value = -1 */
        *returnCode = 0;
        *reasonCode = 0;

        socket->sd = sd;
        sprintf(socket->debugName,"SD=%d",socket->sd);
        socket->isServer = 1;
        socket->tlsFlags = tlsFlags;
        socket->protocol = IPPROTO_TCP;
        if (socketTrace) {
          printf("Immediately before return from tcpServer, socket contains:\n");
          dumpbuffer((char*)socket, sizeof(Socket));
        }
        return socket;
      }
    }
  }
  return NULL;
}

Socket *tcpServer(InetAddr *addr, /* usually NULL/0 */
                  int port,
                  int *returnCode,
                  int *reasonCode){
  return tcpServer2(addr, port, 0, returnCode, reasonCode);
}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/


Socket *socketAccept(Socket *serverSocket, int *returnCode, int *reasonCode){
  int sd = serverSocket->sd;
  int status;
  int socketAddressSize = 0; // We do not care what the socket address is                                                                                                                                                                                      
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXACP(&sd,
         &socketAddressSize,
         NULL,             
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (returnValue == -1){
    return NULL;
  } else{
    Socket *socket = (Socket*)safeMalloc31(sizeof(Socket),"ServerSideSocket");
    socket->sd = returnValue;
    sprintf(socket->debugName,"SD=%d",socket->sd);
    socket->isServer = 0;
    socket->protocol = IPPROTO_TCP;
    socket->tlsFlags = serverSocket->tlsFlags;
    return socket;
  }
}

#define DOT 1
#define FIRST 2
#define SECOND 3
#define THIRD 4

int isV4Numeric(char *chars, int *addr){
  int i,len = strlen(chars);
  int state = DOT;
  int dotCount = 0;
  int value;
  int addressValue = 0;
  for (i=0; i<len; i++){
    char ch = chars[i];
    switch (state){
    case DOT:
      if (ch >= '0' && ch <= '9'){
        if (dotCount == 0 && ch == '0')
          return 0;
        else
          state = FIRST;
      } else {
        return 0;
      }
      break;
    case FIRST:
      if (ch >= '0' && ch <= '9'){
        state = SECOND;
      } else if (ch == '.'){
        state = DOT;
        value = (chars[i-1] - '0');
        addressValue = (addressValue << 8) | value;
        dotCount++;
      } else{
        return 0;
      }
      break;
    case SECOND:
      if (ch >= '0' && ch <= '9'){
        state = THIRD;
      } else if (ch == '.'){
        state = DOT;
        value = ((10 * (chars[i-2] - '0')) +
                 (chars[i-1] - '0'));
        addressValue = (addressValue << 8) | value;     
        dotCount++;
      } else{
        return 0;
      }
      break;
    case THIRD:
      if (ch >= '0' && ch <= '9'){
        return 0;
      } else if (ch == '.'){
        state = DOT;
        value = ((100 * (chars[i-3] - '0')) +
                 (10 * (chars[i-2] - '0')) +
                 (chars[i-1] - '0'));
        addressValue = (addressValue << 8) | value;
        if (value < 256)
          dotCount++;
        else
          return 0;
      } else{
        return 0;
      }
      break;
    }
  }
  if (dotCount == 3){
    switch (state){
    case FIRST:
      value = (chars[i-1] - '0');
      break;
    case SECOND:
      value = ((10 * (chars[i-2] - '0')) +
               (chars[i-1] - '0'));
      break;
    case THIRD:
      value = ((100 * (chars[i-3] - '0')) +
               (10 * (chars[i-2] - '0')) +
               (chars[i-1] - '0'));
      break;
    }
    addressValue = (addressValue << 8) | value;
    *addr = addressValue;
    return (value < 256);
  } else{
    return 0;
  }
}

char* getV4HostEntByName(char *string, int* rc, int* rsn){
  int returnValue = 0;
  int *reasonCodePtr;
  int len = strlen(string);
  char *hostEntPtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)rsn));
#else
  reasonCodePtr = rsn;
#endif
  status = BPXGHN(string,
                  &len,
                  &hostEntPtr,
                  &returnValue,
                  rc,
                  reasonCodePtr);
  if (socketTrace){
    printf("hostent addr = %x\n",*((int*)hostEntPtr));
  }

  return hostEntPtr;
}

int getV4HostByName(char *string){
  int returnValue = 0;
  int returnCode = 0;
  int reasonCode = 0;
  int *reasonCodePtr;
  int len = strlen(string);
  char *hostEntPtr;
  int status;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
  reasonCodePtr = &reasonCode;
#endif
  BPXGHN(string,
         &len,
         &hostEntPtr,
         &returnValue,
         &returnCode,
         reasonCodePtr);
  /* TBD: Check return codes */
  if (socketTrace){
    printf("hostent addr = %x\n",*((int*)hostEntPtr));
  }
  if (hostEntPtr){
    Hostent *hostent = (Hostent*)hostEntPtr;
    int i;
    int numericAddress = 0;
    /* dumpbuffer((char*)hostent,20); */
    for (i=0; i<hostent->length; i++){
      if (socketTrace){
        printf("  addr[%d] = 0x%p\n",i,hostent->addrList[i]);
      }
      if (hostent->addrList[i]){
        numericAddress = *(hostent->addrList[i]);
        break;
      }
    }
    return numericAddress;
  } else{
    return 0;
  }
}

#define FIOASYNC 0x8004A77D

int tcpIOControl(Socket *socket, int command, int argumentLength, char *argument, 
                 int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  /* printf("before read sd=%d buffer=%x\n",sd,buffer); */
  BPXIOC(&sd,
          &command,
          &argumentLength,
          argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }


}

/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

#define SOCKET_MASK_SIZE 4               
#define MAX_SD_NUMBER (32*sizeof(int))   

static int tcpStatusInternal(Socket *socket, int timeout, 
                             int checkWrite, int checkRead,
                             int *returnCode, int *reasonCode){
  /* what does it MEAN?? */
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int userOptionField = 0; /* bits backward */
  int maxSoc = sd+1;
  int socketMaskSize = SOCKET_MASK_SIZE;
  int wordIndex = sd / (8*sizeof(int));
  int activeLength = (sd+32)/32;
  int wordPos = activeLength-(1+wordIndex);
  int bitIndex = sd-(wordIndex*sizeof(int));
  int rsndmsk[SOCKET_MASK_SIZE];
  int wsndmsk[SOCKET_MASK_SIZE];
  int esndmsk[SOCKET_MASK_SIZE];
  int timeoutSpec[4];
  int *timeoutSpecPtr;
  int **timeoutSpecHandle;
  
  memset(rsndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  memset(wsndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  memset(esndmsk,0,SOCKET_MASK_SIZE*sizeof(int));
  if (timeout >= 0) {
#ifndef _LP64
    timeoutSpec[0] = timeout/1000; /* millis to seconds */
    timeoutSpec[1] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#else
    timeoutSpec[0] = 0; 
    timeoutSpec[1] = timeout/1000; /* millis to seconds */
    timeoutSpec[2] = 0; /* padding */
    timeoutSpec[3] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#endif

    timeoutSpecPtr = timeoutSpec;
  } else{
    // setting timeout_pointer to 0 implies wait indefinitely
    timeoutSpecPtr = 0; 
  }
  timeoutSpecHandle = &timeoutSpecPtr;

  if (socketTrace){
    printf("tcpStatus sd=%d wordPos %d bitIndex %d maxSoc %d al %d\n",
          sd,wordPos,bitIndex,maxSoc,activeLength);
  }
  if (checkRead){
    rsndmsk[wordPos] = 1 << bitIndex;
  }
  esndmsk[wordPos] = 1 << bitIndex;
  if (checkWrite){
    wsndmsk[wordPos] = 1 << bitIndex;
  }
  
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXSEL(&maxSoc,
         (checkRead  ? &socketMaskSize : &zero),
         rsndmsk,
         (checkWrite ? &socketMaskSize : &zero),
         wsndmsk,
         &socketMaskSize,
         esndmsk,
         timeoutSpecHandle,
         &zero, /* ecb */
         &userOptionField,
         &returnValue,
         returnCode,
         reasonCodePtr);
  
  if (socketTrace){
    printf("BPXSEL (single) maxSoc=0x%x returnValue 0x%x returnCode 0x%x reasonCode 0x%x\n",
           maxSoc,returnValue,*returnCode,*reasonCode);
  }
  if (returnValue > 0){                            
    int sdStatus = 0;                           
                                                
    if (rsndmsk[wordPos])                       
      sdStatus  |= SD_STATUS_RD_RDY;            
    if (wsndmsk[wordPos])                       
      sdStatus  |= SD_STATUS_WR_RDY;            
    return sdStatus;                            
  } else if (returnValue == 0){ /* timeout */      
    return SD_STATUS_TIMEOUT;                   
  } else if (esndmsk[wordPos]){                
    return SD_STATUS_ERROR;                     
  } else{                                      
    return SD_STATUS_FAILED; /* look in errno */
  }                                             
}

int tcpStatus(Socket *socket, 
              int timeout,   /* in milliseconds */
              int checkWrite, /* otherwise check for read and error */
              int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,!checkWrite,returnCode,reasonCode);
}

int tcpStatus2(Socket *socket, 
               int timeout,   /* in milliseconds */
               int checkWrite, int checkRead,
               int *returnCode, int *reasonCode){
  return tcpStatusInternal(socket,timeout,checkWrite,checkRead,returnCode,reasonCode);
}

#define O_NONBLOCK   0x04
#define F_GETFL       3
#define F_SETFL       4

int setSocketBlockingMode(Socket *socket, int isNonBlocking,
                          int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int *reasonCodePtr;

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int action = F_GETFL;
  int argument = 0;

  BPXFCT(&sd,
          &action,
          &argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  
  if (returnValue < 0){
    return -1;
  } else {
    if (socketTrace){
      printf("BPXFCT value %d returnValue %d \n",isNonBlocking, returnValue);
    }
    *returnCode = 0;
    *reasonCode = 0;
  }

  action = F_SETFL;
  if (isNonBlocking)
  {
    argument = returnValue | O_NONBLOCK;
  }
  else
  {
    argument = returnValue & ~(O_NONBLOCK);
  }
  returnValue = 0;
  
  BPXFCT(&sd,
          &action,
          &argument,
          &returnValue,
          returnCode,
          reasonCodePtr);
  
  if (returnValue < 0){
    return returnValue;
  } else {
    if (socketTrace){
      printf("BPXFCT value %d returnValue %d \n",isNonBlocking, returnValue);
    }
    /* this seems bogus 
     *returnCode = 0;
     *reasonCode = 0;
     */
    return returnValue;
  }
}

int socketRead(Socket *socket, char *buffer, int desiredBytes, 
               int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is an RS_SSL_CONNECTION */
    int bytesRead = 0;
    status = rs_ssl_read(socket->sslHandle, buffer, desiredBytes, &bytesRead);
    if (0 != status) {
      if (socketTrace){
        printf("socketRead: rs_ssl_read failed with status: %d\n", status);
      }
      return -1;
    } else {
      return bytesRead;
    }
  }
#endif
#ifdef USE_ZOWE_TLS
  if (NULL != socket->tlsSocket) {
    int bytesRead = 0;
    status = tlsRead(socket->tlsSocket, buffer, desiredBytes, &bytesRead);
    if (0 != status) {
      if (socketTrace){
        printf("socketRead: tlsRead failed with status: %d (%s)\n", status, tlsStrError(status));
      }
      return -1;
    } else {
      return bytesRead;
    }
  }
#endif

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXRED(&sd,
          &buffer,
          &zero,
          &desiredBytes,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    return -1;
  } else {
    if (socketTrace > 2){
      printf("read %d bytes\n",returnValue);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int socketWrite(Socket *socket, const char *buffer, int desiredBytes, 
	       int *returnCode, int *reasonCode){
  int status = 0;
  int sd = (IS_SYNTHETIC_PIPE(socket->protocol) ?
            socket->pipeOutputSD :
            socket->sd);
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;

#ifdef USE_RS_SSL
  if (NULL != socket->sslHandle) { /* sslHandle is RS_SSL_CONNECTION in this case */
    int bytesWritten = 0;
    status = rs_ssl_write(socket->sslHandle, buffer, desiredBytes, &bytesWritten);
    if (0 != status) {
      if (socketTrace){
        printf("socketWrite: rs_ssl_write failed with status: %d\n", status);
      }
      return -1;
    } else {
      return bytesWritten;
    }
  }
#endif
#ifdef USE_ZOWE_TLS
  if (NULL != socket->tlsSocket) {
    int bytesWritten = 0;
    status = tlsWrite(socket->tlsSocket, buffer, desiredBytes, &bytesWritten);
    if (0 != status) {
      if (socketTrace){
        printf("socketWrite: tlsWrite failed with status: %d (%s)\n", status, tlsStrError(status));
      }
      return -1;
    } else {
      return bytesWritten;
    }
  }
#endif // USE_ZOWE_TLS

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXWRT(&sd,
	  &buffer,
	  &zero,
	  &desiredBytes,
	  &returnValue,
	  returnCode,
	  reasonCodePtr);
  if (socketTrace > 2){
    printf("SocketWrite writing desired=%d retVal=%d retCode=%d reasonCode=%d\n",
	   desiredBytes,returnValue,*returnCode,*reasonCode);
    fflush(stdout);
  }
  if (returnValue < 0){
    /* using MVD Over VPN, I get a massive stream of the following in the log:
     * "write failed ret code 1102 reason 0x76690000"
     * so I think we should only see those if socketTrace is TRUE
     */
    if ((socketTrace > 2) || !(*returnCode == 1102 && *reasonCode == 0x76690000)){
      printf("write failed ret code %d reason 0x%x\n",*returnCode,*reasonCode);
    }
    return -1;
  } else {
    if (socketTrace > 2){
      printf("wrote %d bytes\n",returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int udpSendTo(Socket *socket, 
              SocketAddress *destinationAddress,
              char *buffer, int desiredBytes, 
              int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int flags = 0;  /* some exotic stuff in doc
                     http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/bpxzb1c0/B.30?SHELF=all13be9&DT=20110609191818#HDRYMSGF
                   */
  int socketAddressSize = SOCKET_ADDRESS_SIZE_IPV4;

  if (socketTrace > 2){
    printf("sendTo desired=%d retVal=%d retCode=%d reasonCode=%d\n",
       desiredBytes,returnValue,*returnCode,*reasonCode);
    dumpbuffer(buffer, desiredBytes); 
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXSTO(&sd,
          &desiredBytes,
          buffer,
          &zero,
          &flags,
          &socketAddressSize,
          destinationAddress,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    return -1;
  } else {
    if (socketTrace > 2){
      printf("send %d bytes\n",returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int getSocketOption(Socket *socket, int optionName, int *optionDataLength, char *optionData,
        int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  int level = 0x0000FFFF;


#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  if (socketTrace){
    printf("before get socket option optionName=0x%x dataBufferLen=%d\n",optionName,*optionDataLength);
  }
  BPXOPT(&sd,
   &getOption,
   &level,
   &optionName,
   optionDataLength,
   optionData,
   &returnValue,
   returnCode,
   reasonCodePtr);
  if (socketTrace){
    printf("after getsockopt, retval=%d ret code %d reason 0x%x\n",returnValue,*returnCode,*reasonCode);
  }
  if (returnValue < 0){
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

static int setSDOption(int sd, int level, int optionName, int optionDataLength, char *optionData,
		       int *returnCode, int *reasonCode){
  int status = 0;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int getOption = 1;
  int setOption = 2;
  int setIBMOption = 3;
  

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXOPT(&sd,
	 &setOption,
	 &level,
	 &optionName,
	 &optionDataLength,
	 optionData,
	 &returnValue,
	 returnCode,
	 reasonCodePtr);
  if (returnValue < 0){
    return -1;
  } else {
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

int setSocketOption(Socket *socket, int level, int optionName, int optionDataLength, char *optionData,
		    int *returnCode, int *reasonCode){
  return setSDOption(socket->sd,level,optionName,optionDataLength,optionData,returnCode,reasonCode);
}


int setSocketNoDelay(Socket *socket, int noDelay, int *returnCode, int *reasonCode){
  return setSocketOption(socket,IPPROTO_TCP,TCP_NODELAY,sizeof(int),(char*)&noDelay,returnCode,reasonCode);
}

int setSocketWriteBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOption(socket,SOL_SOCKET,SOCK_SO_SNDBUF,sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}

int setSocketReadBufferSize(Socket *socket, int bufferSize, int *returnCode, int *reasonCode){
  return setSocketOption(socket,SOL_SOCKET,SOCK_SO_RCVBUF,sizeof(int),(char*)&bufferSize,returnCode,reasonCode);
}

static int setSocketReuseAddr(int sd, int *returnCode, int *reasonCode){
  int on = 1;
  return setSDOption(sd,SOL_SOCKET,SOCK_SO_REUSEADDR,sizeof(int),(char*)&on,returnCode,reasonCode);
}

int udpReceiveFrom(Socket *socket, 
                   SocketAddress *sourceAddress,  /* will be overwritten */
                   char *buffer, int bufferLength,
                   int *returnCode, int *reasonCode){
  int status = 0;
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int flags = 0;  /* some exotic stuff in doc
                     http://publibz.boulder.ibm.com/cgi-bin/bookmgr_OS390/BOOKS/bpxzb1c0/B.30?SHELF=all13be9&DT=20110609191818#HDRYMSGF
                   */
  int socketAddressSize = SOCKET_ADDRESS_SIZE_IPV4;

  if (socketTrace > 2){
    printf("receiveFrom into buffer=0x%p bufLen=%d retVal=%d retCode=%d reasonCode=%d\n",
           buffer,bufferLength,returnValue,*returnCode,*reasonCode);
  }

#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif

  BPXRFM(&sd,
          &bufferLength,
          buffer,
          &zero,
          &flags,
          &socketAddressSize,
          sourceAddress,
          &returnValue,
          returnCode,
          reasonCodePtr);
  if (returnValue < 0){
    return -1;
  } else {
    if (socketTrace > 2){
      printf("recvFrom into buffer=0x%p %d bytes\n",buffer,returnValue);fflush(stdout);
    }
    *returnCode = 0;
    *reasonCode = 0;
    return returnValue;
  }
}

/**** Socket Sets and Extended Select */

SocketSet *makeSocketSet(int highestAllowedSD){
  SocketSet *set = (SocketSet*)safeMalloc(sizeof(SocketSet),"SocketSet");
  memset(set,0,sizeof(SocketSet));

  set->highestAllowedSD = highestAllowedSD;

  int socketArrayLength = highestAllowedSD+1;
  set->sockets = (Socket**)safeMalloc(socketArrayLength*sizeof(Socket*),"SocketSetSocketArray");
  memset(set->sockets,0,socketArrayLength*sizeof(Socket*));

  int maxArrayLength = (highestAllowedSD+32) / 32;
  set->allSDs = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ReadSendMask");
  memset(set->allSDs,0,sizeof(int)*maxArrayLength);

  set->scratchReadMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ReadSendMask");
  set->scratchWriteMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"WriteSendMask");
  set->scratchErrorMask = (int*)safeMalloc(sizeof(int)*maxArrayLength,"ErrorSendMask");

  return set;
}

void freeSocketAddr(SocketAddress *address){
  safeFree31((char*)address,sizeof(SocketAddress));
}

void freeInetAddr(InetAddr *inetAddr){
  safeFree31((char*)inetAddr, sizeof(InetAddr));
}


void freeSocketSet(SocketSet *set){
  if (socketTrace){
    printf("freeSocketSet: NYI\n");
  }
  return;
}

int socketSetAdd(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if (sd > set->highestAllowedSD){
    return 12;
  }

  int wordIndex = sd/ 32;
  int bitIndex = sd - (32 * wordIndex);

  set->allSDs[wordIndex] |= (1<<bitIndex);
  set->sockets[sd] = socket;

  return 0;
}

int socketSetRemove(SocketSet *set, Socket *socket){
  int sd = socket->sd;

  if (sd > set->highestAllowedSD){
    return 12;
  }

  int wordIndex = sd/ 32;
  int bitIndex = sd - (32 * wordIndex);

  set->allSDs[wordIndex] &= ~(1<<bitIndex);
  set->sockets[sd] = NULL;
  
  return 0;
}

int extendedSelect(SocketSet *set,
                   int timeout,             /* in milliseconds */
                   void *ecbPointer,
                   int checkWrite, int checkRead, 
                   int *returnCode, int *reasonCode){
  int status = 0;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;
  int *reasonCodePtr;
  int zero = 0;
  int userOptionField = 0; /* bits backward */
  int maxSoc = set->highestAllowedSD+1;
  int timeoutSpec[4];
  int *timeoutSpecPtr;
  int **timeoutSpecHandle;

  int maxArrayLengthInWords = (set->highestAllowedSD+32) /  32;
  int socketMaskSize = maxArrayLengthInWords*sizeof(int);
  *returnCode = 0;

  if (socketTrace > 2){
    printf("maxArrayLength in words = 0x%x socketMaskSize=0x%x\n",maxArrayLengthInWords,socketMaskSize);
  }
  memcpy(set->scratchReadMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  memcpy(set->scratchWriteMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  memcpy(set->scratchErrorMask,set->allSDs,maxArrayLengthInWords*sizeof(int));
  
  
  /* set timeout */
  if (timeout >= 0)
  {
#ifndef _LP64
    timeoutSpec[0] = timeout/1000; /* millis to seconds */
    timeoutSpec[1] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#else
    timeoutSpec[0] = 0; 
    timeoutSpec[1] = timeout/1000; /* millis to seconds */
    timeoutSpec[2] = 0; /* padding */
    timeoutSpec[3] = (timeout-(timeout/1000)*1000)*1000; /* micros */
#endif

    timeoutSpecPtr = timeoutSpec;
  }
  else
  {
    // setting timeout_pointer to 0 implies wait indefinitely
    timeoutSpecPtr = 0; 
  }
  timeoutSpecHandle = &timeoutSpecPtr;
  
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
  ecbPointer =    (int*) (0x7FFFFFFF & ((int)ecbPointer));
#else
  reasonCodePtr = reasonCode;
#endif
  BPXSEL(&maxSoc,
         (checkRead ? &socketMaskSize : &zero),
         set->scratchReadMask,
         (checkWrite ? &socketMaskSize : &zero),
         set->scratchWriteMask,
         (checkRead ? &socketMaskSize : &zero),
         set->scratchErrorMask,
         timeoutSpecHandle, /* ptr to ptr */
         &ecbPointer,
         &userOptionField,
         &returnValue,
         returnCode,
         reasonCodePtr);
  if (socketTrace > 1){
    printf("BPXSEL (extended) maxSoc=0x%x returnValue 0x%x returnCode 0x%x reasonCode 0x%x\n",
           maxSoc,returnValue,*returnCode,*reasonCode);
  }
  return returnValue;
}



/*--5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---*/

InetAddr *getAddressByName(char *addressString)
{
  /* retain 'classic' behavior that only supported IPV4 */
  return getAddressByName2(addressString, ANSI_TRUE);
}

InetAddr *getAddressByName2(char* addressString, int ipv4only)
{
  AddrInfoList addrInfoList = NULL;
  int rc=0, rsn=0;
  InetAddr *inetAddr = NULL;

  int count = 0;
  int numV4mappable = 0;
  int ansiStatus = ANSI_OK;

  do {
    addrInfoList = getAddressInfoList(addressString,
                                      NULL, /* serviceName */
                                      NULL, /* ignore canonical name */
                                      &rc, &rsn);
    if (NULL == addrInfoList) {
      break;
    }

    count = AddrInfoList_count(addrInfoList, &numV4mappable);
    if ((0 == count) ||
        ((ipv4only && (0 == numV4mappable))))
    {
      break;
    }

    void* tmpAddrInfo = NULL;

    if (!ipv4only){
      ansiStatus = AddrInfoList_getAtIndex(addrInfoList, 0, &tmpAddrInfo);
      if (ANSI_OK != ansiStatus) {
        break;
      }
      ansiStatus = AddrInfo_convert(tmpAddrInfo, NULL, &inetAddr);
      break;
    }
    if (ANSI_OK != ansiStatus)
      break;

    /* we must be in the ipv4only case */

    /* iterate over the list to find the first V4-mappable entry,
     * then extract it as an AF_INET InetAddr */
    for (int index=0; index < count; index++)
    {
      ansiStatus = AddrInfoList_getAtIndex(addrInfoList, index, &tmpAddrInfo);
      if (ANSI_OK != ansiStatus) {
        break;
      }
      if(AddrInfo_isV4mappable(tmpAddrInfo)) {
        ansiStatus = AddrInfo_convert_IPv4(tmpAddrInfo, NULL, &inetAddr);
        break;
      }
      tmpAddrInfo = NULL;
    }

  } while(0);

  if (addrInfoList) {
    freeAddressInfoList(addrInfoList);
  }

  return inetAddr;
}

int getSocketName(
    Socket *socket,
    SocketAddress *socketAddress)
{
    int returnValue = 0;
    int returnCode;
    int reasonCode;
    int *reasonCodePtr;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
    reasonCodePtr = &reasonCode;
#endif

    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;

    BPXGNM(socket,
            1,
            &socketAddrSize,
            socketAddress,
            &returnValue,
            &returnCode,
            reasonCodePtr);

    if (returnValue == 0){
      return returnValue;
    } else {
      return returnCode;
    }
}

int getSocketName2(
    Socket *socket,
    SocketAddress *socketAddress)
{
    int returnValue = 0;
    int returnCode;
    int reasonCode;
    int *reasonCodePtr;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)&reasonCode));
#else
    reasonCodePtr = &reasonCode;
#endif

    int socketAddrSize = SOCKET_ADDRESS_SIZE_IPV4;

    BPXGNM(socket,
            2,
            &socketAddrSize,
            socketAddress,
            &returnValue,
            &returnCode,
            reasonCodePtr);

    if (returnValue == 0){
      return returnValue;
    } else {
      return returnCode;
    }
}

int getLocalHostName(char* inout_hostname,
                     unsigned int* inout_hostname_len,
                     int *returnCode, int *reasonCode)
{
  int sts = 0;
  *returnCode = *reasonCode = 0;
  int cs_retval = 0;
  int *reasonCodePtr;

  if ((NULL == inout_hostname) ||
      (NULL == inout_hostname_len) ||
      (NULL == returnCode) || (NULL == reasonCode))
  {
    sts = -1;
  }
  else
  {
#ifndef _LP64
  reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
  reasonCodePtr = reasonCode;
#endif    
    BPXHST(AF_INET,
            inout_hostname_len,
            inout_hostname,
            &cs_retval,
            returnCode,
            reasonCode);

    sts = cs_retval;
  }

  return sts;
}

InetAddr* getLocalHostAddress(int *rc, int *rsn)
{
  InetAddr* inetAddr = NULL;
  int cs_retval = 0;
  int v4addr = 0;
  do {
    if ((NULL == rc) || (NULL == rsn)) {
      break;
    }
    BPXHST(AF_INET,
           &v4addr,
           NULL,
           &cs_retval,
           rc,
           rsn);
    if (0 != *rc) {
      break;
    }

    inetAddr = (InetAddr*)safeMalloc(sizeof(InetAddr),"local InetAddr");
    memset(inetAddr,0,sizeof(InetAddr));
    inetAddr->type = AF_INET;
    inetAddr->port = 0;
    inetAddr->data.data4.addrBytes = v4addr;

  } while(0);

  return inetAddr;
}

/* The character set tables and a2e/e2a functions have
   moved to xlate.[hc] */

int socketClose(Socket *socket, int *returnCode, int *reasonCode){
  int sd = socket->sd;
  int returnValue = 0;
  *returnCode = *reasonCode = 0;

#ifdef USE_RS_SSL
  if ((0 == socket->isServer) && (NULL != socket->sslHandle)) {
    /* sslHandle is RS_SSL_CONNECTION in this case */
    /* this call does a C stdlib shutdown/close internally */
    returnValue = rs_ssl_releaseConnection(socket->sslHandle);
    return returnValue;
  }
#endif // USE_RS_SSL
#ifdef USE_ZOWE_TLS
  if (!socket->isServer && socket->tlsSocket) {
    int rc = tlsSocketClose(socket->tlsSocket);
    if (rc != 0 && socketTrace) {
      printf("Error closing tls socket rc=%d (%s)\n", rc, tlsStrError(rc));
    }
    socket->tlsSocket = NULL;
  }
#endif // USE_ZOWE_TLS

  int *reasonCodePtr;
  int status;

#ifndef _LP64
    reasonCodePtr = (int*) (0x80000000 | ((int)reasonCode));
#else
    reasonCodePtr = reasonCode;
#endif

  status = BPXCLO(&sd,
		   &returnValue,
		   returnCode,
		   reasonCodePtr);

  return returnValue;
}

static
int not_main(int argc, char **argv){
  char *addressString = argv[1];
  int port = argc >= 3 ? atoi(argv[2]) : 80;
  InetAddr *inetAddr = NULL;
  char readBuffer[1024];
  int errno;
  int retcode;

  inetAddr = getAddressByName(addressString);

  if (inetAddr){
    SocketAddress *socketAddress 
      = makeSocketAddr(inetAddr,(unsigned short)port);
    Socket *socket = tcpClient(socketAddress,&errno,&retcode);
    int i;
    int bytesRead;

    fflush(stdout);
    bytesRead = socketRead(socket,readBuffer,200,&errno,&retcode);
    dumpbuffer(readBuffer,bytesRead);
    for (i=0; i<5; i++){
      printf("SLEEP %d\n",i+1);fflush(stdout);
      sleep(1);
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

