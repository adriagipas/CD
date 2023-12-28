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
 *  cue.c - Implementació de 'cue.h'.
 *
 */


#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CD.h"
#include "cue.h"
#include "crc.h"
#include "utils.h"




/**********/
/* MACROS */
/**********/

#define SEC_SIZE 0x930

#define IGAP (2*75)

#define BCD(NUM) ((uint8_t) (((NUM)/10)*0x10 + (NUM)%10))

#define LSD_ENTRY_SIZE 15




/*********/
/* TIPUS */
/*********/

typedef struct bin_file bin_file_t;
struct bin_file
{
  
  FILE       *f;
  size_t      bin_size; // En número de sectors.
  size_t      asize; // Número de sectors acumulats de fitxers
                     // anteriors sense incloure l'actual.
  bin_file_t *next;
  
};

typedef struct
{

  enum {
    AUDIO,
    MODE1,
    MODE2
  }   type; // Tipus de track.
  int    p; // Posició de la primera entrada en entries
  int    N; // Número d'entrades
  size_t sector_index01; // Primer sector de l'índex 01.
  
} track_t;

typedef struct
{

  enum {
    PREGAP,
    INDEX
  }      type;
  int    id; // Identificador.
  size_t time; // Posició del primer sector, en un pregap és el número
               // de segments a ignorar. Aquest camp es reaprofita i
               // acaba siguent el offset de cada entrada en valor
               // absolut.
  const bin_file_t *file; // Fitxer on està desat esta entrada.
} entry_t;

typedef struct
{

  long              offset;
  int               track_id;
  uint8_t           index_id;
  const bin_file_t *file;
  int               subq_ptr; // Punter a la taula q_values.
  
} sec_map_t;

typedef struct
{

  CD_DISC_CLS;

  // Binary
  bin_file_t *files;
  
  // Cue
  track_t *tracks;
  size_t   NT;
  entry_t *entries;
  size_t   NE;

  // Mapa sectors
  sec_map_t *maps;
  size_t     N;

  // Posició actual.
  size_t current_sec;

  // Sectors subcanal_q erronis.
  // El format és literalment el del fitxer LSD:
  // 3B MIN SEC FRA || 12B QSUb (inclou CRC)
  uint8_t **subq;
  
} CD_CUE_Disc;

#define CUE(DISC) ((CD_CUE_Disc *) (DISC))




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static bool
try_open_binary (
        	 CD_CUE_Disc *d,
        	 const char  *fn
        	 )
{

  long size;
  bin_file_t *f;
  

  // Prepara.
  f= mem_alloc ( bin_file_t, 1 );
  f->f= NULL;
  
  // Try open.
  f->f= fopen ( fn, "rb" );
  if ( f->f == NULL ) goto error;
  
  // Check size.
  if ( fseek ( f->f, 0, SEEK_END ) == -1 ) goto error;
  size= ftell ( f->f );
  if ( size == -1 || size%SEC_SIZE ) goto error;
  
  // Return.
  f->bin_size= (size_t) (size/SEC_SIZE);
  f->next= d->files;
  d->files= f;
  f->asize= f->next!=NULL ? f->next->asize + f->next->bin_size : 0;
  
  return true;

 error:
  if ( f->f != NULL ) fclose ( f->f );
  free ( f );
  return false;
  
} // end try_open_binary


static bool
open_binary (
             CD_CUE_Disc  *d,
             const char   *binfn,
             const char   *cuefn,
             char        **err
             )
{

  char *aux;
  int endpos,i;
  bool ok;

  
  // Default file.
  if ( try_open_binary ( d, binfn ) ) return true;

  // Based on cue PATH.
  endpos= strlen ( cuefn ) - 1;
  aux= mem_alloc ( char, strlen(binfn)+strlen(cuefn)+1 );
  for ( ; endpos>=0 && cuefn[endpos]!='/' && cuefn[endpos]!='\\'; --endpos );
  for ( i= 0; i <= endpos; ++i ) aux[i]= cuefn[i];
  aux[i]= '\0';
  strcat ( aux, binfn );
  ok= try_open_binary ( d, aux );
  free ( aux );
  if ( ok ) return true;
  
  // Error.
  CD_msgerror ( err, "binary file '%s' not found or wrong size", binfn );
  
  return false;
  
} // end open_binary


// Torna 0 si tot ha anat bé, -1 si EOF, 1 en cas d'error.
static int
read_file (
            CD_CUE_Disc  *d,
            char         *tok,
            const char   *cuefn,
            char        **err
            )
{

  char *fname,*binfn,*aux;
  
  
  // Prepare.
  fname= NULL;
  
  // Get file name.
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' || *tok != '"' ) goto error_format;
  binfn= ++tok;
  for ( ; *tok && *tok!='"'; ++tok ); // Find end
  if ( *tok != '"' ) goto error_format;
  *(tok++)= '\0';
  fname= mem_alloc ( char, strlen(binfn)+1 );
  strcpy ( fname, binfn );

  // Check binary.
  if ( !isspace (*tok) ) goto error_format;
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  aux= tok;
  for ( ; *tok && !isspace(*tok); ++tok ); // Find end
  *tok= '\0';
  if ( strcmp ( aux, "BINARY" ) ) goto error_format;

  // Obri el fitxer.
  if ( !open_binary ( d, fname, cuefn, err ) )
    goto error;

  free ( fname );
    
  return 0;
  
 error_format:
  CD_msgerror ( err, "wrong file command format" );
 error:
  if ( fname != NULL ) free ( fname );
  return 1;
  
} // end read_file


// Torna 0 si tot ha anat bé, -1 si EOF, 1 en cas d'error.
static int
read_track (
            CD_CUE_Disc  *d,
            char         *tok,
            size_t       *st,
            char        **err
            )
{

  int track_id;
  char *mode;
  
  
  // Get track id.
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  if ( tok[0] < '0' || tok[0] > '9' ||
       tok[1] < '0' || tok[1] > '9' ||
       !isspace(tok[2]) )
    goto error_format;
  tok[2]= '\0';
  track_id= atoi ( tok );
  if ( (size_t) track_id != d->NT+1 )
    {
      CD_msgerror ( err, "expecting track %d, instead track %d was read",
        	    d->NT+1, track_id );
      goto error;
    }
  if ( *st == d->NT )
    {
      (*st)*= 2;
      d->tracks= mem_realloc ( track_t, d->tracks, *st );
    }
  d->tracks[d->NT].p= d->NE;
  d->tracks[d->NT].N= 0;
  tok+= 3;

  // Get mode
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  mode= tok;
  for ( ; *tok && !isspace(*tok); ++tok ); // Find end
  *tok= '\0';
  if ( !strcmp ( mode, "AUDIO" ) ) d->tracks[d->NT].type= AUDIO;
  else if ( !strcmp ( mode, "MODE1/2352" ) ) d->tracks[d->NT].type= MODE1;
  else if ( !strcmp ( mode, "MODE2/2352" ) ) d->tracks[d->NT].type= MODE2;
  else
    {
      CD_msgerror ( err, "TRACK format unknown: %s", mode );
      goto error;
    }
  ++(d->NT);
  
  return 0;
  
 error_format:
  CD_msgerror ( err, "wrong track format" );
 error:
  return 1;
  
} // end read_track


// Torna (size_t) -1 en cas d'error.
static size_t
process_time (
              char               *str,
              const CD_CUE_Disc  *d,
              char              **err
              )
{

  size_t ret;

  
  if ( strlen ( str ) != 8 ||
       str[0] < '0' || str[0] > '9' ||
       str[1] < '0' || str[1] > '9' ||
       str[2] != ':' ||
       str[3] < '0' || str[3] > '9' ||
       str[4] < '0' || str[4] > '9' ||
       str[5] != ':' ||
       str[6] < '0' || str[6] > '9' ||
       str[7] < '0' || str[7] > '9' )
    {
      CD_msgerror ( err, "wrong time format: %s", str );
      return (size_t) -1;
    }

  // Transform.
  ret= 0;
  str[2]= '\0';
  ret+= atoi ( str )*60*75; // minuts
  str+= 3;
  str[2]= '\0';
  ret+= atoi ( str )*75; // segons
  str+= 3;
  str[2]= '\0';
  ret+= atoi ( str ); // sectors

  return ret;
  
} // end process_time


// Torna 0 si tot ha anat bé, -1 si EOF, 1 en cas d'error.
static int
read_index (
            CD_CUE_Disc  *d,
            char         *tok,
            size_t       *se,
            char        **err
            )
{

  int index_id;
  char *time;
  track_t *track;
  size_t time_val;
  

  // Comprova que tenim un fitxer associat.
  if ( d->files == NULL ) goto error_file;
  
  // Current track.
  track= &(d->tracks[d->NT-1]);
  
  // Get index id.
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  if ( tok[0] < '0' || tok[0] > '9' ||
       tok[1] < '0' || tok[1] > '9' ||
       !isspace(tok[2]) )
    goto error_format;
  tok[2]= '\0';
  index_id= atoi ( tok );
  if ( index_id < 0 )
    {
      CD_msgerror ( err, "wrong index identifier %d", index_id );
      goto error;
    }
  if ( *se == d->NE )
    {
      (*se)*= 2;
      d->entries= mem_realloc ( entry_t, d->entries, *se );
    }
  d->entries[d->NE].type= INDEX;
  d->entries[d->NE].id= index_id;
  d->entries[d->NE].file= d->files;
  tok+= 3;

  // Get time
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  time= tok;
  for ( ; *tok && !isspace(*tok); ++tok ); // Find end
  *tok= '\0';
  time_val= process_time ( time, d, err );
  if ( time_val == (size_t) -1 ) return 1; // ERR
  d->entries[d->NE++].time= time_val;
  ++(track->N);
  
  return 0;

 error_file:
  CD_msgerror ( err, "index defined before specifying a file" );
  goto error;
 error_format:
  CD_msgerror ( err, "wrong index format" );
 error:
  return 1;
  
} // end read_index


// Torna 0 si tot ha anat bé, -1 si EOF, 1 en cas d'error.
static int
read_pregap (
             CD_CUE_Disc  *d,
             char         *tok,
             size_t       *se,
             char        **err
             )
{

  char *time;
  track_t *track;
  size_t time_val;
  

  // Current track.
  track= &(d->tracks[d->NT-1]);
  
  // Init.
  if ( *se == d->NE )
    {
      (*se)*= 2;
      d->entries= mem_realloc ( entry_t, d->entries, *se );
    }
  d->entries[d->NE].type= PREGAP;
  d->entries[d->NE].id= -1;
  d->entries[d->NE].file= NULL;
  
  // Get time
  for ( ; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' ) goto error_format;
  time= tok;
  for ( ; *tok && !isspace(*tok); ++tok ); // Find end
  *tok= '\0';
  time_val= process_time ( time, d, err );
  if ( time_val == (size_t) -1 ) return 1; // ERR
  d->entries[d->NE++].time= time_val;
  ++(track->N);
  
  return 0;
  
 error_format:
  CD_msgerror ( err, "wrong pregap format" );
  return 1;
  
} // end read_pregap


// Torna 0 si tot ha anat bé, -1 si EOF, 1 en cas d'error.
static int
read_command (
              FILE         *f,
              CD_CUE_Disc  *d,
              size_t       *st,
              size_t       *se,
              CD_Buffer    *buf,
              const char   *cuefn,
              char        **err
              )
{

  int ok;
  char *tok;
  
  
  // Get first not empty line.
  while ( (ok= CD_gline ( f, buf )) && buf->v[0]=='\0' );
  if ( !ok ) return -1; // EOF
  
  // Find command.
  for ( tok= buf->v; *tok && isspace(*tok); ++tok ); // Skip spaces
  if ( *tok == '\0' )
    {
      CD_msgerror ( err, "wrong command format: empty lime" );
      return 1; // ERR
    }
  
  // Process command.
  if ( !strncmp ( tok, "FILE ", 5 ) )
    {
      tok+= 5;
      return read_file ( d, tok, cuefn, err );
    }
  if ( !strncmp ( tok, "TRACK ", 6 ) )
    {
      tok+= 6;
      return read_track ( d, tok, st, err );
    }
  else if ( !strncmp ( tok, "INDEX ", 6 ) )
    {
      tok+= 6;
      return read_index ( d, tok, se, err );
    }
  else if ( !strncmp ( tok, "PREGAP ", 7 ) )
    {
      tok+= 7;
      return read_pregap ( d, tok, se, err );
    }
  else
    {
      CD_msgerror ( err, "unknown command: %s", tok );
      return 1; // ERR
    }
  
} // end read_command


static bool
read_content (
              FILE         *f,
              CD_CUE_Disc  *d,
              CD_Buffer    *buf,
              const char   *cuefn,
              char        **err
              )
{

  size_t st,se;
  int ret;
  

  // Inicialitza.
  d->tracks= mem_alloc ( track_t, 1 );
  d->NT= 0; st= 1;
  d->entries= mem_alloc ( entry_t, 1 );
  d->NE= 0; se= 1;

  // Llig comandaments.
  while ( (ret= read_command ( f, d, &st, &se, buf, cuefn, err )) == 0 );
  if ( ret != -1 ) return false;

  /*
  // Print.
  for ( size_t t= 0; t < d->NT; ++t )
    {
      printf ( "TRACK ");
      switch ( d->tracks[t].type )
        {
        case AUDIO: printf ( "AUDIO\n" ); break;
        case MODE1: printf ( "MODE1\n" ); break;
        case MODE2: printf ( "MODE2\n" ); break;
        }
      for ( size_t e= d->tracks[t].p; e != d->tracks[t].p+d->tracks[t].N; ++e )
        {
          switch ( d->entries[e].type )
            {
            case INDEX:
              printf ( "  INDEX %d %ld\n",
        	       d->entries[e].id, d->entries[e].time );
              break;
            case PREGAP:
              printf ( "  PREGAP %ld\n", d->entries[e].time );
              break;
            }
        }
    }
  */
  
  // Realloc.
  if ( st != d->NT && d->NT>0 )
    d->tracks= mem_realloc ( track_t, d->tracks, d->NT );
  if ( se != d->NE && d->NE>0 )
    d->entries= mem_realloc ( entry_t, d->entries, d->NE );
  
  return true;
  
} // end read_content


// Torna cert si tot ha anat bé.
static bool
read_cue (
          const char   *fn,
          CD_CUE_Disc  *d,
          char        **err
          )
{

  FILE *f;
  CD_Buffer *buf;
  char *binfn;
  
  
  // Inicialitza.
  f= NULL;
  binfn=NULL;
  buf= CD_buffer_new ();
  
  // Obri el fitxer.
  f= fopen ( fn, "r" );
  if ( f == NULL )
    {
      CD_msgerror ( err, "cannot open '%s'", fn );
      goto error;
    }
  
  // Llig el contingut.
  if ( !read_content ( f, d, buf, fn, err ) )
    goto error;
  
  // Tanca.
  free ( binfn );
  CD_buffer_free ( buf );
  fclose ( f );
  
  return true;
  
 error:
  if ( binfn != NULL ) free ( binfn );
  CD_buffer_free ( buf );
  if ( f != NULL ) fclose ( f );
  return false;
  
} // end read_cue


static size_t
calc_total_gap (
        	const CD_CUE_Disc *d
        	)
{

  size_t gap;
  size_t n;
  

  gap= 2*75; // 2 segons inicials.
  for ( n= 0; n < d->NE; ++n )
    if ( d->entries[n].type == PREGAP )
      gap+= d->entries[n].time;

  return gap;
  
} // end calc_total_gap


static size_t
calc_files_size (
        	const CD_CUE_Disc *d
        	)
{

  size_t ret;
  const bin_file_t *p;
  
  
  ret= 0;
  for ( p= d->files; p != NULL; p= p->next )
    ret+= p->bin_size;
  
  return ret;
  
} // end calc_files_size


static bool
check_indexes_in_range (
                        const CD_CUE_Disc  *d,
                        char              **err
                        )
{
  
  size_t n;
  
  
  for ( n= 0; n < d->NE; ++n )
    if ( d->entries[n].type == INDEX &&
         d->entries[n].time >= d->entries[n].file->bin_size )
      {
        CD_msgerror ( err, "index reference out of range %d (binary"
                      " file has %d sectors)",
                      n, d->entries[n].file->bin_size );
        return false;
      }
  
  return true;
  
} // end check_indexes_in_range


// També fixa el sector_index01 en track.
// Torna cert si tot ha anat bé.
static bool
create_map_sectors (
        	    CD_CUE_Disc  *d,
        	    char        **err
        	    )
{

  size_t gap,bin_size;
  size_t n,t,e,end;
  sec_map_t *maps;
  track_t *track;
  entry_t *entry;
  long offset;
  int prev_ind;
  const bin_file_t *prev_file;
  
  
  // Obté número total de sectors mapejats i comprova.
  gap= calc_total_gap ( d );
  bin_size= calc_files_size ( d );
  if ( !check_indexes_in_range ( d, err ) ) return false;
  
  // Reserva memòria.
  d->N= bin_size + gap;
  d->maps= maps= mem_alloc ( sec_map_t, d->N );

  // Ompli i reajusta 'entries[n].time'.
  // --> Index 00 Track1 (Pregap 2s)
  for ( n= 0; n < 2*75; ++n )
    {
      maps[n].offset= -1;
      maps[n].track_id= 0;
      maps[n].index_id= 0x00;
      maps[n].file= NULL;
      maps[n].subq_ptr= -1;
    }
  // Tracks
  gap= 2*75; prev_file= NULL;
  offset= 0; // CALLA !!!!
  for ( t= 0; t < d->NT; ++t )
    {
      track= &(d->tracks[t]);
      prev_ind= 0;
      for ( e= track->p; e != (size_t) (track->p+track->N); ++e )
        {
          entry= &(d->entries[e]);
          switch ( entry->type )
            {
            case PREGAP: // Pregap
              // Calc end
              if ( e == d->NE-1 || d->entries[e+1].type != INDEX )
                goto error;
              gap+= entry->time;
              end= d->entries[e+1].file->asize + d->entries[e+1].time + gap;
              if ( end <= n ) goto error;
              // Recalculate time and fill
              entry->time= n;
              for ( ; n != end; ++n )
        	{
        	  maps[n].offset= -1;
        	  maps[n].track_id= t;
        	  maps[n].index_id= 0x00;
                  maps[n].file= NULL;
                  maps[n].subq_ptr= -1;
        	}
              break;
            case INDEX: // Índex
              // Comprovacions d'index, el 0 és opcional
              if ( (entry->id == 0 && prev_ind != 0) ||
                   (entry->id > 0 && entry->id != prev_ind+1) )
                goto error;
              if ( entry->id != 0 ) ++prev_ind;
              // Fixa sector_index01
              if ( entry->id == 1 ) track->sector_index01= n;
              // Inicialitza offset si canvia de fitxer.
              if ( entry->file != prev_file )
                {
                  offset= 0;
                  prev_file= entry->file;
                }
              // Calc end
              if ( e == d->NE-1 ) end= d->N;
              else if ( d->entries[e+1].type == INDEX )
                end= d->entries[e+1].file->asize + d->entries[e+1].time + gap;
              else
        	{
        	  if ( e+1 == d->NE-1  || d->entries[e+2].type != INDEX )
        	    goto error;
                  // Sense contar el gap extra.
        	  end= d->entries[e+2].file->asize + d->entries[e+2].time + gap;
        	}
              if ( end <= n ) goto error;
              // Recalculate time and fill
              entry->time= n;
              for ( ; n != end; ++n, offset+= SEC_SIZE )
        	{
        	  maps[n].offset= offset;
        	  maps[n].track_id= t;
        	  maps[n].index_id= BCD ( entry->id );
                  maps[n].file= entry->file;
                  maps[n].subq_ptr= -1;
        	}
              break;
            }
        }
    }
  
  return true;

 error:
  CD_msgerror ( err, "invalid PREGAP/INDEX commands" );
  return false;
  
} // end create_map_sectors


// Força un seek en el fitxer, torna fals en cas d'error.
static bool
force_seek (
            CD_CUE_Disc *d
            )
{

  long coffset;
  sec_map_t val;

  
  if ( d->current_sec >= d->N ) return true; // No fa res i no és error.
  val= d->maps[d->current_sec];
  if ( val.offset != -1 )
    {
      coffset= ftell ( val.file->f );
      if ( coffset == -1 ) return false;
      if ( coffset != val.offset &&
           fseek ( val.file->f, val.offset, SEEK_SET ) != 0 )
        return false;
    }
  
  return true;
  
} // end try_seek


static size_t
mmssff_bcd2long (
                 const uint8_t mm,
                 const uint8_t ss,
                 const uint8_t ff
                 )
{
  return
    (10*(mm/0x10) + mm%0x10)*60*75 +
    (10*(ss/0x10) + ss%0x10)*75 +
    (10*(ff/0x10) + ff%0x10);
} // mmssff_bcd2long


// Llig el contingut d'un fitxer LSD si és possible,
// Torna fals si hi ha algun problema.
static bool
try_read_lsd (
              CD_CUE_Disc  *d,
              const char   *cuefn,
              char        **err
              )
{

  int endpos,i;
  char *fname;
  FILE *f;
  long size;
  uint8_t *mem;
  size_t sec;
  

  // Prepara.
  f= NULL;
  fname= NULL;
  
  // Get lsd file.
  // --> Si el nom del fitxer cue no acava en cue no intente llegir el lsd.
  endpos= strlen ( cuefn ) - 1;
  if ( endpos < 4 || cuefn[endpos-3]!='.' || cuefn[endpos-2]!='c' ||
       cuefn[endpos-1]!='u' || cuefn[endpos]!='e' )
    return true;
  // --> Crea el fitxer.
  fname= mem_alloc ( char, strlen(cuefn)+1 );
  strcpy ( fname, cuefn );
  fname[endpos-2]= 'l';
  fname[endpos-1]= 's';
  fname[endpos]= 'd';
  
  // Intenta llegir el fitxer.
  f= fopen ( fname, "rb" );
  if ( f == NULL ) { free ( fname ); return true; } // Ignorar si no existeix
  
  // Check size.
  if ( fseek ( f, 0, SEEK_END ) == -1 ) goto error_read;
  size= ftell ( f );
  if ( size == -1 || size%LSD_ENTRY_SIZE )
    {
      CD_msgerror ( err, "'%s' has not a valid size for a LSD size",
                    fname );
      goto error;
    }
  
  // Reserva memòria.
  d->subq= mem_alloc ( uint8_t *, size/LSD_ENTRY_SIZE );
  mem= mem_alloc ( uint8_t, size );
  for ( i= 0; i < size/LSD_ENTRY_SIZE; ++i, mem+= LSD_ENTRY_SIZE )
    d->subq[i]= mem;
  
  // Llig.
  rewind ( f );
  if ( fread ( d->subq[0], size, 1, f ) != 1 )
    goto error_read;
  
  // Assigna sectors a mapa.
  for ( i= 0; i < size/LSD_ENTRY_SIZE; ++i )
    {
      sec= mmssff_bcd2long ( d->subq[i][0], d->subq[i][1], d->subq[i][2] );
      if ( sec >= d->N )
        {
          CD_msgerror ( err, "out of range sector in LSD file: '%s'",
                        fname );
          goto error;
        }
      d->maps[sec].subq_ptr= i;
    }
  
  // Allibera.
  free ( fname );
  fclose ( f );
  
  return true;

 error_read:
  CD_msgerror ( err, "unexpected error ocurred reading '%s'", fname );
 error:
  if ( fname != NULL ) free ( fname );
  if ( f != NULL ) fclose ( f );
  return false;
  
} // end try_read_lsd




/***********/
/* MÈTODES */
/***********/

static void
free_ (
       CD_Disc *d
       )
{

  bin_file_t *p,*q;

  
  if ( CUE(d)->subq != NULL )
    {
      free ( CUE(d)->subq[0] );
      free ( CUE(d)->subq );
    }
  if ( CUE(d)->maps != NULL ) free ( CUE(d)->maps );
  if ( CUE(d)->entries != NULL ) free ( CUE(d)->entries );
  if ( CUE(d)->tracks != NULL ) free ( CUE(d)->tracks );
  p= CUE(d)->files;
  while ( p != NULL )
    {
      q= p;
      p= p->next;
      fclose ( q->f );
      free ( q );
    }
  free ( d );
  
} // end free_


static bool
move_to_session (
        	 CD_Disc   *d,
        	 const int  sess
        	 )
{

  if ( sess != 1 ) return false;

  CUE(d)->current_sec= IGAP; // Primer sector amb contingut primer track.

  return true;
  
} // end move_to_session


static bool
move_to_track (
               CD_Disc   *d,
               const int  track
               )
{

  // NOTA!! Busque el primer no gap del track.
  
  const track_t *tp;
  size_t e,end;
  
  
  if ( track < 1 || (size_t) track > CUE(d)->NT ) return false;
  
  tp= &(CUE(d)->tracks[track-1]);
  end= tp->p + tp->N;
  for ( e= tp->p; e != end; ++e )
    if ( CUE(d)->entries[e].type == INDEX )
      break;
  if ( e == end ) return false;
  CUE(d)->current_sec= CUE(d)->entries[e].time;

  return true;
  
} // end move_to_track


static void
reset (
       CD_Disc *d
       )
{
  CUE(d)->current_sec= 0;
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
  if ( pos < 0 || pos >= CUE(d)->N ) return false;

  CUE(d)->current_sec= pos;
  force_seek ( CUE(d) );
  
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

  sec_map_t val;
  
  
  if ( CUE(d)->current_sec >= CUE(d)->N ) return false;

  // Intenta llegir.
  val= CUE(d)->maps[CUE(d)->current_sec];
  *audio= CUE(d)->tracks[val.track_id].type==AUDIO;
  if ( val.offset == -1 )
    memset ( buf, 0, CD_SEC_SIZE );
  else
    {
      force_seek ( CUE(d) );
      if ( fread ( buf, CD_SEC_SIZE, 1, val.file->f ) != 1 )
        return false;
    }
  if ( move ) ++(CUE(d)->current_sec);
  
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

  sec_map_t val;
  const track_t *track;
  size_t tmp;
  uint16_t crc;
  int i;
  

  // CRC ok!!!
  *crc_ok= true;
  
  if ( CUE(d)->current_sec >= CUE(d)->N ) return false;

  // NOTA!!!! No tenim subcanal en CUE així que vaig a assumir que
  // sempre torna ADDR=1 i no és ni LeadIn ni LeadOut.
  val= CUE(d)->maps[CUE(d)->current_sec];
  track= &(CUE(d)->tracks[val.track_id]);

  // En el primer byte fiquem els 2 bits de "Sub-channel
  // synchronization field".
  buf[0]= 0x00; // ¿¿????
  
  // Si tenim el mapa de sectors defectuosos gastem eixe.
  if ( val.subq_ptr != -1 )
    {
      for ( i= 3; i < LSD_ENTRY_SIZE; ++i )
        buf[i-2]= CUE(d)->subq[val.subq_ptr][i];
      *crc_ok= false;
    }

  // Invenció normal.
  else
    {
      
      // ADR/Control
      // --> Assumisc ADR 1 in Data region
      buf[1]=
        (0x1) | // ADR
        (track->type==AUDIO ? 0x00 : 0x40);
      
      // Track and index.
      buf[2]= BCD ( val.track_id+1 );
      buf[3]= val.index_id; // Ja està transformat i és 00 per als pregap
      
      // Track relative MSF address
      if ( CUE(d)->current_sec >= track->sector_index01 )
        tmp= CUE(d)->current_sec - track->sector_index01;
      else // Distància a track->sector_index01 (estem en un pregap)
        // El -1 està copiat de mednafen.
        tmp= track->sector_index01 - 1 - CUE(d)->current_sec;
      buf[4]= BCD ( tmp/(60*75) ); tmp%= 60*75; // MM
      buf[5]= BCD ( tmp/75 ); tmp%= 75; // SS
      buf[6]= BCD ( tmp ); // FF (encara que serien sectors no
      // frames... Confusió en la terminologia)
      
      // Reserved
      buf[7]= 0x00;
      
      // Absolute MSF address
      tmp= CUE(d)->current_sec;
      buf[8]= BCD ( tmp/(60*75) ); tmp%= 60*75; // MM
      buf[9]= BCD ( tmp/75 ); tmp%= 75; // SS
      buf[10]= BCD ( tmp ); // FF
      
      // CRC-16-CCITT error detection code (big-endian: bytes ordered MSB,
      // LSB)
      crc= CD_crc_subq_calc ( buf );
      buf[11]= (crc>>8)&0xFF;
      buf[12]= crc&0xFF;

    }
  
  // Mou.
  if ( move ) ++(CUE(d)->current_sec);
  
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
  size_t t,e;
  const track_t *tp;
  const entry_t *ep;
  
  
  // Reserva memòria.
  ret= mem_alloc ( CD_Info, 1 );
  ret->_mem_sessions= sess= mem_alloc ( CD_SessionInfo, 1 );
  ret->_mem_tracks= tracks= mem_alloc ( CD_TrackInfo, CUE(d)->NT );
  ret->_mem_indexes= indexes= mem_alloc ( CD_IndexInfo, CUE(d)->NE );
  
  // Sesions.
  ret->nsessions= 1;
  ret->sessions= sess;
  sess[0].ntracks= CUE(d)->NT;
  sess[0].tracks= tracks;

  // Tracks i entries.
  ret->ntracks= CUE(d)->NT;
  ret->tracks= tracks;
  for ( t= 0; t < CUE(d)->NT; ++t )
    {
      tp= &(CUE(d)->tracks[t]);
      tracks[t].id= BCD ( t+1 );
      tracks[t].nindexes= tp->N;
      tracks[t].indexes= indexes;
      tracks[t].is_audio= (tp->type == AUDIO);
      tracks[t].audio_four_channel= false;
      tracks[t].audio_preemphasis= false;
      tracks[t].digital_copy_allowed= true; // Per què no?
      if ( t > 0 )
        tracks[t-1].pos_last_sector=
          CD_get_position ( CUE(d)->entries[tp->p].time - 1 );
      for ( e= tp->p; e != (size_t) (tp->p+tp->N); ++e, ++indexes )
        {
          ep= &(CUE(d)->entries[e]);
          indexes->id= ep->type==INDEX ? BCD ( ep->id ) : 0;
          indexes->pos= CD_get_position ( ep->time );
        }
    }
  tracks[CUE(d)->NT-1].pos_last_sector= CD_get_position ( CUE(d)->N-1 );

  // Disk type. (Açò és com un resum)
  switch ( CUE(d)->tracks[0].type )
    {
    case AUDIO: ret->type= CD_DISK_TYPE_AUDIO; break;
    case MODE1: ret->type= CD_DISK_TYPE_MODE1; break;
    case MODE2: ret->type= CD_DISK_TYPE_MODE2; break;
    }
  for ( t= 1; t < CUE(d)->NT; ++t )
    {
      tp= &(CUE(d)->tracks[t]);
      if ( ret->type == CD_DISK_TYPE_AUDIO ) // Sols audio
        {
          if ( tp->type != AUDIO )
            {
              ret->type= CD_DISK_TYPE_UNK;
              break;
            }
        }
      else if ( ret->type == CD_DISK_TYPE_MODE1 ||
        	ret->type == CD_DISK_TYPE_MODE1_AUDIO )
        {
          if ( tp->type == AUDIO ) ret->type= CD_DISK_TYPE_MODE1_AUDIO;
          else if ( tp->type == MODE2 )
            {
              ret->type= CD_DISK_TYPE_UNK;
              break;
            }
        }
      else if ( ret->type == CD_DISK_TYPE_MODE2 ||
        	ret->type == CD_DISK_TYPE_MODE2_AUDIO )
        {
          if ( tp->type == AUDIO ) ret->type= CD_DISK_TYPE_MODE2_AUDIO;
          else if ( tp->type == MODE1 )
            {
              ret->type= CD_DISK_TYPE_UNK;
              break;
            }
        }
    }
  
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
  return (int)
    (CUE(d)->current_sec>=CUE(d)->N ?
     CUE(d)->NT : (size_t) (CUE(d)->maps[CUE(d)->current_sec].track_id + 1));
} // end get_current_track


static uint8_t
get_current_index (
        	   CD_Disc *d
        	   )
{
  return CUE(d)->current_sec>=CUE(d)->N ?
    0x00 : CUE(d)->maps[CUE(d)->current_sec].index_id;
} // end get_current_index


static bool
move_to_leadin (
        	CD_Disc *d
        	)
{

  fprintf ( stderr, "[WW] lead-in not available in CUE/BIN format,"
            " moving to sector 0 (Track 1)\n" );
  CUE(d)->current_sec= 0;
  
  return true;
  
} // end move_to_leadin


static CD_Position
tell (
      CD_Disc *d
      )
{
  return CD_get_position ( CUE(d)->current_sec );
} // end tell




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

CD_Disc *
CD_cue_disc_new (
        	 const char  *fn,
        	 char       **err // Pot ser NULL
        	 )
{

  CD_CUE_Disc *new;


  // Inicialitza.
  new= mem_alloc ( CD_CUE_Disc, 1 );
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
  new->files= NULL;
  new->tracks= NULL;
  new->entries= NULL;
  new->maps= NULL;
  new->current_sec= 0;
  new->subq= NULL;
  
  // Llig.
  if ( !read_cue ( fn, new, err ) )
    goto error;

  // Crea el mapa de sectors.
  if ( !create_map_sectors ( new, err ) )
    goto error;

  // Intenta llegir LSD.
  if ( !try_read_lsd ( new, fn, err ) )
    goto error;
  
  return (CD_Disc *) new;
  
 error:
  CD_disc_free ( (CD_Disc *) new );
  return NULL;
  
} // end CD_cue_disc_new
