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
 *  iso.c - Implementació de 'iso.h'.
 */


#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CD.h"
#include "crc.h"
#include "iso.h"
#include "utils.h"




/**********/
/* MACROS */
/**********/

#define SEC_SIZE 2048

#define IGAP (2*75)

#define BCD(NUM) ((uint8_t) (((NUM)/10)*0x10 + (NUM)%10))




/*********/
/* TIPUS */
/*********/

typedef struct
{

  CD_DISC_CLS;

  FILE   *f; // Fitxer
  size_t  num_secs; // Nombre de sectors (No inclou el IGAP)
  size_t  current_sec;
  
} CD_ISO_Disc;

#define ISO(DISC) ((CD_ISO_Disc *) (DISC))




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// Torna cert si tot ha anat bé.
static bool
read_iso (
          const char   *fn,
          CD_ISO_Disc  *d,
          char        **err
          )
{

  long size;

  
  // Obri.
  d->f= fopen ( fn, "rb" );
  if ( d->f == NULL )
    {
      CD_msgerror ( err, "cannot open '%s'", fn );
      return false;
    }
  if ( fseek ( d->f, 0, SEEK_END ) == -1 )
    {
      CD_msgerror ( err, "unable to load '%s'", fn );
      return false;
    }
  size= ftell ( d->f );
  if ( size == -1 )
    {
      CD_msgerror ( err, "unable to load '%s'", fn );
      return false;
    }
  if ( size%SEC_SIZE != 0 )
    {
      CD_msgerror ( err, "unable to load '%s': invalid size"
                    " (%ld) for a ISO file", fn, size );
      return false;
    }
  d->num_secs= size/SEC_SIZE;
  
  return true;

} // end read_iso


// Força un seek en el fitxer, torna fals en cas d'error.
static bool
force_seek (
            CD_ISO_Disc *d
            )
{

  long offset,coffset;

  
  if ( d->current_sec >= (d->num_secs+IGAP) ) return true;
  if ( d->current_sec < IGAP ) return true;
  offset= ((long) (d->current_sec-IGAP))*((long) SEC_SIZE);
  coffset= ftell ( d->f );
  if ( offset != coffset &&
       fseek ( d->f, offset, SEEK_SET ) != 0 )
    return false;

  return true;
  
} // end force_seek




/***********/
/* MÈTODES */
/***********/

static void
free_ (
       CD_Disc *d
       )
{

  if ( ISO(d)->f != NULL ) fclose ( ISO(d)->f );
  free ( d );
  
} // end free_


static bool
move_to_session (
                 CD_Disc   *d,
                 const int  sess
                 )
{

  if ( sess != 1 ) return false;

  ISO(d)->current_sec= IGAP; // Primer sector amb contingut primer track.
  
  return true;
  
} // end move_to_session


static bool
move_to_track (
               CD_Disc   *d,
               const int  track
               )
{

  // NOTA!! Busque el primer no gap del track.

  if ( track != 1 ) return false;
  
  ISO(d)->current_sec= IGAP; // Primer sector amb contingut primer track.
  
  return true;
  
} // end move_to_track


static void
reset (
       CD_Disc *d
       )
{
  ISO(d)->current_sec= 0;
} // end reset


static bool
seek (
      CD_Disc *d,
      int      amm,
      int      ass,
      int      asect
      )
{

  size_t pos;
  
  
  pos= amm*60*75 + ass*75 + asect;
  if ( pos < 0 || pos >= (ISO(d)->num_secs+IGAP) ) return false;

  ISO(d)->current_sec= pos;
  if ( !force_seek ( ISO(d) ) )
    return false;
  
  return true;
  
} // end seek


static int
get_num_sessions (
                  CD_Disc *d
                  )
{
  return 1;
} // end get_num_sessions


static bool
read (
      CD_Disc    *d,
      uint8_t     buf[CD_SEC_SIZE],
      bool       *audio,
      const bool  move
      )
{

  int i,mm,ss,sec;
  size_t tmp;
  
  
  if ( ISO(d)->current_sec >= (ISO(d)->num_secs+IGAP) ) return false;

  // Intenta llegir.
  *audio= false;
  if ( ISO(d)->current_sec <= IGAP )
    memset ( buf, 0, CD_SEC_SIZE ); // TODO?!!!
  else
    {

      // Llig dades.
      if ( !force_seek ( ISO(d) ) ) return false;
      if ( fread ( &buf[16], SEC_SIZE, 1, ISO(d)->f ) != 1 )
        return false;

      // Emule sectors MODE 01 i Header (La resta de camps els deixe a 0)
      // --> Sync
      buf[0]= 0x00;
      for ( i= 1; i < 11; i++ ) buf[i]= 0xff;
      buf[11]= 0x00;
      // --> Header
      mm= (ISO(d)->current_sec)/(60*75);
      tmp= (ISO(d)->current_sec)%(60*75);
      ss= tmp/75;
      sec= tmp%75;
      buf[12]= BCD(mm);
      buf[13]= BCD(ss);
      buf[14]= BCD(sec);
      buf[15]= 0x01; // Mode 01
      // --> EDC, Intermediate, P-Parity, Q-Parity (TODO??!!!)
      for ( i= 2064; i < CD_SEC_SIZE; i++ ) buf[i]= 0x00;
      
    }
  if ( move ) ++(ISO(d)->current_sec);
  
  return true;
  
} // end read


static bool
read_q (
        CD_Disc    *d,
        uint8_t     buf[CD_SUBCH_SIZE],
        bool       *crc_ok,
        const bool  move
        )
{
  
  size_t tmp;
  uint16_t crc;
  
  
  // NOTA!! No recorde què fea açò, ho he copiat del codi CUE.
  
  // CRC ok!!!
  *crc_ok= true;
  
  if ( ISO(d)->current_sec >= (ISO(d)->num_secs+IGAP) ) return false;

  // En el primer byte fiquem els 2 bits de "Sub-channel
  // synchronization field".
  buf[0]= 0x00; // ¿¿????
  
  // ADR/Control
  // --> Assumisc ADR 1 in Data region
  buf[1]= 0x41;
  
  // Track and index.
  buf[2]= BCD ( 1 );
  buf[3]= ISO(d)->current_sec>=IGAP ? 0x01 : 0x00;
  
  // Track relative MSF address
  if ( ISO(d)->current_sec >= IGAP )
    tmp= ISO(d)->current_sec - IGAP;
  else // Distància a track->sector_index01 (estem en un pregap)
       // El -1 està copiat de mednafen.
    tmp= IGAP - 1 - ISO(d)->current_sec;
  buf[4]= BCD ( tmp/(60*75) ); tmp%= 60*75; // MM
  buf[5]= BCD ( tmp/75 ); tmp%= 75; // SS
  buf[6]= BCD ( tmp ); // FF (encara que serien sectors no
                       // frames... Confusió en la terminologia)
  
  // Reserved
  buf[7]= 0x00;
  
  // Absolute MSF address
  tmp= ISO(d)->current_sec;
  buf[8]= BCD ( tmp/(60*75) ); tmp%= 60*75; // MM
  buf[9]= BCD ( tmp/75 ); tmp%= 75; // SS
  buf[10]= BCD ( tmp ); // FF
      
  // CRC-16-CCITT error detection code (big-endian: bytes ordered MSB,
  // LSB)
  crc= CD_crc_subq_calc ( buf );
  buf[11]= (crc>>8)&0xFF;
  buf[12]= crc&0xFF;
  
  // Mou.
  if ( move ) ++(ISO(d)->current_sec);
  
  return true;
  
} // end read_q


static CD_Info *
get_info (
          CD_Disc *d
          )
{
  
  CD_Info *ret;
  CD_SessionInfo *sess;
  CD_TrackInfo *tracks;
  CD_IndexInfo *indexes;
  
  
  // Reserva memòria.
  ret= mem_alloc ( CD_Info, 1 );
  ret->_mem_sessions= sess= mem_alloc ( CD_SessionInfo, 1 );
  ret->_mem_tracks= tracks= mem_alloc ( CD_TrackInfo, 1 );
  ret->_mem_indexes= indexes= mem_alloc ( CD_IndexInfo, 2 );
  
  // Sesions.
  ret->nsessions= 1;
  ret->sessions= sess;
  sess[0].ntracks= 1;
  sess[0].tracks= tracks;

  // Tracks i entries.
  ret->ntracks= 1;
  ret->tracks= tracks;
  tracks[0].id= BCD(1);
  tracks[0].nindexes= 2;
  tracks[0].indexes= indexes;
  indexes[0].id= BCD(0);
  indexes[0].pos= CD_get_position ( 0 );
  indexes[0].id= BCD(1);
  indexes[0].pos= CD_get_position ( IGAP );
  tracks[0].pos_last_sector= CD_get_position ( (ISO(d)->num_secs+IGAP) - 1 );

  // Tipus.
  ret->type= CD_DISK_TYPE_MODE1;
  
  return ret;

} // end get_info


static int
get_current_session (
                     CD_Disc *d
                     )
{
  return 0;
} // end get_current_session


static int
get_current_track (
                   CD_Disc *d
                   )
{
  return 1;
} // end get_current_track


static uint8_t
get_current_index (
                   CD_Disc *d
                   )
{
  return
    (ISO(d)->current_sec>=(ISO(d)->num_secs+IGAP) || ISO(d)->current_sec<IGAP) ?
    0x00 : 0x01;
} // end get_current_index


static bool
move_to_leadin (
                CD_Disc *d
                )
{

  fprintf ( stderr, "[WW] lead-in not available in ISO format\n" );
  ISO(d)->current_sec= 0;
  
  return true;
  
} // end move_to_leadin


static CD_Position
tell (
      CD_Disc *d
      )
{
  return CD_get_position ( ISO(d)->current_sec );
} // end tell




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

CD_Disc *
CD_iso_disc_new (
                 const char  *fn,
                 char       **err
                 )
{

  CD_ISO_Disc *new;


  new= mem_alloc ( CD_ISO_Disc, 1 );
  new->f= NULL;
  new->num_secs= 0;
  new->current_sec= IGAP; // Primera posició amb contingut.
  new->_m.free= free_;
  new->_m.move_to_session= move_to_session;
  new->_m.move_to_track= move_to_track;
  new->_m.reset= reset;
  new->_m.seek= seek;
  new->_m.get_num_sessions= get_num_sessions;
  new->_m.read= read;
  new->_m.read_q= read_q;
  new->_m.get_info= get_info;
  new->_m.get_current_session= get_current_session;
  new->_m.get_current_track= get_current_track;
  new->_m.get_current_index= get_current_index;
  new->_m.move_to_leadin= move_to_leadin;
  new->_m.tell= tell;
  
  // Llig.
  if ( !read_iso ( fn, new, err ) )
    goto error;

  return (CD_Disc *) new;

 error:
  CD_disc_free ( (CD_Disc *) new );
  return NULL;
  
} // end CD_iso_disc_new
