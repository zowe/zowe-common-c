/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
#include <stdio.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
#include "zds.hpp"
#include <dynit.h>

using namespace std;

/**
 * E.g. SYS1.MACLIB(ABEND)
 * Returns -1 for file not open
 */
int zdsRead(string name, string &data)
{
  name = "//'" + name + "'";

  ifstream in(name.c_str());
  if (!in.is_open())
  {
    // TODO(Kelosky): log & errors
    cout << "Could not open file: '" << name << "'" << endl;
    return -1;
  }

  string line;
  while (getline(in, line))
  {
    data += line;
    data.push_back('\n');
  }

  in.close();

  return 0;
}

int zdsWrite(string name, string &data)
{
  name = "//'" + name + "'";

  ofstream out(name.c_str());

  if (!out.is_open())
  {
    cout << "Could not open file: '" << name << "'.  Is it open elsewhere?" << endl;
    return -1;
  }

  out << data;

  out.close();

  return 0;
}

// https://www.ibm.com/docs/en/zos/3.1.0?topic=examples-listing-partitioned-data-set-members
#define RECLEN 254

typedef struct
{
  unsigned short int count;
  char rest[RECLEN];
} RECORD;

typedef struct
{
  char name[8];
  unsigned char ttr[3];
  unsigned char info;
} IND;

int zdsListMembers(string name, std::vector<ZDSMem> &list)
{
  // PO
  // PO-E (PDS)
  name = "//'" + name + "'";

  RECORD rec = {0};
  FILE *fp = fopen(name.c_str(), "rb");

  const int bufsize = 80;
  char buffer[bufsize] = {0};

  if (!fp)
  {
    cout << "Failed to open '" << name << "'" << endl;
    return -1;
  }

  while (fread(&rec, sizeof *buffer, sizeof(RECORD), fp))
  {
    unsigned char *data = NULL;
    data = (unsigned char *)&rec;
    data += sizeof(rec.count); // increment past halfword length
    int len = sizeof(IND);
    for (int i = 0; i < rec.count; i = i + len)
    {
      IND ind = {0};
      memcpy(&ind, data, sizeof(IND));
      long long int end = 0xFFFFFFFFFFFFFFFF;
      if (memcmp(ind.name, &name, sizeof(end)) == 0)
      {
        break;
      }
      else
      {
        unsigned char info = ind.info;
        char name[9] = {0};
        info &= 0x1F;

        memcpy(name, ind.name, sizeof(ind.name));

        for (int j = 8; j >= 0; j--)
        {
          if (name[j] == ' ')
          {
            name[j] = 0x00;
          }
        }

        ZDSMem mem = {0};
        mem.name = string(name);
        // mem->dsorg = "PO-E";
        list.push_back(mem);

        data = data + sizeof(IND) + (info * 2); // skip number of half workds
        len += (info * 2);
      }
    }
    // break;
    // dumpStorage("buffer", buffer, bufsize);
    /* byte swap here */
  }

  // cout << "ending" << endl;

  fclose(fp);

  return 0;
}

int zdsList(string name, std::vector<ZDS> &list)
{
  // PO
  // PO-E (PDS)
  ZDS zds = {0};
  zds.name = name;
  zds.dsorg = "PO-E";
  list.push_back(zds);

  return 0;
}

// Example: int rc = zdsReadDynalloc("MYDD", "SYS1.MACLIB", "YREGS", data);
int zdsReadDynalloc(string ddname, string dsname, string member, string &data)
{
  int rc = 0;
  string content;

  __dyn_t ip;
  rc = dyninit(&ip);
  if (0 != rc)
  {
    cerr << "Error: dyninit failed with " << rc << endl; // TODO(Kelosky): better error handling scheme
    return -1;
  }
  ip.__ddname = (char *)ddname.c_str();
  ip.__dsname = (char *)dsname.c_str();
  ip.__member = (char *)member.c_str();
  ip.__status = __DISP_SHR;

  rc = dynalloc(&ip);
  if (0 != rc)
  {
    cerr << "Error: dynalloc failed with " << rc << endl; // TODO(Kelosky): better error handling scheme
    return -1;
  }

    char buffer[80] = {0};

    FILE *fp = fopen(string("DD:" + ddname).c_str(), "r");
    // FILE *fp = fopen("DD:MYDD", "r");
    int len = 0;
    // char *data = "this is data from c";
    while ((len = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
      printf("read %s", buffer);
    }
    // fprintf(fp, data);
    fclose(fp);

  rc = dynfree(&ip);
  if (0 != rc)
  {
    cerr << "Error: dynfree failed with " << rc << endl; // TODO(Kelosky): better error handling scheme
    return -1;
  }

  return rc;
}