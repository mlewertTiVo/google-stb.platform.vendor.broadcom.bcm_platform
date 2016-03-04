#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "nexus_base_mmap.h"
#include "nexus_platform.h"
#include "cutils/properties.h"
#include "nx_ashmem.h"
#include "nxclient.h"


#define TEST_SIZE (1 * 1024 * 1024)
static char src_buffer[TEST_SIZE];
static char dest_buffer[TEST_SIZE];

static NEXUS_MemoryBlockHandle block_handle = NULL;
static int nexus_fd;

void get_nexus_buffers( int size, char**srcp, char**destp )
{
    char value[PROPERTY_VALUE_MAX];
    char value2[PROPERTY_VALUE_MAX];
    void *pMemory = NULL;
    struct nx_ashmem_alloc ashmem_alloc;
    NxClient_JoinSettings joinSettings;

    NxClient_GetDefaultJoinSettings(&joinSettings);

    joinSettings.ignoreStandbyRequest = true;
    if (NxClient_Join(&joinSettings) != NEXUS_SUCCESS)
        return;

    property_get("ro.nexus.ashmem.devname", value, "nx_ashmem");
    strcpy(value2, "/dev/");
    strcat(value2, value);

    /* open */
    nexus_fd = open(value2, O_RDWR, 0);

    /* configure size */
    memset(&ashmem_alloc, 0, sizeof(struct nx_ashmem_alloc));
    ashmem_alloc.size = size * 2;
    ashmem_alloc.align = 4096;
    ioctl(nexus_fd, NX_ASHMEM_SET_SIZE, &ashmem_alloc);

    /* allocate */
    block_handle = (NEXUS_MemoryBlockHandle)ioctl(nexus_fd, NX_ASHMEM_GETMEM);
    printf ("block_handle = %p\n", block_handle);
    fflush (stdout);

    /* use */
    NEXUS_MemoryBlock_Lock(block_handle, &pMemory);

    *srcp = (char*)pMemory;
    *destp = (char*)pMemory + size;
}

void free_nexus_buffers( void )
{
    /* trash */
    NEXUS_MemoryBlock_Unlock(block_handle);
    close(nexus_fd);

    NxClient_Uninit();

}

int main (int argc, char *argv[])
{
    int i = 100;
    char *mode = "c";
    char *src = src_buffer;
    char *dest = dest_buffer;

    if (argc > 1)
        mode = argv[1];

    if (argc > 2)
        i = atoi(argv[2]);

    if ((mode[0] == 'C') || (mode[0] == 'D'))
        get_nexus_buffers(TEST_SIZE, &src, &dest);

    printf ("src = %p, dest = %p\n", src, dest);

    while (i--) {
        printf("\r%d", i);
        fflush(stdout);
        switch (mode[0]) {
        default:
        case 'c':
        case 'C':
            memcpy(dest, src, TEST_SIZE);
            break;
        case 'd':
        case 'D':
            {
                int sleep = 1000;

                if (argc > 3)
                    sleep = atoi(argv[3]);

                memcpy(dest, src, TEST_SIZE);
                usleep(sleep*1000);
            }
            break;
        case 'm':
            {
                int a,b,c,d,j;
                a = 1;
                b = 2;
                c = 4;
                d = (int)(&mode);
                for (j = 0; j < 10000; j++) {
                    a *= b;
                    b *= c;
                    c *= d;
                    d *= a;
                }
            }
        case 'p':
            {
                int j;
                for (j = 0; j < 10000; j++) {
                    printf("\r%d %d", i, j);
                    fflush(stdout);
                }
            }
            break;
        }
    }
    printf("\rdone\n");

    if ((mode[0] == 'C') || (mode[0] == 'D'))
        free_nexus_buffers();

    return 0;
}

