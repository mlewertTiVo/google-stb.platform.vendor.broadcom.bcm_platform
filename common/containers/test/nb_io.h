/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.

Project  :  Container Library
Module   :  Test
File     :  $RCSfile: nb_io.h,v $
Revision :  $Revision: #0 $

FILE DESCRIPTION
Public interface definition for non-blocking I/O support.
=============================================================================*/

#ifndef _NB_IO_H_
#define _NB_IO_H_

/** Set whether console input is non-blocking or not.
 *
 * \param enable Pass true to set input as non-blocking, false to set it as blocking again.*/
void nb_set_nonblocking_input(int enable);

/** \return Non-zero when there is a character available to read using nb_get_char(). */
int nb_char_available(void);

/** \return The next character available from the console, or zero. */
char nb_get_char(void);

/** Put the character out to the console.
 *
 * \param ch The character to be output. */
void nb_put_char(char ch);

#endif /* _NB_IO_H_ */
