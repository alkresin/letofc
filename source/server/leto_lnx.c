/*  $Id: leto_lnx.c,v 1.5 2008/06/01 18:35:23 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto FC server (linux) functions
 *
 * Copyright 2008 Alexander S. Kresin <alex / at / belacy.belgorod.su>
 * www - http://www.harbour-project.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA (or visit the web site http://www.gnu.org/).
 *
 * As a special exception, the Harbour Project gives permission for
 * additional uses of the text contained in its release of Harbour.
 *
 * The exception is that, if you link the Harbour libraries with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the Harbour library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by the Harbour
 * Project under the name Harbour.  If you copy code from other
 * Harbour Project or Free Software Foundation releases into a copy of
 * Harbour, as the General Public License permits, the exception does
 * not apply to the code that you add in this way.  To avoid misleading
 * anyone as to the status of such modified files, you must delete
 * this exception notice from them.
 *
 * If you write modifications of your own for Harbour, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 *
 */

#include "hbapi.h"
#include "hbapiitm.h"
#include "hbvm.h"
#include "sys/shm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "srvleto.h"

typedef struct
{
   int hMem;
   char * lpView;
   BOOL bNewArea;
} MEMAREA;

static MEMAREA s_MemArea = { -1, NULL, 0 };

/*
 * lfc_CreateMemArea()
 * creates a new shared memory area or joins existing one
 * returns -1 if fails, 0 - if joins an existing and 1 - if creates a new
 */                                 
HB_FUNC( LFC_CREATEMEMAREA )
{
   key_t key;

   key = ftok( "/var/spool", 0xED );

   if( ( s_MemArea.hMem = shmget( key, 512, IPC_CREAT|IPC_EXCL|0660 ) ) == -1 )
   {
      if( ( s_MemArea.hMem = shmget(key, 512, 0) ) == -1 )
      {
         hb_retni( -1 );
         return;
      }
      s_MemArea.bNewArea = FALSE;
   }
   else
      s_MemArea.bNewArea = TRUE;

   if( ( s_MemArea.lpView = shmat( s_MemArea.hMem, 0, 0 ) ) == -1 )
   {
      shmctl( s_MemArea.hMem, IPC_RMID, 0 );
      s_MemArea.lpView = NULL;
      s_MemArea.hMem = -1;
      hb_retni( -1 );
      return;
   }
   if( s_MemArea.bNewArea )
      ((char*)s_MemArea.lpView)[0] = 'F';

   hb_retni( s_MemArea.bNewArea );
}

HB_FUNC( LFC_CLOSEMEMAREA )
{
   // writelog( "close-1", 0 );
   if( s_MemArea.lpView )
   {
      shmdt( s_MemArea.lpView );
      s_MemArea.lpView = NULL;
   }
   if( s_MemArea.hMem != -1 && s_MemArea.bNewArea )
   {
      shmctl( s_MemArea.hMem, IPC_RMID, 0 );
      s_MemArea.hMem = -1;
      // writelog( "close-2", 0 );
   }
}

HB_FUNC( LFC_SETAREANEW )
{
   s_MemArea.bNewArea = TRUE;
}

BOOL lfc_ReadMemArea( char * szBuffer, int iAddr, int iLength )
{
   if( !s_MemArea.lpView )
      return FALSE;

   memcpy( szBuffer, ((char*)s_MemArea.lpView)+iAddr, iLength );
   szBuffer[iLength] = '\0';
   return TRUE;
}

BOOL lfc_WriteMemArea( char * szBuffer, int iAddr, int iLength )
{
   if( !s_MemArea.lpView )
      return FALSE;

   memcpy( ((char*)s_MemArea.lpView)+iAddr, szBuffer, iLength );
   return TRUE;
}

HB_FUNC( LFC_DAEMON )
{
   int	iPID, stdioFD, i, numFiles;

   iPID = fork();

   switch( iPID )
   {
      case 0:             /* we are the child process */
         break;

      case -1:            /* error - bail out (fork failing is very bad) */
         fprintf(stderr,"Error: initial fork failed: %s\n", strerror(errno));
         hb_retl( FALSE );
         return;
         break;

      default:            /* we are the parent, so exit */
         hb_vmRequestQuit();
         break;
   }

   if( setsid() < 0 )
   {
      hb_retl( FALSE );
      return;
   }

   numFiles = sysconf( _SC_OPEN_MAX ); /* how many file descriptors? */		
   for( i=numFiles-1; i>=0; --i )      /* close all open files except lock */
   {
      close(i);
   }
	
   /* stdin/out/err to /dev/null */
   umask( 0 );                             /* set this to whatever is appropriate for you */
   stdioFD = open( "/dev/null",O_RDWR );   /* fd 0 = stdin */
   dup( stdioFD );                         /* fd 1 = stdout */
   dup( stdioFD );                         /* fd 2 = stderr */

   setpgrp();
   hb_retl( TRUE );
   return;

}

