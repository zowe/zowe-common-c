
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_RUSERMAP__
#define __ZOWE_RUSERMAP__ 1

#define RUSERMAP_SAF_SUCCESS 0
#define RUSERMAP_RACF_FAILURE 8
#define RUSERMAP_RACF_REASON_NOAUTH 0x14
#define RUSERMAP_RACF_REASON_NOUSERID 0x18
#define RUSERMAP_RACF_REASON_BADCERT 0x1C
#define RUSERMAP_RACF_REASON_NOUSER_FOR_CERT 0x20

/** 
  getUseridByCertificate() gets the SAF Userid for a certificate, if one had been previously
  defined for that user.

  The certificate is a standard DER format certificate and must have the proper length passed
  in the second argument.  The useridBuffer must be at least 9 bytes or bad thing will happen.

  The saf return code is returned and must be 0 for success.  Non-Zero returns are further explained
  in the racfRC and racfReason values

  Please consult the following IBM doc for a full explanation of return codes:
 
  https://www.ibm.com/docs/en/zos/2.5.0?topic=descriptions-r-usermap-irrsim00-map-application-user
 */

int getUseridByCertificate(char *certificate, int certificateLength, char *useridBuffer,
                           int *racfRC, int *racfReason);


#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
