#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "CD.h"

int main ( int argc, const char *argv[] )
{

  char *err;
  CD_Disc *d;
  CD_Info *info;
  CD_Position pos;
  bool ok;
  uint8_t buf[CD_SEC_SIZE];
  bool audio;

  
  d= CD_disc_new ( argv[1], &err );
  if ( d == NULL )
    {
      fprintf ( stderr, "[EE] %s\n", err );
      free ( err );
    }
  else
    {
      info= CD_disc_get_info ( d );
      printf ( "NSessions: %d NTracks: %d Type: %d\n",
               info->nsessions, info->ntracks, info->type );
      ok= CD_disc_move_to_track ( d, 1 );
      printf ( "moved: %d\n", ok );
      ok= CD_disc_read ( d, buf, &audio, true );
      ok= CD_disc_read ( d, buf, &audio, true );
      ok= CD_disc_read ( d, buf, &audio, true );
      ok= CD_disc_read ( d, buf, &audio, true );
      ok= CD_disc_read ( d, buf, &audio, true ); // <-- sector amb llicència
      printf ( "read: %d audio: %d\n", ok, audio );
      fwrite ( buf, CD_SEC_SIZE, 1, stdout );
      pos= CD_disc_tell ( d );
      printf ( "\n" );
      printf ( "Next sector: %02X:%02X:%02X\n", pos.mm, pos.ss, pos.sec );
      printf ( "Adéu!\n" );
      CD_info_free ( info );
      CD_disc_free ( d );
    }

  return EXIT_SUCCESS;
  
}
