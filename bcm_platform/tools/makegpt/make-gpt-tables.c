/*****************************************************************************
* Copyright 2007 - 2015 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include "efi.h"

typedef struct
{
    char name[35];      /* name of image (not unicode) */
    uint64_t start;     /* starting address of image in bytes */
    uint64_t size;      /* ending address of image in bytes */
    uint64_t attributes;    /* image attributes (flags) */
}
info_t;;


/* Verbosity levels */
#define V_QUIET  0
#define V_NORMAL 1
#define V_DEBUG  2
#define V_NOISY  3

#define PRINT(fmt, args... )    fprintf(stderr, fmt, ##args)
#define PNORMAL(fmt, args... )  if (verbose > V_QUIET)  { fprintf(stderr, fmt, ##args); }
#define PDEBUG(fmt, args... )   if (verbose > V_NORMAL) { fprintf(stderr, fmt, ##args); }
#define PNOISY(fmt, args... )   if (verbose > V_DEBUG)  { fprintf(stderr, fmt, ##args); }

/*****************************************************************************
*
*   Usage
*/
static void Usage(void)
{
    PRINT( "Usage: makegpt [options] partitionName,startaddr,size,attr [...]...\n");
    PRINT( "where options may be one of:\n");
    PRINT( "  -b base_address     base address of partition table (mandatory)\n");
    PRINT( "  -s total_disk_size  total disk size in bytes (mandatory)\n");
    PRINT( "  -a                  generate alternate GPT table instead\n");
    PRINT( "  -o  outputfile      output file (mandatory)\n");
    PRINT( "  -v  level           turns on verbose mode. 0=off, 1=normal, 2=debug, 3=noisy\n");
    PRINT( "  -h                  prints this help\n");
    PRINT( "\n");
    PRINT( "Example: makegpt -a -b 0x60000 -o gpt_alt.bin -s 0x1e0000000 "
             "image1,0x10000,0x10000,0 "
             "image2,0x20000,0x10000,0 "
             "image3,0x30000,0x30000,0 "
             "image4,0x90000,0x50000,0\n");
}

/*****************************************************************************
*
*   main
*/
static char zero_pad[512];
static efi_gpt_entry_t entries[64];
static efi_partition_info_t partinfo[64];
int main(int argc, char **argv)
{
    int arg;
    int verbose = V_QUIET;
    char fspec[256] = "";
    efi_legacy_mbr_t pmbr;
    efi_gpt_header_t gpt;
    efi_gpt_header_t agpt;
    int imgcount = 0;
    FILE *ofp = NULL;
    uint64_t total_disk_size = 0;
    int isAlternate = 0;
    uint64_t base_address = ~0;

    memset(&pmbr, 0, sizeof(pmbr));
    memset(&gpt, 0, sizeof(gpt));
    memset(&agpt, 0, sizeof(agpt));
    memset(&zero_pad, 0, sizeof(zero_pad));

    while ((arg = getopt(argc, argv, "b:s:ao:hv:")) != -1)
    {
        switch (arg)
        {
            case 'a':
                isAlternate = 1;;
                break;

            case 'o':
                strncpy(fspec, optarg, sizeof(fspec) - 1);
                break;

            case 'v':
                verbose = (int) strtoul(optarg, NULL, 0);
                break;

            case 's':
                total_disk_size = (uint64_t) strtoull(optarg, NULL, 0);
                break;

            case 'b':
                base_address = (uint64_t) strtoull(optarg, NULL, 0);
                break;

            case 'h':
            default:
            {
                Usage();
                exit(1);
            }
        }
    }
    if (strlen(fspec) == 0)
    {
        fprintf(stderr, "Error - must specify output file with -o option\n");
        Usage();
        exit(1);
    }
    if (total_disk_size == 0)
    {
        fprintf(stderr, "Error - must specify total disk size with -s option\n");
        Usage();
        exit(1);
    }
    if (base_address == (uint64_t) ~0)
    {
        fprintf(stderr, "Error - must specify base address of partition table with -b option\n");
        Usage();
        exit(1);
    }
    PDEBUG("Output file %s\n", fspec);
    PDEBUG("total_disk_size 0x%llx\n", (unsigned long long) total_disk_size);
    PDEBUG("base_address 0x%llx\n", (unsigned long long) base_address);
    while (optind < argc)
    {
        /* Parse partitionName,startaddr,size,attr...*/
        char *partitionNameStr = NULL;
        char *startStr = NULL;
        char *sizeStr = NULL;
        char *attrStr = NULL;
        uint64_t start;
        uint64_t size;
        uint64_t attr;

        partitionNameStr = argv[optind++];
        if ((startStr = strchr(partitionNameStr, ',')) != NULL)
        {
            *startStr++ = '\0';
            start = strtoull(startStr, NULL, 0);
        }
        else
        {
            PRINT( "Error: Bad line format  partitionName,startaddr,size,attr ...\n");
            exit(1);
        }
        if ((sizeStr = strchr(startStr, ',')) != NULL)
        {
            *sizeStr++ = '\0';
            size = strtoull(sizeStr, NULL, 0);
        }
        else
        {
            PRINT( "Error: Bad line format  partitionName,startaddr,size,attr ...\n");
            exit(1);
        }
        if ((attrStr = strchr(sizeStr, ',')) != NULL)
        {
            ++attrStr;
            attr = strtoull(attrStr, NULL, 0);
        }
        else
        {
            PRINT( "Error: Bad line format  partitionName,startaddr,size,attr ...\n");
            exit(1);
        }
        strncpy(partinfo[imgcount].name, partitionNameStr, sizeof(partinfo[imgcount].name));
        partinfo[imgcount].start = start;
        if (size != 0)
        {
            partinfo[imgcount].end = start + size - EFI_SECTORSIZE;
        }
        else
        {
            partinfo[imgcount].end = start;
        }
        partinfo[imgcount].attributes = attr;

        PNORMAL( "partition \'%s\' start=0x%llx, end=0x%llx, attr=0x%llx size=0x%llx\n",
                  partinfo[imgcount].name,
                  (unsigned long long) partinfo[imgcount].start,
                  (unsigned long long) partinfo[imgcount].end,
                  (unsigned long long) partinfo[imgcount].attributes,
                  (unsigned long long) size);

        imgcount++;
    }

    ofp = fopen(fspec, "w+");
    if (!ofp)
    {
        PRINT( "Error: Cannot open \'%s\'  %s\n", fspec, strerror(errno));
        exit(1);
    }

    efi_populate_tables(&gpt, &agpt, imgcount, &pmbr, entries, partinfo, total_disk_size, EFI_SECTORADDR(base_address)+1);

    /* Validate */
    if (isAlternate == 0)
    {
        /* Write main PMBR, GPT HDR, and Partition Entries */
        fwrite(&pmbr, sizeof(pmbr), 1, ofp);
        fwrite(&gpt, sizeof(gpt), 1, ofp);
        fwrite(entries, imgcount * sizeof(efi_gpt_entry_t), 1, ofp);
        PNORMAL("Wrote primary GPT table \'%s\'\n", fspec);
    }
    else
    {
        /* Write alternate GPT HDR, and Partition Entries */
        fwrite(entries, imgcount * sizeof(efi_gpt_entry_t), 1, ofp);

        /* pad so that alt GPT is on 512 byte boundary */
        fwrite(zero_pad, EFI_PART2SECT(imgcount) - imgcount * sizeof(efi_gpt_entry_t), 1, ofp);
        fwrite(&agpt, sizeof(gpt), 1, ofp);
        PNORMAL("Wrote alternate GPT table \'%s\'\n", fspec);
    }

    if (verbose > V_NORMAL)
    {
        efi_dump_tables(&gpt, &agpt, entries);
    }

    fclose(ofp);
    return 0;
}