/*=============================================================================
Copyright (c) 2013 Broadcom Europe Limited.
All rights reserved.

Project  :  Containers Library
Module   :  Containers Core

FILE DESCRIPTION
snprintf for Visual Studio
=============================================================================*/

#ifdef _MSC_VER
#include <stdlib.h>
#include <string.h>
#include "containers/core/containers_private.h"

/* At the time of writing Visual Studio does not have a C99-compatible snprintf or vsnprintf.
 * This uses existing APIs to emulate it. */

/* print to str, maximum size characters, using format and the variable args */
int snprintf(char* outbuffer, size_t buffersize, const char* format, ...)
{
   int count;
   va_list args;
   va_start(args, format);

   // Pass through to the next function
   count = vsnprintf(outbuffer, buffersize, format, args);

   va_end(args);

   return count;
}

int vsnprintf(char* outbuffer, size_t buffersize, const char* format, va_list ap)
{
   /* Translate the string, 
    * Set count to -1 if the buffer is empty, or if the string doesn't fit */
   int count = _vsnprintf_s(outbuffer, buffersize, _TRUNCATE, format, ap);

   /* As required by the ANSI vsnprintf specification,
   * calculate the length of buffer that would have been written
   * if count had been large enough */
   if (count == -1)
   {
      count = _vscprintf(format, ap);
   }

   return count;
}

#endif // _MSC_VER