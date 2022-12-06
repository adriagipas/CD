/*
 * Copyright 2018-2022 Adrià Giménez Pastor.
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
 *  utils.h - Utilitats per a CD.
 *
 */

#ifndef __CD_UTILS_H__
#define __CD_UTILS_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ERROR */

void
CD_msgerror (
             char       **err,
             const char  *format,
             ...
             );


/* MEM */

void * CD_mem_alloc__ ( size_t nbytes );
void * CD_mem_realloc__ ( void * ptr, size_t nbytes );

#define mem_alloc(type,size)                            \
  ((type *) CD_mem_alloc__ ( sizeof(type) * (size) ))

#define mem_realloc(type,ptr,size)                              \
  ((type *) CD_mem_realloc__ ( ptr, sizeof(type) * (size) ))


/* BUFFER */

typedef struct
{
  char   *v;
  size_t  s;
} CD_Buffer;

CD_Buffer *
CD_buffer_new (void);

void
CD_buffer_free (
        	CD_Buffer *buf
        	);

// Torna 0 si no s'ha pogut llegir una línea.
int
CD_gline (
          FILE      *f,
          CD_Buffer *b
          );

#endif // __CD_UTILS_H__
