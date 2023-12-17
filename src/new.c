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
 *  new.c - Implementa 'CD_disc_new'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CD.h"
#include "utils.h"

#include "cue.h"
#include "iso.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static const char *
get_ext (
         const char *fn
         )
{

  static char buf[4];
  
  size_t length;
  char c;
  

  length= strlen ( fn );
  if ( length <= 4 || fn[length-4] != '.' )
    buf[0]= '\0';
  else
    {
      c= fn[length-3]; if ( c>='a' && c<='z' ) c+= 'A'-'a'; buf[0]= c;
      c= fn[length-2]; if ( c>='a' && c<='z' ) c+= 'A'-'a'; buf[1]= c;
      c= fn[length-1]; if ( c>='a' && c<='z' ) c+= 'A'-'a'; buf[2]= c;
      buf[3]= '\0';
    }
  
  return &(buf[0]);
  
} // end get_ext




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

CD_Disc *
CD_disc_new (
             const char  *fn,
             char       **err
             )
{

  const char *ext;


  ext= get_ext ( fn );
  if ( !strcmp ( ext, "CUE" ) ) return CD_cue_disc_new ( fn, err );
  else if ( !strcmp ( ext, "ISO" ) ) return CD_iso_disc_new ( fn, err );
  else
    {
      CD_msgerror ( err, "unknown extension" );
      return NULL;
    }
  
} // end CD_disc_new
