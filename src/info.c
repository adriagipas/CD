/*
 * Copyright 2017-2022 Adrià Giménez Pastor.
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
 *  info.c - Sobre CD_Info.
 *
 */


#include <stddef.h>
#include <stdlib.h>

#include "CD.h"




void
CD_info_free (
              CD_Info *info
              )
{

  free ( info->_mem_sessions );
  free ( info->_mem_tracks );
  free ( info->_mem_indexes );
  free ( info );
  
} // end CD_info_free
