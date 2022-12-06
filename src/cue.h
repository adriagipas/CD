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
 *  cue.h - Format CUE/BIN.
 *
 */
/*
 * NOTA!!!! El format CUE en realitat és un format de comandaments amb
 * estat global. És a dir, per exemple, el comandamente FILE vol dir
 * que tot els comandaments següents faran referència al fitxer
 * anterior. Ara bé, en aquest wrapper vaig a implementar un subformat
 * molt senzill on sols hi haurà un FILE seguit de TRACKS.
 */
/*
 * NOTA!! Partint de la informació en NOCASH, el document ECMA i estos
 * paràgrafs trobats per internet:
 *
 *   The lead-in of a CD is typically 2-3 minutes long. Inside this
 *   area, the TOC repeats itself over and over for the full duration
 *   of the lead-in. The TOC tells where all index 1 points of all
 *   tracks are. It also tells where lead-out starts and if there
 *   might be another session on the CD.
 *
 *   The first thing after lead-in is the index 0 point (pause) of the
 *   first track. According to CD spec, this must be no less than 2
 *   seconds long. There is no valid user data in the first 2 seconds
 *   of pause. Tracks may or may not have a pause (except the first
 *   track as mentioned above). Tracks other than the first track may
 *   contain valid user data in their pause (even the first 2
 *   seconds). One exception to this is when the disc is written as
 *   track at once (TAO). In TAO, every track must contain at least 2
 *   seconds of pause that can not contain valid user data. After the
 *   first 2 seconds of pause of TAO discs, there can be valid user
 *   data in the pause.
 *
 * he aplegat a la conclusió que LEAD-IN i LEAD-OUT són dos tracks
 * especials (el LEAD-IN seria el 0) que normalment no es registren en
 * les imatges. Per tant en el format CUE/BIN s'ha perdut tota
 * informació sobre el LEAD-IN i el LEAD-OUT, incloent quants sectors
 * tenien cadascun. Respecte als 2 primers segons buits en el primer
 * TRACK, això es un PREGAP que, mirant en la documentació ECMA, bé
 * obligat.
 */


#ifndef __CD_CUE_H__
#define __CD_CUE_H__

#include "CD.h"

// Torna NULL en cas d'error
CD_Disc *
CD_cue_disc_new (
        	 const char  *fn,
        	 char       **err // Pot ser NULL
        	 );

#endif // __CD_CUE_H__
