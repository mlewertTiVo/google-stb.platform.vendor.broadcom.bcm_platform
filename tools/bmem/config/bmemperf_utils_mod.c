/******************************************************************************
 * (c) 2014 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or its
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
 *    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
 *    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
 *    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include "bstd.h"
#include "bmemperf_server.h"
#include "bmemperf_cgi.h"
#include "bmemperf_utils.h"
#include "bmemperf_info.h"
#include "nexus_platform.h"

#define FILENAME_TAG "filename=\""

#ifdef USE_BOXMODES
extern bmemperf_boxmode_info g_boxmodes[20];
#endif /* USE_BOXMODES */
extern int   g_MegaBytes;
extern int   g_MegaBytesDivisor[2];
extern char *g_MegaBytesStr[2];

#define PROC_INTERRUPTS_FILE         "/proc/interrupts"
#define TEMP_INTERRUPTS_FILE         "interrupts"
#define PROC_STAT_FILE               "/proc/stat"
#define UTILS_LOG_FILE_FULL_PATH_LEN 64

extern char         *g_client_name[BMEMPERF_MAX_NUM_CLIENT];
extern bmemperf_info g_bmemperf_info;

int gethostbyaddr2(
    const char *HostName,
    int         port,
    char       *HostAddr,
    int         HostAddrLen
    )
{
    char            portStr[8];
    struct addrinfo hints, *res, *p;
    int             status = 0;

    if (HostAddr == NULL)
    {
        fprintf( stderr, "ERROR - HostAddr is null\n" );
        return( 2 );
    }

    memset( &portStr, 0, sizeof portStr );
    memset( &hints, 0, sizeof hints );
    hints.ai_family   = AF_UNSPEC; /* AF_INET or AF_INET6 to force version */
    hints.ai_socktype = SOCK_STREAM;

    sprintf( portStr, "%u", port );
    if (( status = getaddrinfo( HostName, portStr, &hints, &res )) != 0)
    {
        fprintf( stderr, "ERROR - getaddrinfo: %s\n", gai_strerror( status ));
        return( 2 );
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        void *addr;

        PRINTF( "family is %u; AF_INET is %u\n", p->ai_family, AF_INET );
        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &( ipv4->sin_addr );

            /* convert the IP to a string and print it: */
            inet_ntop( p->ai_family, addr, HostAddr, HostAddrLen );

            PRINTF( "Hostname: %s; IP Address: %s\n", HostName, HostAddr );
            break;
        }
        else
        {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &( ipv6->sin6_addr );
        }
    }

    return( 0 );
} /* gethostbyaddr2 */

const char *noprintf(
    const char *format,
    ...
    )
{
    return( format );
}

const char *nofprintf(
    FILE       *stdsomething,
    const char *format,
    ...
    )
{
    if (stdsomething == NULL)
    {
    }
    return( format );
}

/**
 *  Function: This function will return a string that contains the current date and time.
 **/
char *DateYyyyMmDdHhMmSs(
    void
    )
{
    static char    fmt [64];
    struct timeval tv;
    struct tm     *tm;

    memset( fmt, 0, sizeof( fmt ));
    gettimeofday( &tv, NULL );
    if (( tm = localtime( &tv.tv_sec )) != NULL)
    {
        strftime( fmt, sizeof fmt, "%Y%m%d-%H%M%S", tm );
    }

    /*printf("%s: returning (%s)\n", __FUNCTION__, fmt);*/
    return( fmt );
}

char *HhMmSs(
    unsigned long int timestamp
    )
{
    static char    fmt[64];
    struct timeval tv;
    struct tm     *tm;

    memset( fmt, 0, sizeof( fmt ));

    if (timestamp == 0)
    {
        gettimeofday( &tv, NULL );
    }
    else
    {
        tv.tv_sec = timestamp;
    }

    if (( tm = localtime( &tv.tv_sec )) != NULL)
    {
        strftime( fmt, sizeof fmt, "%H:%M:%S", tm );
    }

    return( fmt );
} /* HhMmSs */

char *DayMonDateYear(
    unsigned long int timestamp
    )
{
    static char    fmt[64];
    struct timeval tv;
    struct tm     *tm;

    memset( fmt, 0, sizeof( fmt ));

    if (timestamp == 0)
    {
        gettimeofday( &tv, NULL );
    }
    else
    {
        tv.tv_sec = timestamp;
    }

    if (( tm = localtime( &tv.tv_sec )) != NULL)
    {
        strftime( fmt, sizeof fmt, "%a %b %d, %Y %H:%M:%S", tm );
    }

    return( fmt );
} /* DayMonDateYear */

unsigned long int getSecondsSinceEpoch(
    void
    )
{
    struct timeval tv;

    gettimeofday( &tv, NULL );

    return( tv.tv_sec );
}

char *getPlatformVersion(
    void
    )
{
    char              major = (char)( BCHP_VER>>16 ) + 'A';
    unsigned long int minor = ( BCHP_VER>>12 )&0xF;
    static char       version[8];

    memset( version, 0, sizeof( version ));
    /* D0 is 0x30000 */
    sprintf( version, "%c%lx", major, minor );
    return( version );
}

char *getPlatform(
    void
    )
{
    static char platform[8];

    memset( platform, 0, sizeof( platform ));
    sprintf( platform, "%u", NEXUS_PLATFORM );
    return( platform );
} /* getPlatform */

int Close(
    int socketFd
    )
{
    PRINTF( "%s: socket %d\n", __FUNCTION__, socketFd );
    if (socketFd>0) {close( socketFd ); }
    return( 0 );
}

int send_request_read_response(
    struct sockaddr_in *server,
    bmemperf_request   *request,
    bmemperf_response  *response,
    int                 cmd
    )
{
    int                rc = 0;
    int                sd = 0;
    struct sockaddr_in from;
    int                fromlen;

    /*   Create socket on which to send and receive */
    sd = socket( AF_INET, SOCK_STREAM, 0 );

    if (sd<0)
    {
        printf( "%s: ERROR creating socket\n", __FUNCTION__ );
        return( -1 );
    }

    /* Connect to server */
    PRINTF( "%s: connect(sock %u; port %u)\n", __FUNCTION__, sd, BMEMPERF_SERVER_PORT );
    if (connect( sd, (struct sockaddr *) server, sizeof( *server )) < 0)
    {
        Close( sd );
        printf( "%s: ERROR connecting to server\n", __FUNCTION__ );
        return( -1 );
    }

    fromlen = sizeof( from );
    if (getpeername( sd, (struct sockaddr *)&from, (unsigned int *) &fromlen )<0)
    {
        Close( sd );
        printf( "%s: ERROR could not get peername\n", __FUNCTION__ );
        return( -1 );
    }

    PRINTF( "Connected to TCPServer1: " );
    PRINTF( "%s:%d\n", inet_ntoa( from.sin_addr ), ntohs( from.sin_port ));
    #if 0
    struct hostent *hp = NULL;
    if (( hp = gethostbyaddr((char *) &from.sin_addr.s_addr, sizeof( from.sin_addr.s_addr ), AF_INET )) == NULL)
    {
        fprintf( stderr, "Can't find host %s\n", inet_ntoa( from.sin_addr ));
    }
    else
    {
        PRINTF( "(Name is : %s)\n", hp->h_name );
    }
    #endif /* if 0 */

    request->cmd = cmd;

    PRINTF( "%s: client_list %u; %u; %u\n", __FUNCTION__, request->request.client_stats_data.client_list[0][0],
        request->request.client_stats_data.client_list[1][0], request->request.client_stats_data.client_list[2][0] );
    PRINTF( "Sending request cmd (%d) to server; sizeof(request) %u\n", request->cmd, sizeof( *request ));
    if (send( sd, request, sizeof( *request ), 0 ) < 0)
    {
        printf( "%s: failed to send cmd %u to socket %u\n", __FUNCTION__, request->cmd, sd );
        return( -1 );
    }

    PRINTF( "Reading response from server; sizeof(response) %u\n", sizeof( *response ));
    if (( rc = recv( sd, response, sizeof( *response ), 0 )) < 0)
    {
        printf( "%s: failed to recv cmd %u from socket %u\n", __FUNCTION__, request->cmd, sd );
        return( -1 );
    }

    Close( sd );
    return( 0 );
} /* send_request_read_response */

char *getClientName(
    int client_index
    )
{
    return( &g_client_name[client_index][0] );
} /* getClientName */

pid_t daemonize(
    void
    )
{
    pid_t pid = 0, sid = 0;
    int   fd  = 0;

    /* already a daemon */
    if (getppid() == 1)
    {
        printf( "%s: already a daemon\n", __FUNCTION__ );
        return( 0 );
    }

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
    {
        printf( "%s: fork() command failed\n", __FUNCTION__ );
        exit( EXIT_FAILURE );
    }

    if (pid > 0)
    {
        /*printf( "%u: This is the parent; exiting immediately\n", pid );*/
    }
    else
    {
        char utilsLogFilename[UTILS_LOG_FILE_FULL_PATH_LEN];

        /*printf( "%u: This is the child; starting daemonize process\n", pid );*/

        /* At this point we are executing as the child process */
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0)
        {
            exit( EXIT_FAILURE );
        }

        printf( "%s: new sid is %d\n", __FUNCTION__, sid );
        /* Change the current working directory. */
        if (( chdir( GetTempDirectoryStr())) < 0)
        {
            exit( EXIT_FAILURE );
        }

        fd = open( "/dev/null", O_RDWR, 0 );

        if (fd != -1)
        {
            /*printf( "%u: Assign STDIN to /dev/null\n", pid );*/
            dup2( fd, STDIN_FILENO );

            if (fd > 2)
            {
                close( fd );
            }
        }

        PrependTempDirectory( utilsLogFilename, sizeof( utilsLogFilename ), "bmemperf_utils.log" );
        fd = open( utilsLogFilename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR  );

        if (fd != -1)
        {
            /*printf( "%u: Assign STDOUT/STDERR to fd (%u)\n", pid, fd );*/
            dup2( fd, STDOUT_FILENO );
            dup2( fd, STDERR_FILENO );
        }

        /*reset file creation mask */
        umask( 0177 );
    }

    return( pid );
} /* daemonize */

char *decodeFilename(
    const char *filename
    )
{
    char        *pos         = NULL;
    char        *prev        = NULL;
    char        *decodedName = NULL;
    unsigned int pass        = 0;

    if (filename == NULL)
    {
        return( NULL );
    }

    PRINTF( "%s: filename (%s)\n", __FUNCTION__, filename );
    decodedName = malloc( strlen( filename ));
    if (decodedName == NULL)
    {
        PRINTF( "%s: could not malloc %u bytes for decodedName\n", __FUNCTION__, strlen( filename ));
        return( NULL );
    }
    PRINTF( "%s: malloc %u bytes for decodedName\n", __FUNCTION__, strlen( filename ));
    memset( decodedName, 0, strlen( filename ));

    pos = prev = (char *) filename;

    while (( pos = strstr( pos, "%2F" )))
    {
        strncat( decodedName, prev, pos - prev );
        strncat( decodedName, "/", 1 );
        PRINTF( "%s: pass %u: newname (%s)\n", __FUNCTION__, pass, decodedName );
        pos += 3;
        prev = pos;
        pass++;
    }
    strcat( decodedName, prev );
    PRINTF( "%s: pass %u: newname (%s)\n", __FUNCTION__, pass, decodedName );

    return( decodedName );
} /* decodeFilename */

int sendFileToBrowser(
    const char *filename
    )
{
    char        *contents = NULL;
    FILE        *fpInput  = NULL;
    struct stat  statbuf;
    unsigned int newlen           = 0;
    char        *pFileNameDecoded = NULL;
    char        *pos              = NULL;
    char         tempFilename[TEMP_FILE_FULL_PATH_LEN];

    PRINTF( "~%s: filename (%p)\n~", __FUNCTION__, filename ); fflush( stdout ); fflush( stderr );
    PRINTF( "~%s: filename (%s)\n~", __FUNCTION__, filename ); fflush( stdout ); fflush( stderr );
    pFileNameDecoded = decodeFilename( filename );

    PRINTF( "~%s: pFileNameDecoded (%s)\n~", __FUNCTION__, filename ); fflush( stdout ); fflush( stderr );
    if (pFileNameDecoded == NULL)
    {
        return( -1 );
    }

    PrependTempDirectory( tempFilename, sizeof( tempFilename ), pFileNameDecoded );
    PRINTF( "~Prepended (%s)\n~", tempFilename ); fflush( stdout ); fflush( stderr );

    if (lstat( tempFilename, &statbuf ) == -1)
    {
        printf( "Could not stat (%s)\n", tempFilename ); fflush( stdout ); fflush( stderr );
        return( -1 );
    }
    /* remove the tempdir part from beginning of the file */
    pos = strrchr( tempFilename, '/' );
    PRINTF( "~pos 1 (%s)(%p)\n~", pos, pos ); fflush( stdout ); fflush( stderr );
    PRINTF( "~tempFilename (%s)\n~", tempFilename ); fflush( stdout ); fflush( stderr );
    PRINTF( "~statbuf.st_size %lld\n~", statbuf.st_size ); fflush( stdout ); fflush( stderr );
    if (pos)
    {
        pos++; /* advance pointer past the slash character */
        PRINTF( "~pos 2 (%s)(%p); len %d\n~", pos, pos, strlen( pos )); fflush( stdout ); fflush( stderr );
    }
    else
    {
        pos = (char *) tempFilename;
        PRINTF( "~pos 3 (%s)(%p); len %d\n~", pos, pos, strlen( pos )); fflush( stdout ); fflush( stderr );
    }
    newlen = strlen( pos ) + 1 /* null string terminator */;

    printf( "Content-Disposition: form-data; filename=\"%s\"\n", pos ); fflush( stdout ); fflush( stderr );

    if (statbuf.st_size)
    {
        contents = malloc( statbuf.st_size );
        PRINTF( "%s: malloc(%llu); contents (%p)\n", __FUNCTION__, statbuf.st_size, contents ); fflush( stdout ); fflush( stderr );
        if (contents == NULL) {return( -1 ); }

        fpInput = fopen( tempFilename, "r" );
        PRINTF( "%s: fread(%llu)\n", __FUNCTION__, statbuf.st_size ); fflush( stdout ); fflush( stderr );
        fread( contents, 1, statbuf.st_size, fpInput );
        fclose( fpInput );

        printf( "Content-Type: application/octet-stream\n" ); fflush( stdout ); fflush( stderr );
        printf( "Content-Length: %lu\n", (unsigned long int) statbuf.st_size ); fflush( stdout ); fflush( stderr );
        printf( "\n" );

        fwrite( contents, 1, statbuf.st_size, stdout );
    }

    if (pFileNameDecoded) {free( pFileNameDecoded ); }
    if (contents) {free( contents ); }

    return( 0 );
} /* sendFileToBrowser */

int scanForInt(
    const char *queryRequest,
    const char *formatStr,
    int        *returnValue
    )
{
    char  newstr[32];
    char *pos = strstr( queryRequest, formatStr );

    /*printf("looking for (%s) in (%s); pos is %p<br>\n", formatStr, queryRequest, pos );*/
    if (pos)
    {
        strncpy( newstr, formatStr, sizeof( newstr ));
        strncat( newstr, "%d", sizeof( newstr ));
        sscanf( pos, newstr, returnValue );
        PRINTF( "%s is %d\n", formatStr, *returnValue );
    }
    return( 0 );
}

int scanForStr(
    const char  *queryRequest,
    const char  *formatStr,
    unsigned int returnValuelen,
    char        *returnValue
    )
{
    char *pos = strstr( queryRequest, formatStr );

    /*printf( "looking for (%s) in (%s); pos is (%s)<br>\n", formatStr, queryRequest, pos );*/
    if (pos)
    {
        char *delimiter = NULL;

        delimiter = strstr( pos+strlen( formatStr ), "&" );
        if (delimiter)
        {
            unsigned int len = delimiter - pos - strlen( formatStr );
            PRINTF( "delimiter (%s)\n", delimiter );
            strncpy( returnValue, pos+strlen( formatStr ), len );
            returnValue[len] = '\0';
        }
        else
        {
            strncpy( returnValue, pos+strlen( formatStr ), returnValuelen );
        }
        PRINTF( "%s is (%s)\n", formatStr, returnValue );
    }
    return( 0 );
} /* scanForStr */

int readFileFromBrowser(
    const char       *contentType,
    const char       *contentLength,
    char             *sFilename,
    unsigned long int lFilenameLen
    )
{
    int   ContentLengthInt;
    char *boundaryString = NULL;
    char *cgi_query      = NULL;
    FILE *fOut           = NULL;

    /* How much data do we expect? */

    PRINTF( "Content-type: text/html\n\n" );
    PRINTF( "len (%s); type (%s)<br>\n", contentLength, contentType );
    if (( contentLength == NULL ) ||
        ( sscanf( contentLength, "%d", &ContentLengthInt ) != 1 ))
    {
        PRINTF( "could not sscanf CONTENT_LENGTH<br>\n" );
        return( -1 );
    }
    /* Create space for it: */

    cgi_query = malloc( ContentLengthInt + 1 );
    PRINTF( "malloc'ed space for %d bytes<br>\n", ContentLengthInt + 1 );

    if (cgi_query == NULL)
    {
        PRINTF( "could not malloc(%d) bytes<br>\n", ContentLengthInt + 1 );
        return( -1 );
    }
    /* Read it in: */

    boundaryString = strstr( contentType, "boundary=" );
    if (boundaryString == NULL)
    {
        PRINTF( "could not find boundary string<br>\n" );
        return( -1 );
    }
    boundaryString += strlen( "boundary=" );

    {
        int           byteCount        = 0;
        int           fileBytes        = 0;
        int           fgetBytes        = 0;
        int           headerBytes      = 0;
        unsigned char endOfHeaderFound = 0;    /* set to true when \r\n found on a line by itself */
        unsigned char endOfFileFound   = 0;    /* set to true when file contents have been read */

        while (byteCount < ContentLengthInt && endOfFileFound == 0)
        {
            if (endOfHeaderFound == 0)
            {
                fgets( &cgi_query[fileBytes], ContentLengthInt + 1, stdin );
                fgetBytes  = strlen( &cgi_query[fileBytes] );
                byteCount += fgetBytes;
                PRINTF( "got %u bytes (%s)<br>\n", fgetBytes, &cgi_query[fileBytes] );

                /* if next line has a boundary directive on it */
                if (strstr( &cgi_query[fileBytes], boundaryString ))
                {
                    headerBytes += strlen( cgi_query ) + strlen( boundaryString ) + strlen( "--" ) + strlen( "--" ) + strlen( "\r\n" ) + strlen( "\r\n" );
                    PRINTF( "found boundary (%s); len %d; headerBytes %d; remaining %d<br>\n", &cgi_query[fileBytes], strlen( cgi_query ), headerBytes, ContentLengthInt - headerBytes );
                    cgi_query[fileBytes] = '\0';
                }
                else if (strncmp( &cgi_query[fileBytes], "Content-", 8 ) == 0)  /* if next line has a Content directive on it */
                {
                    headerBytes += strlen( cgi_query );
                    PRINTF( "found directive (%s); len %d; headerBytes %d; remaining %d<br>\n", &cgi_query[fileBytes], strlen( cgi_query ), headerBytes, ContentLengthInt - headerBytes );
                    if (strncmp( &cgi_query[fileBytes], "Content-Disposition:", 20 ) == 0)  /* if next line has a Content-Disposition directive on it */
                    {
                        char *filename = NULL;
                        /* find filename tag */
                        filename = strstr( &cgi_query[fileBytes], FILENAME_TAG );
                        if (filename)
                        {
                            char *end_of_filename = NULL;
                            filename       += strlen( FILENAME_TAG ); /* advance pointer past the tag name */
                            end_of_filename = strchr( filename, '"' );
                            if (end_of_filename)
                            {
                                unsigned int tempFilenameLen = end_of_filename - filename + strlen( GetTempDirectoryStr());
                                /* if destination string has space for the filename string passed in from the browser */
                                if (sFilename && ( tempFilenameLen < lFilenameLen ))
                                {
                                    memset( sFilename, 0, lFilenameLen );
                                    strncpy( sFilename, GetTempDirectoryStr(), lFilenameLen );
                                    strncat( sFilename, filename, end_of_filename - filename );
                                    PRINTF( "%s: sFilename %p:(%s)<br>\n", __FUNCTION__, sFilename, sFilename );
                                }
                            }
                        }
                    }
                    cgi_query[fileBytes] = '\0';
                }
                else if (( endOfHeaderFound == 0 ) && ( cgi_query[fileBytes] == '\r' ) && ( cgi_query[fileBytes + 1] == '\n' ))
                {
                    headerBytes += strlen( cgi_query );
                    PRINTF( "found end of header (%s) (skipping these bytes); headerBytes %d; remaining %d<br>\n", &cgi_query[fileBytes], headerBytes,
                        ContentLengthInt - headerBytes );
                    endOfHeaderFound     = 1;
                    cgi_query[fileBytes] = '\0';
                }
            }
            else
            {
                fgetBytes  = fread( &cgi_query[fileBytes], 1, ContentLengthInt - headerBytes, stdin );
                byteCount += fgetBytes;
                PRINTF( "added %d bytes to file contents<br>\n", fgetBytes );
                fileBytes       += fgetBytes;
                endOfHeaderFound = 0;
                endOfFileFound   = 1;
            }

            PRINTF( "byteCount %d; ContentLengthInt %d; fileBytes %d<br>\n", byteCount, ContentLengthInt, fileBytes );
        }

        PRINTF( "%s: fopen (%s)<br>\n", __FUNCTION__, sFilename );
        fOut = fopen( sFilename, "w" );
        if (fOut)
        {
            PRINTF( "%s: fwrite (%d) bytes<br>\n", __FUNCTION__, fileBytes );
            fwrite( cgi_query, 1, fileBytes, fOut );
            fclose( fOut );
            PRINTF( "<h3>Output file is (%s)</h3>\n", sFilename );
        }
        /* free the malloc'ed space if it was malloc'ed successfully */
        if (cgi_query)
        {
            free( cgi_query );
        }
    }

    return( 0 );
} /* readFileFromBrowser */

int sort_on_id(
    const void *a,
    const void *b
    )
{
    int rc = 0;

    bmemperf_client_data *client_a = (bmemperf_client_data *)a;
    bmemperf_client_data *client_b = (bmemperf_client_data *)b;

    rc = client_b->client_id - client_a->client_id;

    return( rc );
} /* sort_on_id */

int sort_on_id_rev(
    const void *a,
    const void *b
    )
{
    return( sort_on_id( b, a ));
} /* sort_on_id_rev */

int sort_on_bw(
    const void *a,
    const void *b
    )
{
    int rc = 0;

    bmemperf_client_data *client_a = (bmemperf_client_data *)a;
    bmemperf_client_data *client_b = (bmemperf_client_data *)b;

    rc = client_b->bw - client_a->bw;

    return( rc );
} /* sort_on_bw */

int sort_on_bw_rev(
    const void *a,
    const void *b
    )
{
    return( sort_on_bw( b, a ));
} /* sort_on_bw_rev */

int changeFileExtension(
    char       *destFilename,
    int         sizeofDestFilename,
    const char *srcFilename,
    const char *fromext,
    const char *toext
    )
{
    char *pos = NULL;

    PRINTF( "~%s: strcpy(%s)(%s)(%d)\n~", __FUNCTION__, destFilename, srcFilename, sizeofDestFilename );
    strncpy( destFilename, srcFilename, sizeofDestFilename );

    /* change the extension to new one ...   .tgz to 2.csv */
    pos = strstr( destFilename, fromext );
    PRINTF( "~%s: dest (%s)(%p); pos (%p); size (%d)\n~", __FUNCTION__, destFilename, destFilename, pos, sizeofDestFilename );
    PRINTF( "~%s: src  (%s)(%p); from (%s); to (%s)\n~", __FUNCTION__, srcFilename, srcFilename, fromext, toext );
    if (pos)
    {
        pos[0] = '\0'; /* null-terminate the beginning part of the name */
        strncat( destFilename, toext, sizeofDestFilename - strlen( destFilename ));
    }

    PRINTF( "destfilename (%s)\n", destFilename );

    return( 0 );
} /* changeFileExtension */

/**
 *  Function: This function will create the HTML needed to display the Top X clients.
 **/
int clientListCheckboxes(
    unsigned int top10Number
    )
{
    unsigned int idx = 0, memc = 0, client_idx = 0, displayCount[NEXUS_NUM_MEMC];

    printf( "~CLIENT_LIST_CHECKBOXES~" );
    PRINTF( "top10Number %u<br>\n", top10Number );

    memset( displayCount, 0, sizeof( displayCount ));

#define COLUMNS_PER_MEMC 2
    printf( "<table style=border-collapse:collapse; border=1 >\n" );
    for (idx = 0; idx<top10Number+2 /*there are two header rows */; idx++)
    {
        printf( "<tr>" );
        for (memc = 0; memc<g_bmemperf_info.num_memc; memc++)
        {
            /* 1st row */
            if (idx==0)
            {
                printf( "<th colspan=%u >MEMC %u</th>", COLUMNS_PER_MEMC, memc );
            }
            /* 2nd row */
            else if (idx==1)
            {
                printf( "<th style=\"width:50px;\"  >ID</th>" );
                printf( "<th style=\"width:150px;\" align=left >Name</th>" );
            }
            else
            {
                client_idx = idx-2;
                if (( strncmp( g_client_name[client_idx], "sage", 4 ) != 0 ) && ( strncmp( g_client_name[client_idx], "bsp", 3 ) != 0 ))
                {
                    printf( "<td id=client%umemc%ubox align=left ><input type=checkbox id=client%umemc%u onclick=\"MyClick(event);\" >%u</td>",
                        client_idx, memc, client_idx, memc, client_idx );
                    printf( "<td id=client%umemc%uname >%s</td>", client_idx, memc, g_client_name[client_idx] );
                }
            }
        }
        PRINTF( "<br>\n" );
        printf( "</tr>\n" );
    }
    printf( "</table>\n" );
    return( 0 );
} /* clientListCheckboxes */

/**
 *  Function: This function will convert a number to its string value while adding commas to separate the
 *  thousands, millions, billions, etc.
 **/
int convert_to_string_with_commas(
    unsigned long int value,
    char             *valueStr,
    unsigned int      valueStrLen
    )
{
    unsigned long int numCommas = 0;
    unsigned long int numDigits = 0;
    char             *newString = NULL;
    unsigned long int newLen    = 0;

    numCommas = log10( value )/3;
    numDigits = trunc( log10( value ))+1;

    newLen    = numDigits + numCommas + 1 /* null terminator */;
    newString = (char *) malloc( newLen );
    if (newString)
    {
        unsigned int comma      = 0;
        unsigned int groupStart = 0;

        memset( newString, 0, newLen );

        sprintf( newString, "%lu", value );
        /*printf("%s: newString (%s); len %lu; space %lu\n", __FUNCTION__, newString, strlen(newString), newLen );*/

        /* 999999999 -> 999,999,999 */
        for (comma = 1; comma<=numCommas; comma++) {
            groupStart = numDigits - ( 3*comma );
            /*           1 ...           1111 */ /* 8,9,10 -> 11,12,13 */ /* 5,6,7 -> 7,8,9 */ /* 2,3,4 -> 3,4,5 */
            /* 01234567890 ... 01234567890123 */
            /* 11222333444 ... 11 222 333 444 */
            /*printf("%s: [%lu] <- [%u]\n", __FUNCTION__, groupStart + 2 + numCommas- (comma - 1) , groupStart + 2);*/
            newString[groupStart + 2 + numCommas - ( comma - 1 )] = newString[groupStart + 2];
            /*printf("%s: [%lu] <- [%u]\n", __FUNCTION__, groupStart + 1 + numCommas- (comma - 1) , groupStart + 1);*/
            newString[groupStart + 1 + numCommas - ( comma - 1 )] = newString[groupStart + 1];
            /*printf("%s: [%lu] <- [%u]\n", __FUNCTION__, groupStart + 0 + numCommas- (comma - 1) , groupStart + 0);*/
            newString[groupStart + 0 + numCommas - ( comma - 1 )] = newString[groupStart + 0];
            /*printf("%s: [%lu] <- ','\n", __FUNCTION__, groupStart - 1 + numCommas- (comma - 1)  );*/
            newString[groupStart - 1 + numCommas - ( comma - 1 )] = ',';
            /*printf("%s: comma %u; groupStart %u; (%s)\n", __FUNCTION__, comma, groupStart, newString);*/
        }

        if (valueStr && ( valueStrLen>1 ))
        {
            strncpy( valueStr, newString, valueStrLen -1 );
        }
        /*printf("%s", newString );*/

        free( newString );
    }

    return( 0 );
} /* convert_to_string_with_commas */

unsigned long int get_interrupt_total(
    void
    )
{
    unsigned long int irqTotal = 0;
    FILE             *fpStats  = NULL;
    char              buf[1024];
    char             *pos = NULL;

    fpStats = fopen( PROC_STAT_FILE, "r" );
    if (fpStats != NULL)
    {
        while (fgets( buf, sizeof( buf ), fpStats ))
        {
            pos = strchr( buf, '\n' );
            if (pos)
            {
                *pos = 0; /* null-terminate the line */
            }
            if (strncmp( buf, "intr ", 5 ) == 0)
            {
                /* just read the first number. it contains the total number of interrupts since boot */
                sscanf( buf + 5, "%lu", &irqTotal );
                PRINTF( "%s: irqTotal %lu\n", __FUNCTION__, irqTotal );
            }
        }

        fclose( fpStats );
    }
    else
    {
        fprintf( stderr, "Could not open %s\n", PROC_STAT_FILE );
    }
    return( irqTotal );
} /* get_interrupt_total */

int get_interrupt_counts(
    bmemperf_irq_data *irqData
    )
{
    struct stat       statbuf;
    char             *contents      = NULL;
    unsigned long     numBytes      = 0;
    FILE             *fpText        = NULL;
    char             *posstart      = NULL;
    char             *posend        = NULL;
    unsigned long int lineNum       = 0;
    unsigned long int cpu           = 0;
    unsigned int      numProcessors = sysconf( _SC_NPROCESSORS_ONLN );
    unsigned int      numCpusActive = 0;
    unsigned int      numLines      = 0;
    char              cmd[64+TEMP_FILE_FULL_PATH_LEN];
    char              tempFilename[TEMP_FILE_FULL_PATH_LEN];

    PrependTempDirectory( tempFilename, sizeof( tempFilename ), TEMP_INTERRUPTS_FILE );

    setlocale( LC_NUMERIC, "" );

    memset( &statbuf, 0, sizeof( statbuf ));

    /* create a shell command to concatinate the cumuulative interrupt counts file to the temp directory */
    sprintf( cmd, "cat %s > %s", PROC_INTERRUPTS_FILE, tempFilename );
    system( cmd );

    if (lstat( tempFilename, &statbuf ) == -1)
    {
        printf( "%s - (%s); lstat failed; %s\n", __FUNCTION__, tempFilename, strerror( errno ));
        return( -1 );
    }

    /*printf("size of (%s) %lu\n", tempFilename, statbuf.st_size);*/

    if (statbuf.st_size == 0)
    {
        printf( "could not determine interrupts file (%s) size.\n", tempFilename );
        return( -1 );
    }

    contents = (char *) malloc( statbuf.st_size + 1 );
    if (contents == NULL)
    {
        printf( "could not malloc(%lu+1) bytes\n", (unsigned long int) statbuf.st_size );
        return( -1 );
    }

    memset( contents, 0, statbuf.st_size + 1 );

    fpText = fopen( tempFilename, "r" );
    if (fpText == NULL)
    {
        printf( "could not fopen(%s)\n", tempFilename );
        return( -1 );
    }
    numBytes = fread( contents, 1, statbuf.st_size, fpText );

    fclose( fpText );

    /* we are done with the temp file; delete it. */
    remove( tempFilename );

    if (numBytes != statbuf.st_size)
    {
        printf( "tried to fread %lu bytes but got %lu\n", (unsigned long int) statbuf.st_size, numBytes );
        return( -1 );
    }

    posstart = contents; /* start parsing the file at the beginning of the file */

    /* step through the file line by line, searching for interrupt counts for each CPU */
    do {
        posend = strstr( posstart, "\n" );
        if (posend)
        {
            *posend = '\0';
            posend++;
        }
        /*printf("next line (%s); posend %p\n", posstart, posend );*/

        numLines++;

        if (lineNum == 0)
        {
            char *cp, *restOfLine;
            restOfLine = posstart;

            /*printf("numProcessors %u\n", numProcessors);*/
            while (( cp = strstr( restOfLine, "CPU" )) != NULL && numCpusActive < numProcessors)
            {
                cpu = strtoul( cp + 3, &restOfLine, BMEMPERF_IRQ_VALUE_LENGTH );
                numCpusActive++;
            }
            /*printf("found %u cpus in header; numProcessors %u\n", numCpusActive, numProcessors );*/
        }
        else if (strlen( posstart ))
        {
            char        *cp         = NULL;
            char        *restOfLine = NULL;
            char        *pos        = NULL;
            unsigned int irqIdx     = lineNum - 1; /* the first line has the header on it */

            if (irqIdx < BMEMPERF_IRQ_MAX_TYPES)
            {
                /* Skip over "IRQNAME:" */
                cp = strchr( posstart, ':' );
                if (!cp)
                {
                    continue;
                }

                cp++;

                pos  = cp;
                pos += numCpusActive * ( BMEMPERF_IRQ_VALUE_LENGTH + 1 ); /* each number is 10 digits long separated by 1 space */
                pos += 2;                                                 /* after all of the numbers are listed, the name is separated by 2 more spaces */

                /* some names have a few spaces at the beginning of them; advance the pointer past all of the beginning spaces */
                while (*pos == ' ')
                {
                    pos++;
                }

                /* the line is long enough to have a name at the end of it */
                if (pos < ( cp + strlen( cp )))
                {
                    strncpy( irqData->irqDetails[irqIdx].irqName, pos, sizeof( irqData->irqDetails[irqIdx].irqName ));
                    PRINTF( "%s: added new irq to idx %2u (%s)\n", __FUNCTION__, irqIdx, pos );
                }

                PRINTF( "line %3u: (%s) ", numLines, cp );
                for (cpu = 0; cpu < numCpusActive; cpu++)
                {
                    unsigned long int value = 0;

                    /* parse the next value from the current line */
                    value = strtoul( cp, &restOfLine, BMEMPERF_IRQ_VALUE_LENGTH );

                    /* add interrupt count to the running value for this CPU */
                    irqData->irqCount[cpu] += value;

                    /* save the value for this specific IRQ */
                    irqData->irqDetails[irqIdx].irqCount[cpu] = value;
                    PRINTF( "%lu ", value );

                    cp = restOfLine;
                    if (cp == NULL)
                    {
                        break;
                    }
                }
                PRINTF( "\n" );
            }
            else
            {
                fprintf( stderr, "%s: irqIdx (%u) exceeded BMEMPERF_IRQ_MAX_TYPES\n", __FUNCTION__, irqIdx );
            }
        }

        posstart = posend;

        lineNum++;
    } while (posstart != NULL);

    if (contents) {free( contents ); }

    /* add up all of the cpu's interrupts into one big total */
    for (cpu = 0; cpu < numCpusActive; cpu++)
    {
        irqData->irqTotal += irqData->irqCount[cpu];
    }
    PRINTF( "after %u lines, total %lu\n\n\n", numLines, irqData->irqTotal );

    return( 0 );
} /* get_interrupt_counts */

int getCpuOnline(
    bmemperf_cpu_status *pCpuStatus
    )
{
#define MAX_LENGTH_CPU_STATUS 128
    unsigned int cpu = 0;
    FILE        *cmd = NULL;
    char         line[MAX_LENGTH_CPU_STATUS];

    if (pCpuStatus == NULL)
    {
        return( -1 );
    }

    /* clear out the array */
    memset( pCpuStatus, 0, sizeof( *pCpuStatus ));

    sprintf( line, "grep processor /proc/cpuinfo" );
    cmd = popen( line, "r" );

    PRINTF( "detected cpu: " );
    do {
        memset( line, 0, sizeof( line ));
        fgets( line, MAX_LENGTH_CPU_STATUS, cmd );
        if (strlen( line ))
        {
            char *pos = strchr( line, ':' );
            if (pos)
            {
                pos++;
                cpu = strtoul( pos, NULL, BMEMPERF_CPU_VALUE_LENGTH );
                PRINTF( "%u ", cpu );
                if (cpu < BMEMPERF_MAX_NUM_CPUS)
                {
                    pCpuStatus->isActive[cpu] = true;
                }
            }
        }
    } while (strlen( line ));
    PRINTF( "\n" );

    pclose( cmd );
    return( 0 );
} /* getCpuOnline */

static float gUptime = 0.0;

int getUptime(
    float *uptime
    )
{
    if (uptime == NULL)
    {
        return( 0 );
    }

    *uptime = gUptime;

    return( 0 );
} /* getUptime */

int setUptime(
    void
    )
{
    long int numBytes     = 0;
    FILE    *fpProcUptime = NULL;
    char     bufProcStat[256];

    fpProcUptime = fopen( "/proc/uptime", "r" );
    if (fpProcUptime==NULL)
    {
        printf( "could not open /proc/uptime\n" );
        return( -1 );
    }

    numBytes = fread( bufProcStat, 1, sizeof( bufProcStat ), fpProcUptime );
    fclose( fpProcUptime );

    if (numBytes)
    {
        sscanf( bufProcStat, "%f.2", &gUptime );
#ifdef  BMEMPERF_CGI_DEBUG
        {
            static float uptimePrev = 0;
            printf( "uptime: %8.2f (delta %8.2f)\n", gUptime, ( gUptime - uptimePrev ));
            uptimePrev = gUptime;
        }
#endif /* ifdef  BMEMPERF_CGI_DEBUG */
    }
    return( 0 );
} /* setUptime */

char *GetFileContents(
    const char *filename
    )
{
    char       *contents = NULL;
    FILE       *fpInput  = NULL;
    struct stat statbuf;

    if (lstat( filename, &statbuf ) == -1)
    {
        printf( "Could not stat (%s)\n", filename );
        return( NULL );
    }

    contents = malloc( statbuf.st_size + 1 );
    if (contents == NULL) {return( NULL ); }

    memset(contents, 0, statbuf.st_size + 1 );

    if ( statbuf.st_size )
    {
        fpInput = fopen( filename, "r" );
        fread( contents, 1, statbuf.st_size, fpInput );
        fclose( fpInput );
    }

    return( contents );
} /* GetFileContents */

#ifdef USE_BOXMODES
int bmemperf_boxmode_init(
    long int boxmode
    )
{
    unsigned int idx         = 0;
    bool         bMatchFound = false;

    PRINTF( "~boxmode:%ld; before ... %u, %u, %u\n~", boxmode, g_bmemperf_info.num_memc, g_bmemperf_info.ddrFreqInMhz, g_bmemperf_info.scbFreqInMhz );

    if (boxmode > 0)
    {
        /* loop through all boxmodes to find the client names, MHz, and SCB freq for the current box mode */
        for (idx = 0; idx<( sizeof( g_boxmodes )/sizeof( g_boxmodes[0] )); idx++)
        {
            if (( g_boxmodes[idx].boxmode > 0 ) && g_boxmodes[idx].g_bmemperf_info && g_boxmodes[idx].g_client_name)
            {
                if (boxmode == (long int) g_boxmodes[idx].boxmode)
                {
                    printf( "~boxmode:%ld; copying from idx %u\n~", boxmode, idx );
                    memcpy( &g_bmemperf_info, g_boxmodes[idx].g_bmemperf_info, sizeof( g_bmemperf_info ));
                    printf( "~boxmode:%ld; %u, %u, %u\n~", boxmode, g_bmemperf_info.num_memc, g_bmemperf_info.ddrFreqInMhz, g_bmemperf_info.scbFreqInMhz );
                    memcpy( &g_client_name, g_boxmodes[idx].g_client_name, sizeof( g_client_name ));
                    boxmode     = g_boxmodes[idx].boxmode;
                    bMatchFound = true;
                    break;
                }
            }
            else
            {
                printf( "~boxmode:%ld; stopped looking at idx %u\n~", boxmode, idx );
                break;
            }
        }
    }

    /* if a match was not found above, then use the first entry in the array */
    if (bMatchFound == false)
    {
        idx = 0;
        printf( "~boxmode:%ld; copying from default idx %u\n~", boxmode, idx );
        memcpy( &g_bmemperf_info, g_boxmodes[idx].g_bmemperf_info, sizeof( g_bmemperf_info ));
        memcpy( &g_client_name, g_boxmodes[idx].g_client_name, sizeof( g_client_name ));
        boxmode = g_boxmodes[idx].boxmode;
    }

    PRINTF( "~boxmode: returning %ld\n~", boxmode );
    return( boxmode );
} /* bmemperf_boxmode_init */

#endif /* USE_BOXMODES */

int overall_stats_html(
    bmemperf_response *pResponse
    )
{
    unsigned int      memc = 0;
    unsigned int      cpu  = 0;
    unsigned int      numProcessorsConfigured = sysconf( _SC_NPROCESSORS_CONF );
    unsigned int      numProcessorsOnline     = sysconf( _SC_NPROCESSORS_ONLN );
    float             averageCpuUtilization   = 0.0;
    bmemperf_irq_data irqData;
    char              irqTotalStr[64];
    unsigned int      numberOfMemc = g_bmemperf_info.num_memc;

    PRINTF( "%s: numberOfMemc %u<br>\n", __FUNCTION__, numberOfMemc );
    printf( "~OVERALL~" );
    printf( "<table ><tr><th>OVERALL STATISTICS:</th></tr></table>\n" );
    printf( "<table style=border-collapse:collapse; border=1 >\n" );

    memset( &irqData, 0, sizeof( irqData ));
    memset( &irqTotalStr, 0, sizeof( irqTotalStr ));

    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><th>&nbsp;</th>" );
        }
        printf( "<th>MEMC&nbsp;%u</th>", memc );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>Data BW&nbsp;(%s)</td>", g_MegaBytesStr[g_MegaBytes] );
        }
        printf( "<td>%u</td>", pResponse->response.overallStats.systemStats[memc].dataBW / g_MegaBytesDivisor[g_MegaBytes] );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>Transaction&nbsp;BW&nbsp;(%s)</td>", g_MegaBytesStr[g_MegaBytes] );
        }
        printf( "<td>%u</td>", pResponse->response.overallStats.systemStats[memc].transactionBW / g_MegaBytesDivisor[g_MegaBytes] );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>Idle&nbsp;BW&nbsp;(%s)</td>", g_MegaBytesStr[g_MegaBytes] );
        }
        printf( "<td>%u</td>", pResponse->response.overallStats.systemStats[memc].idleBW / g_MegaBytesDivisor[g_MegaBytes] );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>Utilization %%</td>" );
        }
        printf( "<td>%5.2f</td>", pResponse->response.overallStats.systemStats[memc].dataUtil );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>DDR&nbsp;Freq&nbsp;(MHz)</td>" );
        }
        printf( "<td>%u</td>", pResponse->response.overallStats.systemStats[memc].ddrFreqMhz );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }
    for (memc = 0; memc<numberOfMemc; memc++)
    {
        /* first column */
        if (memc == 0)
        {
            printf( "<tr><td>SCB&nbsp;Freq&nbsp;(MHz)</td>" );
        }
        printf( "<td>%u</td>", pResponse->response.overallStats.systemStats[memc].scbFreqMhz );

        /* last column */
        if (( memc+1 ) == numberOfMemc)
        {
            printf( "</tr>\n" );
        }
    }

    printf( "</table>~" );

    /* convert 999999 to 999,999 ... e.g. add commas */
    convert_to_string_with_commas( pResponse->response.overallStats.irqData.irqTotal,  irqTotalStr, sizeof( irqTotalStr ));

    printf( "~DETAILSTOP~" );
    printf( "<table border=0 style=\"border-collapse:collapse;\" width=\"100%%\" ><tr><th width=160px align=left >CLIENT DETAILS:</th>" );
    printf( "<th align=right ><table border=0 style=\"border-collapse:collapse;\" ><tr><th align=left>CPU&nbsp;UTILIZATION:&nbsp;&nbsp;</th>" );
    for (cpu = 0; cpu<numProcessorsConfigured; cpu++)
    {
        /* 255 indicates this CPU is offline */
        if (pResponse->response.overallStats.cpuData.idlePercentage[cpu] != 255)
        {
            averageCpuUtilization += pResponse->response.overallStats.cpuData.idlePercentage[cpu];
            printf( "<td align=center style=\"width:50px;border-top: solid thin; border-right: solid thin; border-left: solid thin; border-bottom: solid thin\" >%u%%</td>",
                pResponse->response.overallStats.cpuData.idlePercentage[cpu] );
        }
    }
    averageCpuUtilization /= numProcessorsOnline;
    printf( "<td align=center style=\"width:50px;border-top: solid thin; border-right: solid thin; border-left: solid thin; border-bottom: solid thin; "
            "background-color:lightgray;\"  >%5.1f%%</td>", averageCpuUtilization );
    printf( "<td>&nbsp;&nbsp;Irq/sec:&nbsp;&nbsp;</td>"
            "<td align=center style=\"width:50px;border-top: solid thin; border-right: solid thin; border-left: solid thin; border-bottom: solid thin; "
            "background-color:lightgray;\"  >%s</td>", irqTotalStr );
    printf( "</tr></table></th></tr></table>~\n" );

    return( 0 );
} /* overall_stats_html */

unsigned int getNextNonZeroIdx(
    unsigned int       displayCount,
    unsigned int       memc,
    bmemperf_response *pResponse,
    int                sortColumn
    )
{
    unsigned int client;
    unsigned int nonZeroCount = 0;

    BSTD_UNUSED( sortColumn );

    for (client = 0; client<BMEMPERF_MAX_NUM_CLIENT; client++)
    {
        /* if bandwidth is non-zero OR we are sorting on ID column or NAME column */
        /*if (pResponse->response.overallStats.clientOverallStats[memc].clientData[client].bw > 0 || abs(sortColumn) != BMEMPERF_COLUMN_BW )*/
        if (pResponse->response.overallStats.clientOverallStats[memc].clientData[client].client_id != BMEMPERF_CGI_INVALID)
        {
            if (nonZeroCount == displayCount)
            {
                PRINTF( "%s: for memc %u, idx %u\n", __FUNCTION__, memc, client );
                break;
            }
            nonZeroCount++;
        }
    }
    return( client );
} /* getNextNonZeroIdx */

/**
 *  Function: This function will create the HTML needed to display the Top X clients.
 **/
int top10_html(
    bmemperf_response *pResponse,
    unsigned int       top10Number
    )
{
    unsigned int idx = 0, memc = 0, client_idx = 0, displayCount[NEXUS_NUM_MEMC];
    char         client_checked[8];
    unsigned int numberOfMemc = g_bmemperf_info.num_memc;

    printf( "%s: top10Number %u; numberOfMemc %u\n", __FUNCTION__, top10Number, numberOfMemc );
    printf( "~TOP10~" );

    for (memc = 0; memc<numberOfMemc; memc++)
    {
        displayCount[memc] = 0;
    }

#define TOP_COLUMNS_PER_MEMC 5
    printf( "<table style=border-collapse:collapse; border=1 >\n" );
    for (idx = 0; idx<top10Number+2 /*there are two header rows */; idx++)
    {
        printf( "<tr>" );
        for (memc = 0; memc<numberOfMemc; memc++)
        {
            /* 1st row */
            if (idx==0)
            {
                printf( "<th colspan=%u >MEMC %u</th>", TOP_COLUMNS_PER_MEMC, memc );
            }
            /* 2nd row */
            else if (idx==1)
            {
                /* negative values mean sort the opposite direction */
                int temp = (int) pResponse->response.overallStats.sortColumn[memc];
                PRINTF( "%s: sortColumn temp %d\n", __FUNCTION__, temp );
                printf( "<th style=\"width:50px;\"  id=sort%uid   onclick=\"MyClick(event);\" >ID</th>", memc );
                printf( "<th style=\"width:150px;\" id=sort%uname onclick=\"MyClick(event);\" align=left >Name</th>", memc );
                printf( "<th style=\"width:40px;\"><table><tr><th id=sort%ubw   onclick=\"MyClick(event);\" >BW&nbsp;(%s)</th></tr></table></th>", memc,
                    g_MegaBytesStr[g_MegaBytes] );
                printf( "<th>RR</th><th>Block&nbspOut</th>\n" );
            }
            else
            {
                client_idx = idx-2;
                {
                    char        *background     = NULL;
                    unsigned int background_len = 0;
                    char         rr_checked[8];
                    unsigned int nonZeroIdx = 0;
                    bool         isChecked  = 0;
                    int          temp       = (int) pResponse->response.overallStats.sortColumn[memc];

                    nonZeroIdx = getNextNonZeroIdx( displayCount[memc], memc, pResponse, temp );

                    if (memc==0) {PRINTF( "%s: idx %u: client %u: memc %u; displayCount %u; nonZeroIdx %u;<br>\n", __FUNCTION__, idx, client_idx, memc, displayCount[memc], nonZeroIdx ); }
                    /* if BW is zero OR name contains unprintable character, do not print anything */
                    if (nonZeroIdx >= BMEMPERF_MAX_NUM_CLIENT)
                    {
                        printf( "<td colspan=%u>&nbsp;</td>\n", TOP_COLUMNS_PER_MEMC );
                    }
                    else
                    {
                        isChecked = pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].is_detailed;
                        if (memc==0)
                        {
                            PRINTF( "memc %u; cnt:%u-nonZero %u; id %u; chkd-%u;<br>", memc, displayCount[memc], nonZeroIdx,
                                pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id, isChecked );
                        }
                        memset( client_checked, 0, sizeof( client_checked ));
                        memset( rr_checked, 0, sizeof( rr_checked ));

                        if (isChecked)
                        {
                            strncpy( client_checked, "checked", sizeof( client_checked ));
                        }

                        if (pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].rr)
                        {
                            strncpy( rr_checked, "checked", sizeof( rr_checked ));
                        }

                        if (pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].err_cnt)
                        {
                            background_len = strlen( BACKGROUND_RED ) + 1;
                            background     = malloc( background_len );
                            strncpy( background, BACKGROUND_RED, background_len-1 );
                            background[background_len-1] = '\0';
                        }
                        else if (isChecked)
                        {
                            background_len = strlen( BACKGROUND_YELLOW ) + 1;
                            background     = malloc( background_len );
                            strncpy( background, BACKGROUND_YELLOW, background_len-1 );
                            background[background_len-1] = '\0';
                        }
                        else
                        {
                            background_len = strlen( BACKGROUND_WHITE ) + 1;
                            background     = malloc( background_len );
                            strncpy( background, BACKGROUND_WHITE, background_len-1 );
                            background[background_len-1] = '\0';
                        }
                        printf( "<td align=left %s ><input type=checkbox id=client%umemc%u onclick=\"MyClick(event);\" %s >%u</td>", background,
                            pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id, memc, client_checked,
                            pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id );
                        printf( "<td %s >%s",
                            background, g_client_name[pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id] );
                        if (pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].err_cnt)
                        {
                            printf( " (%u)", pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].err_cnt );
                        }
                        printf( "</td><td align=center %s >%u</td>",
                            background, pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].bw / g_MegaBytesDivisor[g_MegaBytes] );
                        printf( "<td %s ><input type=checkbox id=client%urrobin%u %s onclick=\"MyClick(event);\" ></td>",
                            background, pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id, memc, rr_checked );
                        printf( "<td %s ><input type=text %s id=client%ublockout%u onmouseover=\"MyMouseOver(event);\" onmouseout=\"MyMouseOut(event);\" size=5 value=%u "
                                "onchange=\"MyChange(event);\" /></td>\n", background, background,
                            pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].client_id, memc,
                            pResponse->response.overallStats.clientOverallStats[memc].clientData[nonZeroIdx].block_out );

                        if (background)
                        {
                            free( background );
                        }
                    }

                    displayCount[memc]++;
                }
            }
        }
        PRINTF( "<br>\n" );
        printf( "</tr>\n" );
    }
    printf( "</table>\n" );
    return( 0 );
} /* top10_html */

int get_boxmode(
    bmemperf_boxmode_source *pboxmode_info
    )
{
    unsigned    boxMode = 0;
    const char *override;
    FILE       *pFile = NULL;

    if (pboxmode_info == NULL)
    {
        return( -1 );
    }

    override = getenv( "B_REFSW_BOXMODE" );
    if (override)
    {
        boxMode = atoi( override );
    }
    if (!boxMode)
    {
        pFile = fopen( BOXMODE_RTS_FILE, "r" );
        pboxmode_info->source = BOXMODE_RTS;
    }
    else
    {
        pboxmode_info->source = BOXMODE_ENV;
    }
    if (pFile)
    {
        char buf[64];
        if (fgets( buf, sizeof( buf ), pFile ))
        {
            /* example: 20140402215718_7445_box1, but skip over everything (which may change) and get to _box# */
            char *p = strstr( buf, "_box" );
            if (p)
            {
                boxMode = atoi( &p[4] );
            }
        }
        fclose( pFile );
    }
    pboxmode_info->boxmode = boxMode;
    return( 0 );
} /* get_boxmode */

/**
 *  Function: This function will return to the user the known temporary path name. In Linux system, this
 *  file will be /tmp/. In Android systems, it will be dictated by the environment variable B_ANDROID_TEMP.
 **/
char *GetTempDirectoryStr(
    void
    )
{
    static char tempDirectory[TEMP_FILE_FULL_PATH_LEN] = "empty";
    char       *contents     = NULL;
    char       *posErrorLog  = NULL;
    char       *posLastSlash = NULL;
    char       *posEol       = NULL;

    PRINTF( "~%s: tempDirectory (%s)\n~", __FUNCTION__, tempDirectory );
    /* if the boa.conf file has no yet been scanned for the temporary directory, do it now */
    if (strncmp( tempDirectory, "empty", 5 ) == 0)
    {
        contents = GetFileContents( "boa.conf" );

        /* if the contents of boa.conf were successfully read */
        if (contents)
        {
            posErrorLog = strstr( contents, "\nErrorLog " );
            PRINTF( "~%s: posErrorLog (%p)\n~", __FUNCTION__, posErrorLog );
            if (posErrorLog != NULL)
            {
                posErrorLog += strlen( "\nErrorLog " );
                /* look for the end of the ErrorLog line */
                posEol = strstr( posErrorLog, "\n" );

                PRINTF( "~%s: posErrorLog (%p); posEol (%p)\n~", __FUNCTION__, posErrorLog, posEol );
                /* if end of ErrorLog line found */
                if (posEol)
                {
                    posEol[0] = '\0'; /* terminate the end of the line so that the strrchr() call works just on this line */

                    posLastSlash = strrchr( posErrorLog, '/' );
                    PRINTF( "~%s: posLastSlash (%p)(%s)\n~", __FUNCTION__, posLastSlash, posErrorLog );
                }
            }
            else
            {
                PRINTF( "~ALERT~%s: could not find ErrorLog line in boa.conf\n~", __FUNCTION__ );
            }
        }
    }

    /* if the last forward slash was found on the ErrorLog line */
    if (posErrorLog && posLastSlash)
    {
        posLastSlash[1] = '\0';
        PRINTF( "~%s: detected temp directory in boa.conf of (%s)\n~", __FUNCTION__, posErrorLog );
        strncpy( tempDirectory, posErrorLog, sizeof( tempDirectory ) -1 );
    }
    /* if the temp directory is already set to something previously */
    else if (strncmp( tempDirectory, "empty", 5 ) != 0)
    {
        /* use the previous setting */
    }
    else
    {
        strncpy( tempDirectory, "/tmp/", sizeof( tempDirectory ) -1 );
        PRINTF( "~%s: using default temp directory of (%s)\n~", __FUNCTION__, tempDirectory );
    }

    if (contents)
    {
        free( contents );
    }
    PRINTF( "~%s: returning (%s)\n~", __FUNCTION__, tempDirectory );
    return( tempDirectory );
} /* GetTempDirectoryStrg */

/**
 *  Function: This function will prepend the specified file name with the known temporary path name.
 **/
void PrependTempDirectory(
    char       *filenamePath,
    int         filenamePathLen,
    const char *filename
    )
{
    if (filenamePath)
    {
        strncpy( filenamePath, GetTempDirectoryStr(), filenamePathLen -1 );
        strncat( filenamePath, filename,              filenamePathLen -1 );

        PRINTF( "~%s: returning (%s)\n~", __FUNCTION__, filenamePath );
    }

    return;
} /* PrependTempDirectory */

/**
 *  Function: This function will scan the specified string and return true if a number digit is found; it will
 *  return false otherwise.
 **/
bool hasNumeric(
    const char *mystring
    )
{
    const char *nextchar = mystring;

    while (*nextchar != '\0')
    {
        if (( '0' <= *nextchar ) && ( *nextchar <= '9' ))
        {
            return( 1 );
        }
        nextchar++;
        /*printf("nextchar (%c)\n", *nextchar );*/
    }

    /*printf("string (%s) has no numbers\n", mystring );*/
    return( 0 );
} /* hasNumeric */
