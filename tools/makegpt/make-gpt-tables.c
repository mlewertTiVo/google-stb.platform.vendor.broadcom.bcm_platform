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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include "efi.h"


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
    PRINT( "  startaddr can be '-' to guess based on previous startaddr+size.\n");
    PRINT( "  size can be '-' to guess based on next startaddr.\n");
    PRINT( "  startaddr and size can be a hex value or a value with suffix K, M, or G.\n");
    PRINT( "\n");
    PRINT( "Example: makegpt -a -b 0x60000 -o gpt_alt.bin -s 0x1e0000000 -- "
             "image1,0x10000,0x10000,0 "
             "image2,-,64k,0 "
             "image3,0x30000,0x30000,0 "
             "image4,0x90000,-,0\n");
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
    int verbose = V_NORMAL;
    char fspec[256] = "";
    efi_legacy_mbr_t pmbr;
    efi_gpt_header_t gpt;
    efi_gpt_header_t agpt;
    int imgcount = 0;
    FILE *ofp = NULL;
    uint64_t total_disk_size = 0;
    int isAlternate = 0;
    uint64_t base_address = ~0;
    int i;
    int part_guess_start[64];
    int part_guess_size[64];
    uint64_t agpt_reserved_size;
    uint64_t size;

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
    PDEBUG("total_disk_size 0x%"PRIx64"\n", total_disk_size);
    PDEBUG("base_address 0x%"PRIx64"\n", base_address);
    while (optind < argc)
    {
        /* Parse partitionName,startaddr,size,attr...*/
        char *partitionNameStr = NULL;
        char *startStr = NULL;
        char *sizeStr = NULL;
        char *attrStr = NULL;
        char *endptr = NULL;
        uint64_t start;
        uint64_t attr;

        partitionNameStr = argv[optind++];
        if ((startStr = strchr(partitionNameStr, ',')) != NULL)
        {
            *startStr++ = '\0';
            start = strtoull(startStr, &endptr, 0);
            if (start==0 && index(startStr, '-'))
            {
                part_guess_start[imgcount]=1;
            }
            else
            {
                part_guess_start[imgcount]=0;
                /* Handle suffixes */
                switch (*endptr)
                {
                case 'k':
                case 'K':
                    start *= 1024;
                    break;
                case 'M':
                    start *= 1024*1024;
                    break;
                case 'G':
                    start *= 1024*1024*1024;
                    break;
                }
            }
        }
        else
        {
            PRINT( "Error: Bad line format  partitionName,startaddr,size,attr ...\n");
            PRINT( "Unable to find start of partition %d (%s)\n", imgcount, partitionNameStr);
            exit(1);
        }
        if ((sizeStr = strchr(startStr, ',')) != NULL)
        {
            *sizeStr++ = '\0';
            size = strtoull(sizeStr, &endptr, 0);
            if (size==0 && index(sizeStr, '-'))
            {
                part_guess_size[imgcount]=1;
            }
            else
            {
                part_guess_size[imgcount]=0;
                /* Handle suffixes */
                switch (*endptr)
                {
                case 'k':
                case 'K':
                    size *= 1024;
                    break;
                case 'M':
                    size *= 1024*1024;
                    break;
                case 'G':
                    size *= 1024*1024*1024;
                    break;
                }
            }
        }
        else
        {
            PRINT( "Error: Bad line format  partitionName,startaddr,size,attr ...\n");
            PRINT( "Unable to find size of partition %d (%s)\n", imgcount, partitionNameStr);
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
            PRINT( "Unable to find attributes of partition %d (%s)\n", imgcount, partitionNameStr);
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
        imgcount++;
    }

    /* Reserve space for the alternate gpt (used when guessing the size of
       the last partition).

       The minimum size required is 1 sector for the header + 1 sector per
       4 partition table entries. However, we'll reserve more space so that
       the gpt binary can grow without impacting the last partition.

       Similar to sgdisk, we'll reserve 1 sector for the header + 32 sectors
       for 128 partition table entries.

       At this time, efi_populate_header doesn't take advantage of all that
       space, but when a user modifies the GPT at runtime, the tool can
       regenerate the GPT at the correct offset within our reserved space.
    */
    if (imgcount <= 128)
        agpt_reserved_size = EFI_SECTORSIZE*(1+EFI_SECTORADDR(EFI_PART2SECT(128)));
    else
        agpt_reserved_size = EFI_SECTORSIZE*(1+EFI_SECTORADDR(EFI_PART2SECT(imgcount)));
    PDEBUG("agpt_reserved_size 0x%"PRIx64"\n", agpt_reserved_size);

    /* Finalize partition start and sizes */
    size = 0;
    for (i=0; i<imgcount; i++)
    {
        if (part_guess_start[i])
        {
            if (i == 0)
            {
                partinfo[i].start = 0;
            }
            else if (part_guess_size[i-1])
            {
                PRINT( "Error: unable to guess consecutive size and start...\n");
                exit(1);
            }
            else
            {
                /* Reuse "size" value from i-1 */
                partinfo[i].start = partinfo[i-1].start + size;
                /* "end" was based on 0, rebase on the new "start" */
                partinfo[i].end += partinfo[i].start;
            }
        }

        /* Check if the user requested to fill (size='-') */
        if (part_guess_size[i])
        {
            /* Find how much we can fill */
            int j;

            if ((partinfo[i].start + EFI_SECTORSIZE) >= (total_disk_size - agpt_reserved_size))
            {
                PRINT("Error: no room for partition %d (%s). start=0x%"PRIx64" limit=0x%"PRIx64"\n",
                      i, partinfo[i].name, partinfo[i].start,
                      total_disk_size - agpt_reserved_size);
                exit(1);
            }

            size = total_disk_size - agpt_reserved_size - partinfo[i].start;
            for (j=0; j<imgcount; j++)
            {
                if (partinfo[j].start > partinfo[i].start &&
                    (partinfo[j].start - partinfo[i].start) < size)
                {
                    size = partinfo[j].start - partinfo[i].start;
                }
            }
            partinfo[i].end = partinfo[i].start + size - EFI_SECTORSIZE;
        }
        else
        {
            size = partinfo[i].end - partinfo[i].start + EFI_SECTORSIZE;
        }

        PNORMAL( "partition \'%s\' start=0x%"PRIx64", end=0x%"PRIx64", attr=0x%"PRIx64" size=0x%"PRIx64"\n",
                  partinfo[i].name,
                  partinfo[i].start,
                  partinfo[i].end,
                  partinfo[i].attributes,
                  size);
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
