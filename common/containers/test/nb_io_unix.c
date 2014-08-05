/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: nb_io_unix.c,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Support for non-blocking console I/O on Unix-compatible platforms.
=============================================================================*/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

#include "nb_io.h"

void nb_set_nonblocking_input(int enable)
{
   struct termios ttystate;

    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);

    if (enable)
    {
        //turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    }
    else
    {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }

    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

int nb_char_available(void)
{
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}

char nb_get_char(void)
{
   return getchar();
}

void nb_put_char(char ch)
{
   putchar(ch);
}
