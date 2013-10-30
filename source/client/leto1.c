/*  $Id: leto1.c,v 1.88 2008/07/08 19:17:32 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Harbour Leto RDD
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

#define HB_OS_WIN_32_USED
#define HB_SET_IMPORT
#define MAX_STR_LEN 255

#include "hbapi.h"
#include "hbinit.h"
#include "hbapiitm.h"
#include "hbapierr.h"
#include "hbdbferr.h"
#include "hbapilng.h"
#include "hbdate.h"
#include "hbset.h"
#include "hbvm.h"
#include "rddsys.ch"
#include <ctype.h>
#if defined(HB_OS_UNIX)
   #include <netinet/in.h>
   #include <arpa/inet.h>
#endif
#include "funcleto.h"

typedef struct _LFCCONNECTION_
{
   HB_SOCKET_T hSocket;
   char *      pAddr;
   int         iPort;
   char *      szPath;
   char        szVersion[16];
} LFCCONNECTION;

#define MAX_CONNECTIONS_NUMBER   5

static char * szBuffer = NULL;
static long int lBufferLen;
static LFCCONNECTION lfcConnPool[ MAX_CONNECTIONS_NUMBER ];
static BOOL s_bInit = 0;

char * lfc_NetName( void );

void lfc_ConnectionClose( LFCCONNECTION * pConnection )
{

   if( pConnection->pAddr )
   {
      hb_ipclose( pConnection->hSocket );
      pConnection->hSocket = 0;
      hb_xfree( pConnection->pAddr );
      pConnection->pAddr = NULL;
      if( pConnection->szPath )
         hb_xfree( pConnection->szPath );
      pConnection->szPath = NULL;
   }

}

static ERRCODE lfcInit( void )
{
   if( !s_bInit )
   {
      hb_ipInit();
      memset( lfcConnPool, 0, sizeof(LFCCONNECTION) * MAX_CONNECTIONS_NUMBER );
      szBuffer = (char*) malloc(HB_SENDRECV_BUFFER_SIZE);
      lBufferLen = HB_SENDRECV_BUFFER_SIZE;
   }
   return SUCCESS;
}

static ERRCODE lfcExit( void )
{
   if( !s_bInit )
   {
      USHORT i;

      for( i=0; i<MAX_CONNECTIONS_NUMBER; i++ )
         lfc_ConnectionClose( &(lfcConnPool[i]) );

      free( szBuffer );
      hb_ipCleanup();
   }
   return SUCCESS;
}

char * lfc_firstchar( void )
{
   int iLenLen;

   if( ( iLenLen = (((int)*szBuffer) & 0xFF) ) < 10 )
      return (szBuffer+2+iLenLen);
   else
      return (szBuffer+1);
}

LONG lfc_msglen( void )
{
   int iLenLen;

   iLenLen = (((int)*szBuffer) & 0xFF);
   if( iLenLen < 10 )
      return lfc_b2n( (unsigned char *)szBuffer+1, iLenLen );
   else
      return 0;
}

static long int lfc_Recv( HB_SOCKET_T hSocket )
{
   int iRet, iLenLen = 0;
   char szRet[HB_SENDRECV_BUFFER_SIZE], * ptr;
   long int lLen = 0;

   // writelog("recv",0);
   ptr = szBuffer;
   do
   {
      while( hb_ipDataReady( hSocket,2 ) == 0 )
      {
      }
      iRet = hb_ipRecv( hSocket, szRet, HB_SENDRECV_BUFFER_SIZE );
      // writelog(szRet,iRet);
      if( iRet <= 0 )
         break;
      else
      {
         if( (ptr - szBuffer) + iRet > lBufferLen )
         {
            char * szTemp;
            lBufferLen += HB_SENDRECV_BUFFER_SIZE;
            szTemp = (char*) malloc( lBufferLen );
            memcpy( szTemp, szBuffer, ptr-szBuffer );
            ptr = szTemp + (ptr-szBuffer);
            free( szBuffer );
            szBuffer = szTemp;
         }
         memcpy( ptr, szRet, iRet );
         ptr += iRet;
         if( lLen == 0 && ( iLenLen = (((int)*szBuffer) & 0xFF) ) < 10 && (ptr-szBuffer) > iLenLen )
            lLen = lfc_b2n( (unsigned char *)szBuffer+1, iLenLen );
         if( ( lLen > 0 && lLen+iLenLen+1 <= ptr-szBuffer ) ||
               ( iLenLen >= 10 && *(ptr-1) == '\n' && *(ptr-2) == '\r' ) )
            break;
      }
   }
   while(1);

   if( iLenLen >= 10 )
      *(ptr-2) = '\0';
   else
      *ptr = '\0';

   // writelog( szBuffer,ptr - szBuffer );
   return (long int) (ptr - szBuffer);
}

long int lfc_DataSendRecv( LFCCONNECTION * pConnection, char * sData, ULONG ulLen )
{
   if ( hb_ipSend( pConnection->hSocket, sData, (ulLen)? ulLen:strlen(sData), -1 ) <= 0 )
   {
      return 0;
   }
   return lfc_Recv( pConnection->hSocket );
}

int lfc_getIpFromPath( char * sSource, char * szAddr, int * piPort, char * szPath, BOOL bFile )
{
   char * ptrPort, * ptr = sSource;

   while( strlen(ptr) >= 2 && (ptr[0] != '/' || ptr[1] != '/'))
   {
      if( ( ptrPort = strchr( ptr, ',' ) ) == NULL)
         ptrPort = strchr( ptr, ';' );
      if( ptrPort )
         ptr = ptrPort + 1;
      else
         return 0;
   }
   ptr ++;
   if( ( ptrPort = strchr( ptr,':' ) ) == NULL )
   {
      return 0;
   }

   ptr ++;
   memcpy( szAddr, ptr, ptrPort-ptr );
   szAddr[ptrPort-ptr] = '\0';
   ptrPort ++;
   sscanf( ptrPort, "%d", piPort );

   if( szPath )
   {
      do ptrPort ++; while( *ptrPort>='0' && *ptrPort<='9' );
      ptr = ptrPort;
      ptrPort = sSource + strlen(sSource);
      if( bFile )
      {
         while( *ptrPort != '/' && *ptrPort != '\\' ) ptrPort --;
      }
      if( ptrPort < ptr )
      {
         return 0;
      }
      else if( ptrPort >= ptr )
      {
         ptrPort ++;
         if( *(ptr+2) == ':' )
            ptr ++;
         memcpy( szPath, ptr, ptrPort-ptr );
      }
      szPath[ptrPort-ptr] = '\0';
      if( ( ptrPort = strchr( szPath, ',' ) ) != NULL || ( ptrPort = strchr( szPath, ';' ) ) != NULL)
         ptrPort[0] = '\0';
      ptr = szPath;
      while( ( ptr = strchr( ptr,'.' ) ) != NULL )
      {
         ptr ++;
         if( *ptr == '.' )
            return 0;
      }
   }
   return 1;
}

void lfc_getFileFromPath( char * sSource, char * szFile )
{
   char * ptr, * ptrEnd;

   ptrEnd = ptr = sSource + strlen(sSource);
   while( ptr >= sSource && *ptr != '/' && *ptr != '\\' ) ptr --;
   ptr ++;
   memcpy( szFile, ptr, ptrEnd-ptr );
   *(szFile + (ptrEnd-ptr)) = '\0';
}

LFCCONNECTION * lfc_getConnectionPool( void )
{
   return lfcConnPool;
}

LFCCONNECTION * lfc_ConnectionFind( char * szAddr, int iPort )
{
   int i;

   for( i=0; i<MAX_CONNECTIONS_NUMBER; i++ )
   {
      if( lfcConnPool[i].pAddr && lfcConnPool[i].iPort == iPort &&
              !strcmp(lfcConnPool[i].pAddr,szAddr) )
         return &(lfcConnPool[i]);
   }
   return NULL;
}

LFCCONNECTION * lfc_ConnectionNew( char * szAddr, int iPort )
{
   HB_SOCKET_T hSocket;
   char szData[256];
   int i;

   lfcInit();

   for( i=0; i<MAX_CONNECTIONS_NUMBER; i++ )
   {
      if( !lfcConnPool[i].pAddr )
         break;
   }
   if( i < MAX_CONNECTIONS_NUMBER )
   {
      hSocket = hb_ipConnect( szAddr, htons( iPort ), 5000 );
      if( !hb_iperrorcode() )    
      {
         lfcConnPool[i].hSocket = hSocket;
         lfcConnPool[i].iPort = iPort;
         lfcConnPool[i].pAddr = (char*) hb_xgrab( strlen(szAddr)+1 );
         memcpy( lfcConnPool[i].pAddr, szAddr, strlen(szAddr) );
         lfcConnPool[i].pAddr[strlen(szAddr)] = '\0';
         if( lfc_Recv( hSocket ) )
         {
            char * ptr = szBuffer, * pName, **pArgv;
            char szFile[_POSIX_PATH_MAX + 1];

            memcpy( lfcConnPool[i].szVersion, ptr, strlen(ptr) );
            lfcConnPool[i].szVersion[strlen(ptr)] = '\0';

            pName = lfc_NetName();
            pArgv = hb_cmdargARGV();
            lfc_getFileFromPath( *pArgv, szFile );
#if defined( HB_OS_WIN_32 )
            sprintf( szData, "intro;win-1.0;%s;%s;\r\n",
               (pName)? pName:"", szFile );
#else
            sprintf( szData, "intro;linux-1.0;%s;%s;\r\n",
               (pName)? pName:"", szFile );
#endif
            if( pName )
              hb_xfree( pName );
            if ( hb_ipSend( hSocket, szData, strlen(szData), -1 ) <= 0 )
            {
               return NULL;
            }           
            if( !lfc_Recv( hSocket ) )
            {
               return NULL;
            }
         }
         return &(lfcConnPool[i]);
      }
   }
   return NULL;
}

int lfc_getCmdItem( char ** pptr, char * szDest )
{
   char * ptr = *pptr;

   while( *ptr && *ptr != ';' ) ptr++;
   if( *ptr )
   {
      if( ptr > *pptr )
         memcpy( szDest, *pptr, ptr-*pptr );
      szDest[ptr-*pptr] = '\0';
      *pptr = ptr;
      return 1;
   }
   else
      return 0;
}

HB_FUNC( LFCEXIT )
{
   lfcExit();
}
