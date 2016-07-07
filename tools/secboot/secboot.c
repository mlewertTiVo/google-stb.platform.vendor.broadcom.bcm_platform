/******************************************************************************
* Broadcom Proprietary and Confidential. (c)2016 Broadcom. All rights reserved.
*
* This program is the proprietary software of Broadcom and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. , WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/


#include "bchp_bsp_cmdbuf.h"
#include "bchp_bsp_glb_control.h"
#include "bchp_common.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

unsigned int VIRTUAL_ADDRESS = 0xA0000000;

unsigned char send_otp_program_pattern(void)
{
   unsigned char status;

   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_IRDY) & 0x02 )  != 0x02);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x300) = 0x10;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x304) = 0x22;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x308) = 0xabcdef00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x30C) = 0xae55aa51;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x310) = 0x789a0024;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x314) = 0xbc32f4ac;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x318) = 0xd18616b6;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x31C) = 0x9feb4d54;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x320) = 0x4a27bf4a;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x324) = 0xcf1c3178;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x328) = 0xe2db98a0;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x32C) = 0x24f64bba;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x330) = 0x7698e712;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x334) = 0x0000f48d;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_ILOAD2) = 0x01;
   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) & 0x01 )  != 0x01);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) = 0x00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_HOST_INTR_STATUS) = 0x00;
   status = *(volatile unsigned char*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x494);
   return status;
}

unsigned char read_msp(unsigned char msp_field, unsigned long *msp_data)
{
   unsigned char status;

   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_IRDY) & 0x02 )  != 0x02);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x300) = 0x10;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x304) = 0x22;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x308) = 0xabcdef00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x30C) = 0xe455aa1b;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x310) = 0x789a0004;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x314) = msp_field;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_ILOAD2) = 0x01;
   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) & 0x01 )  != 0x01);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) = 0x00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_GLB_CONTROL_GLB_HOST_INTR_STATUS) = 0x00;
   status = *(volatile unsigned char*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x494);
   if ( status != 0x00 )
      return status;
   *msp_data = *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x498);
   return status;
}

unsigned char write_msp(
   unsigned char msp_field,
   unsigned long bit_length,
   unsigned long mask,
   unsigned long msp_data)
{
   unsigned char status;

   status = send_otp_program_pattern();
   if(status) {
      status = send_otp_program_pattern();
      if(status) return status;
   }
   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_GLB_CONTROL_GLB_IRDY) & 0x02 )  != 0x02);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x300) = 0x10;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x304) = 0x22;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x308) = 0xabcdef00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x30C) = 0xe555aa1a;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x310) = 0x789a0014;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x314) = 0x12;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x318) = msp_field;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x31C) = bit_length;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x320) = mask;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x324) = msp_data;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_GLB_CONTROL_GLB_ILOAD2) = 0x01;
   while (  ( *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) & 0x01 )  != 0x01);
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_GLB_CONTROL_GLB_OLOAD2) = 0x00;
   *(volatile unsigned long*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_GLB_CONTROL_GLB_HOST_INTR_STATUS) = 0x00;
   return(*(volatile unsigned char*) (VIRTUAL_ADDRESS+BCHP_PHYSICAL_OFFSET+BCHP_BSP_CMDBUF_DMEMi_ARRAY_BASE + 0x494));
}

void CmdReadBootEn(void)
{
   unsigned long ulData;
   unsigned char ucStatus;

   ucStatus = read_msp(0x4A, &ulData);
   if(ucStatus)
      printf("read failed status = %x\n", ucStatus );
   else {
      if(ulData)
         printf("SecureBootOtp is enabled\n");
      else
         printf("SecureBootOtp is disabled\n");
   }
   return;
}

void CmdProgramBootEn(void)
{
   unsigned char ucStatus;
   ucStatus = write_msp(0x4A, 1, 1, 1);
   if(ucStatus)
      printf("progamming failure = %x\n", ucStatus);
   else
      printf("SecureBootOtp bit is programmed\n");
   return;
}


int main(int argc, char **argv)
{
	int fd;
   uint32_t value;
   int enable = 0;
   int arg;

   while ((arg = getopt(argc, argv, "e")) != -1) {
   switch (arg) {
   case 'e':
      enable = 1;
      break;
	default:
      return -1;
	}
	}

   fd = open ("/dev/mem", O_RDWR|O_SYNC);
   if (fd < 0) {
      fprintf(stderr, "cannot open \'/dev/mem\'.  aborting.\n");
      return -1;
   }

#define REGISTER_BASE   (BCHP_PHYSICAL_OFFSET + (BCHP_REGISTER_START & ~0xFFF))
#define REGISTER_SIZE  (BCHP_REGISTER_END - (BCHP_REGISTER_START & ~0xFFF))
   VIRTUAL_ADDRESS = (unsigned int) mmap64(0, REGISTER_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, REGISTER_BASE);
   if (VIRTUAL_ADDRESS == 0xFFFFFFFF) {
      fprintf(stderr, "cannot mmap.  aborting (%d).\n", errno);
      return -1;
   }
   printf("register %x (%x) mapped @ %x\n", REGISTER_BASE, REGISTER_SIZE, VIRTUAL_ADDRESS);

   CmdReadBootEn();

   if (enable) {
      CmdProgramBootEn();
      CmdReadBootEn();
   }

   munmap((void *)VIRTUAL_ADDRESS, REGISTER_SIZE);
   close(fd);
   return 0;
}
