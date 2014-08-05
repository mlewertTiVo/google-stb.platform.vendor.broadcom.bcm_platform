/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: nb_io_win32.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Support for non-blocking console I/O on Windows.
=============================================================================*/

#include <conio.h>   /* For _kbhit() */

#include "nb_io.h"

void nb_set_nonblocking_input(int enable)
{
   /* No need to do anything in Win32 */
   (void)enable;
}

int nb_char_available()
{
   return _kbhit();
}

char nb_get_char()
{
   char c = _getch();
   _putch(c);   /* Echo back */
   return c;
}

void nb_put_char(char ch)
{
   _putch(ch);
}
