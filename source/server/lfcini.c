/*  $Id: letofunc.c,v 1.107 2008/07/10 19:26:49 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto file check server functions
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
#include "hbapifs.h"
#ifdef __XHARBOUR__
   #include "hbfast.h"
#else
   #include "hbapicls.h"
#endif
#include "hbapierr.h"
#include "math.h"
#include "srvleto.h"

/* EOL:  DOS - \r\n  Unix - \n,  \r = 13, \n = 10 */

extern void lfc_addModule( char * szModname, USHORT uiItems );
extern void lfc_addItem2Module( char * szCln, char * szSrv, USHORT nSystem );
extern void lfc_releaseModules( void );

#define MAX_FILES_IN_MODULE  128

#define HB_SKIPTABSPACES( sptr ) while( *sptr && ( *sptr == ' ' || *sptr == '\t' || \
         *sptr == '\r' || *sptr == '\n' ) ) ( sptr )++

#define HB_SKIPCHARS( sptr ) while( *sptr && *sptr != ' ' && *sptr != '\t' && \
         *sptr != '=' && *sptr != ',' && *sptr != '\r' && *sptr != '\n' ) ( sptr )++

#define HB_SKIPTILLEOL( sptr ) while( *sptr && *sptr != '\n' ) ( sptr )++

static LONG lIniTime = -1;

void lfc_checkIni( void )
{
   char * szIni = "lfcmod.ini";
   BOOL lModule = 0;

   lfc_fileInfo( szIni );

   if( lfc_filetime() != lIniTime )
   {
      FHANDLE fhnd;
      ULONG ulLen;

      lIniTime = lfc_filetime();

      fhnd = hb_fsOpen( szIni, FO_READ | FO_SHARED );
      ulLen = hb_fsSeek( fhnd, 0, FS_END );
      if( ulLen != 0 )
      {
         BYTE * ptr, * ptr1, * ptrEnd;
         BYTE * pBuffer = ( BYTE * ) hb_xgrab( ulLen + 1 );
         BYTE * pFiles[MAX_FILES_IN_MODULE], * pModule;
         int i, iFiles;

         // writelog( "check-1",0);
         lfc_releaseModules();

         hb_fsSeek( fhnd, 0, FS_SET );
         hb_fsReadLarge( fhnd, pBuffer, ulLen );
         hb_fsClose( fhnd );
         pBuffer[ulLen] = '\0';

         ptr = pBuffer;

         // writelog( "check-2",0);
         while( *ptr )
         {
            HB_SKIPTABSPACES( ptr );
            if( *ptr == '[' )
            {
               iFiles = 0;
               pModule = NULL;
               lModule = !hb_strnicmp( ptr,"[module]",8 );
               HB_SKIPTILLEOL( ptr );
            }
            else
            {
               if( lModule )
               {
                  ptrEnd = ptr;
                  while( *ptrEnd && *ptrEnd != '=' ) ptrEnd ++;
                  if( ptrEnd-ptr == 4 && !hb_strnicmp( ptr,"name",4 ) )
                  {
                     pModule = ptrEnd + 1;
                  }
                  else if( ptrEnd-ptr == 4 && !hb_strnicmp( ptr,"file",4 ) )
                  {
                     pFiles[iFiles] = ptrEnd + 1;
                     iFiles ++;
                  }
                  else if( ptrEnd-ptr == 5 && ( !hb_strnicmp( ptr,"lfile",5 ) || !hb_strnicmp( ptr,"wfile",5 ) ) )
                  {
                     pFiles[iFiles] = ptrEnd + 1;
                     iFiles ++;
                  }
               }
               HB_SKIPTILLEOL( ptr );
            }
            if( *ptr )
               ptr ++;
            HB_SKIPTABSPACES( ptr );
            if( ( ! *ptr || *ptr == '[' ) && pModule && iFiles > 0 )
            {
               // writelog( "check-5",0);
               USHORT nSystem;

               ptr1 = pModule;
               HB_SKIPCHARS( ptr1 );
               *ptr1 = '\0';
               // writelog( pModule,0);
               lfc_addModule( pModule, iFiles );
               for( i=0; i<iFiles; i++ )
               {
                  ptr1 = pFiles[i];
                  if( *(ptr1-6) == 'l' || *(ptr1-6) == 'L' )
                     nSystem = 1;
                  else if( *(ptr1-6) == 'w' || *(ptr1-6) == 'W' )
                     nSystem = 2;
                  else
                     nSystem = 0;

                  HB_SKIPCHARS( ptr1 );
                  if( *ptr1 == ',' )
                  {
                     *ptr1 = '\0';
                     ptr1 ++;
                     ptrEnd = ptr1;
                     HB_SKIPCHARS( ptrEnd );
                     *ptrEnd = '\0';
                     lfc_addItem2Module( pFiles[i], ptr1, nSystem );
                  }
                  else
                  {
                     *ptr1 = '\0';
                     ptr1 ++;
                     lfc_addItem2Module( pFiles[i], NULL, nSystem );
                  }
               }
               // writelog( "check-6",0);
            }
         }

         hb_xfree( pBuffer );
      }
   }
}

