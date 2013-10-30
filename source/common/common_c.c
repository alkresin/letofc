/*  $Id: common_c.c,v 1.7 2008/06/01 18:35:23 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto db server functions
 *
 * Copyright 2008 Alexander S. Kresin <alex / at / belacy.belgorod.su>
 * www - http://www.harbour-project.org
 *
 * Copyright 2007 Przemyslaw Czerpak <druzus / at / priv.onet.pl>
 * www - http://www.harbour-project.org
 * hb_dateMilliSeconds() function
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
#include "hbdate.h"
#include "hbapierr.h"
#include "hbapifs.h"
#include "hbvm.h"
#include "hb_io.h"

#include <time.h>

#if ( defined( HB_OS_BSD ) || defined( HB_OS_LINUX ) ) && !defined( __WATCOMC__ )
   #include <sys/time.h>
#elif !( defined( HB_WINCE ) && defined( _MSC_VER ) )
   #include <sys/timeb.h>
#endif
#if defined( OS_UNIX_COMPATIBLE )
   #include <sys/times.h>
   #include <sys/types.h>
   #include <utime.h>
   #include <unistd.h>
#endif
#if defined( HB_OS_WIN_32 )
   #include <windows.h>
#elif defined(_MSC_VER)
   #define timeb _timeb
   #define ftime _ftime
#endif

static LONG  s_lDate = 0;
static LONG  s_lTime = 0;
static LONG  s_lSize = 0;

void writelog( char* s, int n )
{
   FHANDLE handle;

   if( hb_fsFile( (unsigned char *) "ac.log" ) )
      handle = hb_fsOpen( (unsigned char *) "ac.log", FO_WRITE );
   else
      handle = hb_fsCreate( (unsigned char *) "ac.log", 0 );

   hb_fsSeek( handle,0, SEEK_END );
   hb_fsWrite( handle, (unsigned char *) s, (n)? n:strlen(s) );
   hb_fsWrite( handle, (unsigned char *) "\n\r", 2 );

   hb_fsClose( handle );
}

static HB_ULONG hb_dateMilliSec( void )
{
#if defined(HB_OS_WIN_32)
   SYSTEMTIME st;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   GetLocalTime( &st );

   return ( HB_ULONG ) hb_dateEncode( st.wYear, st.wMonth, st.wDay ) * 86400000L +
          ( ( st.wHour * 60 + st.wMinute ) * 60 + st.wSecond ) * 1000 +
          st.wMilliseconds;
#elif ( defined( HB_OS_LINUX ) || defined( HB_OS_BSD ) ) && !defined( __WATCOMC__ )
   struct timeval tv;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   gettimeofday( &tv, NULL );

   return ( HB_ULONG ) tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
   struct timeb tb;

   HB_TRACE(HB_TR_DEBUG, ("hb_dateMilliSeconds()"));

   ftime( &tb );

   return ( HB_ULONG ) tb.time * 1000 + tb.millitm;
#endif
}

HB_FUNC( LFC_MILLISEC )
{
   hb_retnl( hb_dateMilliSec() );
}

long lfc_Date( void )
{
   int iYear, iMonth, iDay;
   hb_dateToday( &iYear, &iMonth, &iDay );
   return hb_dateEncode( iYear, iMonth, iDay );
}

HB_FUNC( LFC_LONG2DATE )
{
   hb_retdl( hb_parnl(1) );
}


long int lfc_b2n( unsigned char *s, int iLenLen )
{
   unsigned long int n = 0;
   int i = 0;

   do
   {
      n += (unsigned long int)(s[i] << (8*i));
      i ++;
   }
   while( i < iLenLen );
   return n;
}

int lfc_n2b( unsigned char *s, unsigned long int n )
{
   int i = 0;

   do
   {
      s[i] = (unsigned char)(n & 0xff);
      n = n >> 8;
      i ++;
   }
   while( n );
   s[i] = '\0';
   return i;
}

BYTE * lfc_AddLen( BYTE * pData, ULONG * ulLen, BOOL lBefore )
{
   if( *ulLen < 256 )
   {
      if( lBefore )
      {
         *(--pData) = (BYTE) *ulLen & 0xFF;
         *(--pData) = '\1';
      }
      else
      {
         *(pData++) = '\1';
         *(pData++) = (BYTE) *ulLen & 0xFF;
      }
      *ulLen = *ulLen + 2;
   }
   else
   {
      USHORT uiLenLen;
      BYTE sNumf[4];
      uiLenLen = lfc_n2b( sNumf, *ulLen );
      if( lBefore )
      {
         pData -= uiLenLen;
         memcpy( pData, sNumf, uiLenLen );
         *(--pData) = (BYTE) uiLenLen & 0xFF;
      }
      else
      {
         *(pData++) = (BYTE) uiLenLen & 0xFF;
         memcpy( pData, sNumf, uiLenLen );
         pData += uiLenLen;
      }
      *ulLen = *ulLen + uiLenLen + 1;
   }
   return pData;

}

void lfc_fileInfo( char * szFile )
{
   PHB_FFIND ffind;
   BOOL fFree;
   LONG uiH, uiM, uiS;

   szFile = ( char * ) hb_fsNameConv( ( BYTE * ) szFile, &fFree );
   ffind = hb_fsFindFirst( szFile, HB_FA_ALL );
   if( fFree )
      hb_xfree( szFile );

   s_lDate = ( ffind ? ffind->lDate : 0 );
   s_lSize  = ( ffind ? (LONG)ffind->size : -1 );
   if( ffind )
   {
      sscanf( ffind->szTime, "%u:%u:%u", &uiH, &uiM, &uiS );
      s_lTime = uiS + uiM*60 + uiH*3600;
   }
   else
   {
      s_lTime = 0;
   }

   hb_fsFindClose( ffind );

}

LONG lfc_filesize( void )
{
   return s_lSize;
}

LONG lfc_filedate( void )
{
   return s_lDate;
}

LONG lfc_filetime( void )
{
   return s_lTime;
}

