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
 *  CD.h - Wrapper per a llegir imatges de CD-Rom.
 *
 */

#ifndef __CD_H__
#define __CD_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define CD_SEC_SIZE 0x930
#define CD_SUBCH_SIZE 13 /* En realitat 12.15 (98 bits)*/

// És un tipus global, igual cal definir més tipos.
typedef enum
  {
    CD_DISK_TYPE_AUDIO,
    CD_DISK_TYPE_MODE1,
    CD_DISK_TYPE_MODE1_AUDIO,
    CD_DISK_TYPE_MODE2,
    CD_DISK_TYPE_MODE2_AUDIO,
    CD_DISK_TYPE_UNK
  } CD_DiskType;

// Poisició dins del disc.
typedef struct
{

  uint8_t mm; // Minutes BCD, 74, (00h..73h)
  uint8_t ss; // Seconds BCD, 60, (00h..59h)
  uint8_t sec; // Sector BCD, 75, (00h..74h)
  
} CD_Position;

// Informació índex.
typedef struct
{

  uint8_t     id; // Identificador en BCD 99 (01h..99h) Pot existir una 0.
  CD_Position pos;
  
} CD_IndexInfo;

// Informació track.
typedef struct
{
  
  uint8_t       id; // Identificador en BCD 99 (01h..99h)
  int           nindexes;
  CD_IndexInfo *indexes;
  CD_Position   pos_last_sector; // Posició absoluta de l'últim sector
        			 // del track.
  
} CD_TrackInfo;

// Informació sessió.
typedef struct
{
  
  int           ntracks;
  CD_TrackInfo *tracks;
  
} CD_SessionInfo;

// Informació del disc.
typedef struct
{

  int             nsessions;
  CD_SessionInfo *sessions;
  int             ntracks; // Totals (Lead-in i Lead-outs no inclosos)
  CD_TrackInfo   *tracks; // Totes

  // Metadata.
  CD_DiskType     type;
  
  // Private
  CD_SessionInfo *_mem_sessions;
  CD_TrackInfo   *_mem_tracks;
  CD_IndexInfo   *_mem_indexes;
  
} CD_Info;

// Allibera la memòria de CD_Info.
void
CD_info_free (
              CD_Info *info
              );

// Estructura principal
typedef struct CD_Disc_ CD_Disc;

typedef struct
{
  void (*free) (CD_Disc *);
  bool (*move_to_session) (CD_Disc *,const int); // 1 .. ??
  bool (*move_to_track) (CD_Disc *,const int); // 1 .. 99 ??
  void (*reset) (CD_Disc *);
  bool (*seek) (CD_Disc *,int amm,int ass,int asect);
  int  (*get_num_sessions) (CD_Disc *);
  bool (*read) (CD_Disc *,uint8_t buf[CD_SEC_SIZE],bool *audio,const bool move);
  bool (*read_q) (CD_Disc *,uint8_t buf[CD_SUBCH_SIZE],
                  bool *crc_ok,const bool move);
  CD_Info * (*get_info) (CD_Disc *);
  int (*get_current_session) (CD_Disc *);
  int (*get_current_track) (CD_Disc *);
  uint8_t (*get_current_index) (CD_Disc *);
  bool (*move_to_leadin) (CD_Disc *);
  CD_Position (*tell) (CD_Disc *);
} CD_Disc_Meths;

#define CD_DISC_CLS CD_Disc_Meths _m;

struct CD_Disc_
{
  CD_DISC_CLS;
};

// Crea un nou disc a partir d'una imatge en disc. El format de la
// imatge es dedueix a partir de l'extensió. Torna null en cas d'error.
CD_Disc *
CD_disc_new (
             const char  *fn,
             char       **err
             );

// Allibera la memòria.
#define CD_disc_free(DISC) ((DISC)->_m.free ( (DISC) ))

// Mou la posició de lectura al principi de la SESS indicat
// (1..?). Torna cert si s'ha pogut moure sense cap problema, o fals
// en cas d'error (no existeix la sessió). Internament llig el TOC.
#define CD_disc_move_to_session(DISC,SESS)        \
  ((DISC)->_m.move_to_session ( (DISC), (SESS) ))

// Mou la posició de lectura al principi del TRACK indicat (1..99)
// (índex global) (No és BCD!!!). Torna cert si s'ha pogut moure sense
// cap problema, o fals en cas d'error (no existeix la track).
#define CD_disc_move_to_track(DISC,TRACK)        \
  ((DISC)->_m.move_to_track ( (DISC), (TRACK) ))

// Fica el disc en l'estat inicial, com si haguerem reinsertat el disc
// en la unitat.
#define CD_disc_reset(DISC) ((DISC)->_m.reset ( (DISC) ))

// Fa un seek a la posició indicada (Minut.Segon.Sector). Torna true
// si tot ha anat bé.
#define CD_disc_seek(DISC,AMM,ASS,ASECT)        	\
  ((DISC)->_m.seek ( (DISC), (AMM), (ASS), (ASECT) ))

#define CD_disc_get_num_sessions(DISC)        	\
  ((DISC)->_m.get_num_sessions ( (DISC) ))

// Llig en BUF[CD_SEC_SIZE] el sector actual i avança al següent
// sector si MOVE. En audio torna CERT si es tracta d'un sector
// d'audio. Torna cert si tot ha anat bé, false en cas d'error.
#define CD_disc_read(DISC,BUF,AUDIO,MOVE)        	\
  ((DISC)->_m.read ( (DISC), (BUF), (AUDIO), (MOVE) ))

// Llig en BUF el subcanal Q del sector actual (98 bits, 12.25 bytes)
// i avança al següent sector si MOVE. ATENCIÓ!!! El primer byte sols
// conté els 2 primer bits, els de sincronització, d'aquesta manera la
// resta estan alineats.
#define CD_disc_read_q(DISC,BUF,PTR_CRC_OK,MOVE)        \
  ((DISC)->_m.read_q ( (DISC), (BUF), (PTR_CRC_OK), (MOVE) ))

// Retorna una estructura amb informació sobre l'estructura del
// CD. Aquesta estructura s'ha d'alliberar.
#define CD_disc_get_info(DISC)        		\
  (DISC)->_m.get_info ( (DISC) )

// Torna el número de la sessió actual. Començant per 0.
#define CD_disc_get_current_session(DISC)        \
  (DISC)->_m.get_current_session ( (DISC) )

// Torna el número (en sencer 1..99) (global) del 'track'
// actual.
#define CD_disc_get_current_track(DISC)        \
  (DISC)->_m.get_current_track ( (DISC) )

// Torna l'identificador de l'índex actual en BCD.
#define CD_disc_get_current_index(DISC)        \
  (DISC)->_m.get_current_index ( (DISC) )

// Mou la posició de lectura al principi de l'àrea 'Lead-in' de la
// sessió actual.
#define CD_disc_move_to_leadin(DISC) \
  (DISC)->_m.move_to_leadin ( (DISC) )

// Torna la posició actual.
#define CD_disc_tell(DISC) \
  (DISC)->_m.tell ( (DISC) )

#endif // __CD_H__
