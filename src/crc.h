/*
 * Copyright 2020-2022 Adrià Giménez Pastor.
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
 *  crc.h - Funcions per a calcular CRC.
 *
 */

#ifndef __CD_CRC_H__
#define __CD_CRC_H__

#include <stdint.h>

#include "CD.h"

// Calcula el "CRC-16-CCITT error detection code" del subcanal Q.
uint16_t
CD_crc_subq_calc (
                  const uint8_t subq[CD_SUBCH_SIZE]
                  );

#endif // __CD_CRC_H__

