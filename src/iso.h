/*
 * Copyright 2023 Adrià Giménez Pastor.
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
 *  iso.h - Imatge d'un volum ISO9660.
 *
 */


#ifndef __CD_ISO_H__
#define __CD_ISO_H__

#include "CD.h"

// Torna NULL en cas d'error
CD_Disc *
CD_iso_disc_new (
                 const char  *fn,
                 char       **err // Pot ser NULL
                 );

#endif // __CD_ISO_H__
