/*  $Id: letomgmn.c,v 1.12 2008/06/16 16:40:21 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Harbour Leto management functions
 *
 * Copyright 2008 Pavel Tsarenko <tpe2 / at / mail.ru>
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
#include "funcleto.h"
#include "hbdate.h"

typedef struct _LFCCONNECTION_
{
   HB_SOCKET_T hSocket;
   char *      pAddr;
   int         iPort;
   char *      szPath;
   char        szVersion[16];
} LFCCONNECTION;

#define MAX_CONNECTIONS_NUMBER   5

static int iFError = 0;
static LFCCONNECTION * pCurrentConn = NULL;

extern int lfc_getIpFromPath( char * sSource, char * szAddr, int * piPort, char * szPath, BOOL lFile );
extern void lfc_getFileFromPath( char * sSource, char * szFile );
extern LFCCONNECTION * lfc_ConnectionFind( char * szAddr, int iPort );
extern LFCCONNECTION * lfc_ConnectionNew( char * szAddr, int iPort );
extern void lfc_ConnectionClose( LFCCONNECTION * pConnection );
extern LFCCONNECTION * lfc_getConnectionPool( void );
extern long int lfc_DataSendRecv( LFCCONNECTION * pConnection, char * sData, ULONG ulLen );
char * lfc_firstchar( void );
LONG lfc_msglen( void );
int lfc_getCmdItem( char ** pptr, char * szDest );

HB_FUNC( LFC_FERROR )
{
   hb_retni( iFError );
}

HB_FUNC( __LFC_CONNECT )
{
   LFCCONNECTION * pConnection;
   char szAddr[96];
   int iPort;

   if( lfc_getIpFromPath( hb_parc(1), szAddr, &iPort, NULL, FALSE ) &&
       ( ( ( pConnection = lfc_ConnectionFind( szAddr, iPort ) ) != NULL ) ||
         ( ( pConnection = lfc_ConnectionNew( szAddr, iPort ) ) ) != NULL ) )
   {
      pCurrentConn = pConnection;
      hb_retni( pConnection - lfc_getConnectionPool() + 1 );
   }
   else
   {
      pCurrentConn = NULL;
      hb_retni( -1 );
   }
}

HB_FUNC( LFC_DISCONNECT )
{
   if( pCurrentConn )
      lfc_ConnectionClose( pCurrentConn );
}

HB_FUNC( LFC_SETCURRENTCONNECTION )
{
   USHORT uiConn = hb_parni(1);

   if( uiConn < MAX_CONNECTIONS_NUMBER )
      pCurrentConn = lfc_getConnectionPool() + uiConn - 1;
   else
      pCurrentConn = NULL;
}

HB_FUNC( LFC_GETCURRENTCONNECTION )
{
   if( pCurrentConn )
      hb_retni( pCurrentConn - lfc_getConnectionPool() + 1 );
   else
      hb_retni( -1 );
}

HB_FUNC( LFC_GETSERVERVERSION )
{
   if( pCurrentConn )
      hb_retc( pCurrentConn->szVersion );
   else
      hb_retc( "" );
}

HB_FUNC( LFC_GETFILELIST )
{
   char szData[48];

   sprintf( szData, "flist;%s;\r\n", hb_parc(1) );

   if( pCurrentConn && lfc_DataSendRecv( pCurrentConn, szData, 0 ) )
   {
      PHB_ITEM temp;
      PHB_ITEM aInfo = hb_itemArrayNew( 3 );
      PHB_ITEM aFiles, aItem;
      char szData[256];
      char * ptr = lfc_firstchar(), * ptr1;
      LONG lTime, lNumber, l;

      // writelog( ptr,0 );
      if( !lfc_getCmdItem( &ptr, szData ) )
      {
         hb_itemRelease( aInfo );
         hb_ret();
         return;
      }

      sscanf( szData, "%ld", &lTime );
      temp = hb_itemPutNL( NULL, lTime );
      hb_itemArrayPut( aInfo, 1, temp );
      hb_itemRelease( temp );

      ptr ++;
      if( !lfc_getCmdItem( &ptr, szData ) )
      {
         temp = hb_itemPutNL( NULL, 0 );
         hb_itemArrayPut( aInfo, 2, temp );
         hb_itemRelease( temp );

         hb_itemReturn( aInfo );
         hb_itemRelease( aInfo );
         return;
      }

      ptr ++;
      sscanf( szData, "%ld", &lNumber );
      aFiles = hb_arrayGetItemPtr( aInfo, 2 );
      hb_arrayNew( aFiles, lNumber );
      for( l=1; l<=lNumber; l++ )
      {
         aItem = hb_arrayGetItemPtr( aFiles, l );
         hb_arrayNew( aItem, 3 );

         if( !lfc_getCmdItem( &ptr, szData ) ) return; ptr ++;
#if defined( HB_OS_WIN_32 )
            for( ptr1 = szData; *ptr1; ptr1++ )
               if( *ptr1 == '/' )
                  *ptr1 = '\\';
#else
            for( ptr1 = szData; *ptr1; ptr1++ )
               if( *ptr1 == '\\' )
                  *ptr1 = '/';
#endif
         hb_itemPutC( hb_arrayGetItemPtr( aItem, 1 ), szData );

         if( !lfc_getCmdItem( &ptr, szData ) ) return; ptr ++;
         sscanf( szData, "%ld", &lTime );
         hb_itemPutNL( hb_arrayGetItemPtr( aItem, 2 ), lTime );

         if( !lfc_getCmdItem( &ptr, szData ) ) return; ptr ++;
         sscanf( szData, "%ld", &lTime );
         hb_itemPutNL( hb_arrayGetItemPtr( aItem, 3 ), lTime );
      }

      if( lfc_getCmdItem( &ptr, szData ) )
         sscanf( szData, "%ld", &lTime );
      else
         lTime = 0;
      temp = hb_itemPutNL( NULL, lTime );
      hb_itemArrayPut( aInfo, 3, temp );
      hb_itemRelease( temp );

      hb_itemReturn( aInfo );
      hb_itemRelease( aInfo );
   }
}

HB_FUNC( LFC_GETFILE )
{
   int iNum;
   char * szFile;
   char szData[_POSIX_PATH_MAX + 1];
   BOOL bRetVal = FALSE;

   if( ISNUM(1) )
   {
      sprintf( szData, "fget;n;%d;\r\n", hb_parni(1) );
      szFile = hb_parc( 2 );
   }
   else if( ISCHAR( 1 ) )
   {
      szFile = hb_parc( 1 );
      sprintf( szData, "fget;s;%s;\r\n", szFile );
   }
   else
   {
      hb_retl( 0 );
      return;
   }

   if( pCurrentConn && lfc_DataSendRecv( pCurrentConn, szData, 0 ) )
   {
      char * ptr = lfc_firstchar();
      LONG lLen = lfc_msglen() - 1;
#if defined( HB_OS_WIN_32 )
      FHANDLE fhnd = hb_fsCreate( szFile, FC_NORMAL );
#else
      FHANDLE fhnd = hb_fsCreate( szFile, ( ISLOG(3) && hb_parl(3) )? FC_NORMAL | FC_SYSTEM : FC_NORMAL );
#endif

      if( fhnd != FS_ERROR )
      {
         bRetVal = ( hb_fsWriteLarge( fhnd, ( BYTE * ) ptr, lLen ) == lLen );
         hb_fsClose( fhnd );
      }
   }
   hb_retl( bRetVal );
}

HB_FUNC( LFC_ISFILEEXIST )
{
   PHB_FFIND ffind;
   BOOL fFree;
   char * szFile = hb_parc(1);

   szFile = ( char * ) hb_fsNameConv( ( BYTE * ) szFile, &fFree );
   ffind = hb_fsFindFirst( szFile, HB_FA_DIRECTORY );
   if( fFree )
      hb_xfree( szFile );

   if( ffind )
      hb_retl( 1 );
   else
      hb_retl( 0 );

   hb_fsFindClose( ffind );

}

HB_FUNC( LFC_ISFILEOLD )
{
   LONG lDate = hb_parni(2);
   LONG lTime = hb_parni(3);
   LONG lDelta = hb_parni(4);
   int iRes;

   lfc_fileInfo( hb_parc(1) );

   iRes = ( ( lfc_filedate() < lDate ) ||
      ( lfc_filedate() == lDate && lfc_filetime() < (lTime - lDelta) ) );

   if( iRes )
   {
      char s[256];
      sprintf( s,"%s %lu %lu %lu %lu %lu",hb_parc(1),lfc_filedate(),lDate,lfc_filetime(),lTime,lDelta );
      writelog( s,0 );      
   }
   hb_retl( iRes );
}

HB_FUNC( LFC_DATEJULIAN )
{

   hb_retnl( lfc_Date() );
}

HB_FUNC( LFC_PATH )
{
   if( pCurrentConn )
   {
     hb_retc( pCurrentConn->szPath ? pCurrentConn->szPath : "");
     if( ISCHAR(1) )
     {
        if( pCurrentConn->szPath )
           hb_xfree( pCurrentConn->szPath );
        pCurrentConn->szPath = (char*) hb_xgrab( hb_parclen(1) + 1 );
        strcpy( pCurrentConn->szPath, hb_parc( 1 ) );
     }
   }
   else
      hb_retc( "" );
}

HB_FUNC( LFC_MGGETINFO )
{
   if( pCurrentConn )
   {
      if( lfc_DataSendRecv( pCurrentConn, "mgmt;00;\r\n", 0 ) )
      {
         PHB_ITEM temp;
         PHB_ITEM aInfo = hb_itemArrayNew( 10 );
         char szData[32];
         char * ptr = lfc_firstchar();
         int i;

         for( i=1; i<=10; i++ )
         {
            if( !lfc_getCmdItem( &ptr, szData ) )
            {
               hb_itemReturn( aInfo );
               hb_itemRelease( aInfo );
               return;
            }
            ptr ++;
            temp = hb_itemPutCL( NULL, szData, strlen(szData) );
            hb_itemArrayPut( aInfo, i, temp );
            hb_itemRelease( temp );
         }

         hb_itemReturn( aInfo );
         hb_itemRelease( aInfo );
      }
   }
}

HB_FUNC( LFC_MGGETUSERS )
{
   char szData[32];

   if( pCurrentConn )
   {
      if( ISNIL(1) )
         sprintf( szData, "mgmt;01;\r\n" );
      else
         sprintf( szData, "mgmt;01;%s;\r\n", hb_parc(1) );
      if ( lfc_DataSendRecv( pCurrentConn, szData, 0 ) )
      {
         int iUsers, i, j;
         char * ptr = lfc_firstchar();
         PHB_ITEM pArray, pArrayItm;

         if( !lfc_getCmdItem( &ptr, szData ) ) return; ptr ++;
         sscanf( szData, "%d", &iUsers );
         pArray = hb_itemArrayNew( iUsers );
         for ( i = 1; i <= iUsers; i++ )
         {
            pArrayItm = hb_arrayGetItemPtr( pArray, i );
            hb_arrayNew( pArrayItm, 4 );
            for( j=1; j<=4; j++ )
            {
               if( !lfc_getCmdItem( &ptr, szData ) ) return; ptr ++;
               hb_itemPutC( hb_arrayGetItemPtr( pArrayItm, j ), szData );
            }
         }
         hb_itemReturn( pArray );
         hb_itemRelease( pArray );
      }
   }
}

HB_FUNC( LFC_MGKILL )
{
   char szData[32];

   if( pCurrentConn )
   {
      if( !ISNIL(1) )
      {
         sprintf( szData, "mgmt;09;%s;\r\n", hb_parc(1) );
         lfc_DataSendRecv( pCurrentConn, szData, 0 );
      }
   }
}

