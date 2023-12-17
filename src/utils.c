/*
 * Copyright 2018-2023 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/CD.
 *
 * adriagipas/CD is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/CD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/CD.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  utils.c - Implementació de 'utils.h'.
 *
 */


#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CD.h"
#include "utils.h"




/**********/
/* MACROS */
/**********/

#define isreturn(c) ((c) == '\r' || (c) == '\n')

#define BCD(NUM) ((uint8_t) (((NUM)/10)*0x10 + (NUM)%10))




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
resize_buffer (
               CD_Buffer *buf
               )
{

  buf->s*= 2;
  buf->v= mem_realloc ( char, buf->v, buf->s );
  
} // end resize_buffer




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
CD_msgerror (
             char       **err,
             const char  *format,
             ...
             )
{

  va_list ap;
  static char buffer[1000];
  
  
  if  ( err == NULL ) return;
  va_start ( ap, format );
  vsnprintf ( buffer, 1000, format, ap );
  va_end ( ap );
  *err= mem_alloc ( char, strlen(buffer)+1 );
  strcpy ( *err, buffer );
  
} // end CD_msgerror


void *
CD_mem_alloc__ (
                size_t nbytes
                )
{
  
  void *ret;
  
  
  ret= malloc ( nbytes );
  if ( ret == NULL )
    {
      fprintf ( stderr, "[EE] cannot allocate memory\n" );
      exit ( EXIT_FAILURE );
    }
  
  return ret;
  
} // end CD_mem_alloc__


void *
CD_mem_realloc__ (
                  void * ptr,
                  size_t nbytes
                  )
{
  
  void *ret;
  
  
  ret= realloc ( ptr, nbytes );
  if ( ret == NULL )
    {
      fprintf ( stderr, "[EE] cannot allocate memory\n" );
      exit ( EXIT_FAILURE );
    }
  
  return  ret;
  
} // CD_mem_realloc__


CD_Buffer *
CD_buffer_new (void)
{

  CD_Buffer *new;


  new= mem_alloc ( CD_Buffer, 1 );
  new->v= mem_alloc ( char, 2 );
  new->s= 2;

  return new;
  
} // end CD_buffer_new


void
CD_buffer_free (
        	CD_Buffer *buf
        	)
{

  free ( buf->v );
  free ( buf );
  
} // end CD_buffer_free


int
CD_gline (
          FILE      *f,
          CD_Buffer *b
          )
{

  int c;
  size_t pos;
  
  
  while ( (c= fgetc ( f )) != EOF && isreturn ( c ) );
  if ( c == EOF ) return 0;
  
  pos= 1;
  b->v[0]= (char) ((unsigned char) c);
  while ( (c= fgetc ( f )) != EOF && !isreturn ( c ) )
    {
      if ( pos >= b->s ) resize_buffer ( b );
      b->v[pos++]= (char) ((unsigned char) c);
    }
  if ( pos >= b->s ) resize_buffer ( b );
  b->v[pos]= '\0';
  
  return 1;
  
} // end CD_gline


CD_Position
CD_get_position (
                 const size_t sec_ind
                 )
{

  CD_Position ret;
  int mm,ss,sec,tmp;
  
  
  // Obté minuts, segons i sectors
  mm= sec_ind/(60*75);
  tmp= sec_ind%(60*75);
  ss= tmp/75;
  sec= tmp%75;

  // Passa a BCD.
  ret.mm= BCD ( mm );
  ret.ss= BCD ( ss );
  ret.sec= BCD ( sec );
  
  return ret;
  
} // end CD_get_position
