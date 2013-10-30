/*  $Id: leto_win.c,v 1.7 2008/06/01 18:35:23 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto db server (Windows) functions
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
#include "srvleto.h"

extern char carr1[40][40];
extern USHORT uiarr1len;
extern char carr2[100][40];
extern USHORT uiarr2len;

typedef struct
{
   HANDLE hMem;
   LPVOID lpView;
   BOOL bNewArea;
} MEMAREA;

static MEMAREA s_MemArea = { 0,0,0 };

HB_FUNC( MSGSTOP )
{
   MessageBox( GetActiveWindow(), hb_parc(1), "Leto fc server", MB_OK | MB_ICONSTOP );
}

HB_FUNC( LFC_CREATEMEMAREA )
{
   s_MemArea.hMem = CreateFileMapping( (HANDLE)0xFFFFFFFF, NULL, PAGE_READWRITE, 0, 1024, __TEXT("Leto_FC") );

   if( s_MemArea.hMem )
   {
      s_MemArea.bNewArea = !( GetLastError() == ERROR_ALREADY_EXISTS );
      s_MemArea.lpView = MapViewOfFile( s_MemArea.hMem, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0 );
      if( s_MemArea.bNewArea )
         ((char*)s_MemArea.lpView)[0] = 'F';
   }

   hb_retni( (s_MemArea.hMem)?  s_MemArea.bNewArea : -1 );
}

HB_FUNC( LFC_CLOSEMEMAREA )
{
   if( s_MemArea.lpView )
   {
      UnmapViewOfFile( s_MemArea.lpView );
      s_MemArea.lpView = NULL;
   }
   if( s_MemArea.hMem )
   {
      CloseHandle( s_MemArea.hMem );
      s_MemArea.hMem = NULL;
   }
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

