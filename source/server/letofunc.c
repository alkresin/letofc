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
#include "hbvm.h"
#ifdef __XHARBOUR__
   #include "hbfast.h"
#else
   #include "hbapicls.h"
#endif
#include "hbdate.h"
#include "srvleto.h"
#include "hbapierr.h"
#include "hbset.h"
#include "math.h"

#if defined(HB_OS_UNIX)
  #include "sys/timeb.h"
#endif 

#if defined( HB_OS_WIN_32 )
   #define HB_SOCKET_T SOCKET
#else
   #define HB_SOCKET_T int
#endif

#define INIT_USERS_ALLOC    20
#define USERS_REALLOC       20

#define INIT_MODULES_ALLOC    40
#define MODULES_REALLOC       20

extern void lfc_checkIni( void );

typedef struct
{
   BYTE *      szClnName;
   BYTE *      szSrvName;
   USHORT      nSystem;
} MITEMSTRU, *PMITEMSTRU;

typedef struct
{
   BYTE *      szModname;
   USHORT      uiItems;
   USHORT      ui;
   PMITEMSTRU  pMitemStru;
} MODSTRU, *PMODSTRU;

typedef struct
{
   HB_SOCKET_T hSocket;
   BYTE *      pBuffer;
   ULONG       ulBufferLen;
   ULONG       ulStartPos;
   BYTE *      pBufRead;
   ULONG       ulDataLen;
   BYTE *      szVersion;
   BYTE *      szAddr;
   BYTE *      szNetname;
   BYTE *      szExename;
   USHORT      nSystem;
   PMODSTRU    pMStru;
} USERSTRU, *PUSERSTRU;

static char * szRelease = "Leto FC Server v.0.1";

static int iServerPort;
static HB_SOCKET_T hSocketMain;          // Initial server socket

static PMODSTRU s_modules = NULL;
static USHORT uiModulesAlloc = 0;        // Number of allocated Module structures
static USHORT uiModulesCurr  = 0;        // Current number of modules

static PUSERSTRU s_users = NULL;
static USHORT uiUsersAlloc = 0;         // Number of allocated user structures
static USHORT uiUsersMax   = 0;         // Higher index of user structure, which was busy
static USHORT uiUsersCurr  = 0;         // Current number of users

static USHORT uiAnswerSent = 0;

static ULONG ulOperations = 0;
static ULONG ulBytesRead  = 0;
static ULONG ulBytesSent  = 0;

static long   lStartDate;
static double dStartsec;

static BYTE * szOk   = (BYTE*) "++++";
static BYTE * szErr2 = (BYTE*) "-002";
static BYTE * szErr3 = (BYTE*) "-003";
static BYTE * szErr4 = (BYTE*) "-004";

static char * pDataPath = NULL;

ULONG lfc_MilliSec( void )
{
#if defined(HB_OS_WIN_32)
   SYSTEMTIME st;
   GetLocalTime( &st );
   return (st.wMinute * 60 + st.wSecond) * 1000 + st.wMilliseconds;
#elif ( defined( HB_OS_LINUX ) || defined( HB_OS_BSD ) ) && !defined( __WATCOMC__ )
   struct timeval tv;
   gettimeofday( &tv, NULL );
   return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#else
   struct timeb tb;
   ftime( &tb );
   return tb.time * 1000 + tb.millitm;
#endif
}

void lfc_profile( int iMode, int i1, ULONG ** parr1, ULONG ** parr2 )
{
   static ULONG arr1[10];
   static ULONG arr2[10];
   static ULONG arr3[10];
   ULONG ul = lfc_MilliSec();

   switch( iMode )
   {
      case 0:
         memset( arr1,0,sizeof(ULONG)*10 );
         memset( arr2,0,sizeof(ULONG)*10 );
         memset( arr3,0,sizeof(ULONG)*10 );
         break;
      case 1:
         arr3[i1] = ul;
         break;
      case 2:
         if( arr3[i1] != 0 && ul >= arr3[i1] )
         {
            arr1[i1] += ul - arr3[i1];
            arr2[i1] ++;
            arr3[i1] = 0;
         }
         break;
      case 3:
         *parr1 = arr1;
         *parr2 = arr2;
         break;
   }
}

void lfc_addModule( char * szModname, USHORT uiItems )
{
   PMODSTRU pMStru = s_modules + uiModulesCurr;
   USHORT uiLen;

   if( uiModulesCurr == uiModulesAlloc )
   {
      s_modules = (MODSTRU*) hb_xrealloc( s_modules, sizeof(MODSTRU) * ( uiModulesAlloc+MODULES_REALLOC ) );
      memset( s_modules+uiModulesAlloc, 0, sizeof(MODSTRU) * MODULES_REALLOC );
      pMStru = s_modules + uiModulesAlloc;
      uiModulesAlloc += MODULES_REALLOC;
   }
   uiModulesCurr ++;

   uiLen = (USHORT) strlen( szModname );
   pMStru->szModname = (char*) hb_xgrab( uiLen+1 );
   memcpy( pMStru->szModname, szModname, uiLen );
   pMStru->szModname[uiLen] = '\0';

   pMStru->uiItems = uiItems;
   pMStru->ui = 0;

   pMStru->pMitemStru = (MITEMSTRU*) hb_xgrab( sizeof(MITEMSTRU) * pMStru->uiItems );
   memset( pMStru->pMitemStru, 0, sizeof(MITEMSTRU) * pMStru->uiItems );
}

void lfc_addItem2Module( char * szCln, char * szSrv, USHORT nSystem )
{
   PMODSTRU pMStru = s_modules + uiModulesCurr - 1;
   PMITEMSTRU pMiStru = pMStru->pMitemStru + pMStru->ui;
   USHORT uiLen;

   uiLen = (USHORT) strlen( szCln );
   pMiStru->szClnName = (char*) hb_xgrab( uiLen+1 );
   memcpy( pMiStru->szClnName, szCln, uiLen );
   pMiStru->szClnName[uiLen] = '\0';

   if( szSrv )
   {
      uiLen = (USHORT) strlen( szSrv );
      pMiStru->szSrvName = (char*) hb_xgrab( uiLen+1 );
      memcpy( pMiStru->szSrvName, szSrv, uiLen );
      pMiStru->szSrvName[uiLen] = '\0';
   }

   pMiStru->nSystem = nSystem;

   pMStru->ui ++;

}

HB_FUNC( LFC_SETAPPOPTIONS )
{
   USHORT uiLen;

   if( !ISNIL(1) )
   {
      uiLen = (USHORT) hb_parclen(1);
      pDataPath = (char*) hb_xgrab( uiLen+1 );
      memcpy( pDataPath, hb_parc(1), uiLen );
      pDataPath[uiLen] = '\0';
   }

}

HB_FUNC( LFC_READMEMAREA )
{
   char * pBuffer = (char*) hb_xgrab( hb_parni(2)+1 );
   if( !lfc_ReadMemArea( pBuffer, hb_parni(1), hb_parni(2) ) )
   {
      hb_xfree( pBuffer );
      hb_ret();
   }
   else
      hb_retc_buffer( pBuffer );
}

HB_FUNC( LFC_WRITEMEMAREA )
{
   lfc_WriteMemArea( hb_parc(1), hb_parni(2), hb_parni(3) );
}

HB_FUNC( GETCMDITEM )
{
   PHB_ITEM pText = hb_param( 1, HB_IT_STRING );
   PHB_ITEM pStart = hb_param( 2, HB_IT_NUMERIC );

   if( pText && pStart )
   {
      ULONG ulSize = hb_itemGetCLen( pText );
      ULONG ulStart = pStart ? hb_itemGetNL( pStart ) : 1;
      ULONG ulEnd = ulSize, ulAt;
      char *Text = hb_itemGetCPtr( pText );

      if ( ulStart > ulSize ) 
      {
         hb_retc( NULL );
         if ISBYREF( 3 ) hb_storni( 0, 3 );
      }
      else
      {
         ulAt = hb_strAt( ";", 1, Text + ulStart - 1, ulEnd - ulStart + 1 );
         if ( ulAt > 0) 
            ulAt += ( ulStart - 1 );

         if ISBYREF( 3 ) 
            hb_storni( ulAt, 3 );

         if ( ulAt != 0) 
         {
            ULONG ulLen = ulAt - ulStart;

            if( ulStart ) ulStart--;

            if( ulStart < ulSize )
            {
               if( ulLen > ulSize - ulStart )
               {
                  ulLen = ulSize - ulStart;
               }

               if( ulLen > 0 )
               {
                  if( ulLen == ulSize )
                     hb_itemReturn( pText );
                  else
                     hb_retclen( Text + ulStart, ulLen );
               }
               else
                  hb_retc( NULL );
            }
            else
               hb_retc( NULL );
         }
         else
            hb_retc( NULL );
      }
   }
   else
      hb_errRT_BASE_SubstR( EG_ARG, 1108, NULL, "AT", HB_ERR_ARGS_BASEPARAMS );
}

static int lfc_GetParam( BYTE *szData, BYTE **pp2, BYTE **pp3, BYTE **pp4, BYTE **pp5 )
{
   BYTE * ptr;
   int iRes = 0;

   if( ( ptr = (BYTE*) strchr( (char*) szData, ';' ) ) != NULL )
   {
      iRes ++;
      if( pp2 )
      {
         *pp2 = ++ptr;
         if( ( ptr = (BYTE*) strchr( (char*)*pp2, ';' ) ) != NULL )
         {
            iRes ++;
            if( pp3 )
            {
               *pp3 = ++ptr;
               if( ( ptr = (BYTE*) strchr( (char*)*pp3, ';' ) ) != NULL )
               {
                  iRes ++;
                  if( pp4 )
                  {
                     *pp4 = ++ptr;
                     if( ( ptr = (BYTE*) strchr( (char*)*pp4, ';' ) ) != NULL )
                     {
                        iRes ++;
                        if( pp5 )
                        {
                           *pp5 = ++ptr;
                           if( ( ptr = (BYTE*) strchr( (char*)*pp5, ';' ) ) != NULL )
                           {
                              iRes ++;
                           }
                        }
                     }
                  }
               }
            }
         }
      }
   }

   return iRes;
}

void lfc_CloseUS( PUSERSTRU pUStru )
{

   hb_ip_rfd_clr( pUStru->hSocket );
   hb_ipclose( pUStru->hSocket );
   pUStru->hSocket = 0;

   if( pUStru->pBuffer )
   {
      hb_xfree( pUStru->pBuffer );
      pUStru->pBuffer = NULL;
   }
   if( pUStru->szAddr )
   {
      hb_xfree( pUStru->szAddr );
      pUStru->szAddr = NULL;
   }
   if( pUStru->szVersion )
   {
      hb_xfree( pUStru->szVersion );
      pUStru->szVersion = NULL;
   }
   if( pUStru->szNetname )
   {
      hb_xfree( pUStru->szNetname );
      pUStru->szNetname = NULL;
   }
   if( pUStru->szExename )
   {
      hb_xfree( pUStru->szExename );
      pUStru->szExename = NULL;
   }

   uiUsersCurr --;
}

PUSERSTRU lfc_InitUS( HB_SOCKET_T hSocket )
{
   PUSERSTRU pUStru = s_users;
   USHORT ui = 0;

   while( ui < uiUsersAlloc && pUStru->pBuffer )
   {
     pUStru ++;
     ui ++;
   }
   if( ui == uiUsersAlloc )
   {
      s_users = (USERSTRU*) hb_xrealloc( s_users, sizeof(USERSTRU) * ( uiUsersAlloc+USERS_REALLOC ) );
      memset( s_users+uiUsersAlloc, 0, sizeof(USERSTRU) * USERS_REALLOC );
      pUStru = s_users + uiUsersAlloc;
      uiUsersAlloc += USERS_REALLOC;
   }
   pUStru->hSocket = hSocket;
   pUStru->ulBufferLen = HB_SENDRECV_BUFFER_SIZE;
   pUStru->pBuffer = (BYTE*) hb_xgrab( pUStru->ulBufferLen );

   if( ++ui > uiUsersMax )
      uiUsersMax = ui;
   uiUsersCurr ++;

   return pUStru;
}

HB_FUNC( LFC_CREATEDATA )
{
   uiUsersAlloc = INIT_USERS_ALLOC;
   s_users = (USERSTRU*) hb_xgrab( sizeof(USERSTRU) * uiUsersAlloc );
   memset( s_users, 0, sizeof(USERSTRU) * uiUsersAlloc );

   uiModulesAlloc = INIT_MODULES_ALLOC;
   s_modules = (MODSTRU*) hb_xgrab( sizeof(MODSTRU) * uiModulesAlloc );
   memset( s_modules, 0, sizeof(MODSTRU) * uiModulesAlloc );

   lStartDate = lfc_Date();
   dStartsec  = hb_dateSeconds();
}

void lfc_releaseModules( void )
{
   PMODSTRU pMStru = s_modules;
   PMITEMSTRU pMiStru;
   USHORT ui, ui1;

   if( pMStru )
   {
      for( ui=0; ui<uiModulesCurr; ui++,pMStru++ )
      {
         hb_xfree( pMStru->szModname );
         pMStru->szModname = NULL;
         if( pMStru->pMitemStru )
         {
            for( ui1=0,pMiStru=pMStru->pMitemStru; ui1<pMStru->uiItems; ui1++,pMiStru++ )
            {
               if( pMiStru->szClnName )
                  hb_xfree( pMiStru->szClnName );
               if( pMiStru->szSrvName )
                  hb_xfree( pMiStru->szSrvName );
               pMiStru->szClnName = pMiStru->szSrvName = NULL;
            }
            hb_xfree( pMStru->pMitemStru );
            pMStru->pMitemStru = NULL;
         }
      }
      uiModulesCurr = 0;
    
   }

}

HB_FUNC( LFC_RELEASEDATA )
{
   PUSERSTRU pUStru = s_users;
   USHORT ui;

   // writelog( "release-0",0);
   if( pUStru )
   {
      for( ui=0; ui<uiUsersAlloc; ui++,pUStru++ )
      {
         if( pUStru->pBuffer )
            lfc_CloseUS( pUStru );
      }
      
      hb_xfree( s_users );
      s_users = NULL;
   }
   // writelog( "release-1",0);
   lfc_releaseModules();
   if( s_modules )
   {
      hb_xfree( s_modules );
      s_modules = NULL;
   }

   // writelog( "release-2",0);
   if( pDataPath )
   {
      hb_xfree( pDataPath );
      pDataPath = NULL;
   }
   // writelog( "release-10",0);

}

static void lfc_SendAnswer( PUSERSTRU pUStru, BYTE* szData, ULONG ulLen )
{

   if( (((int)*szData) & 0xFF) >= 10 && *(szData+ulLen-1) != '\n' )
   {
      if( pUStru->ulBufferLen < ulLen+2 )
      {
         pUStru->ulBufferLen = ulLen + 2;
         pUStru->pBuffer = (BYTE*) hb_xrealloc( pUStru->pBuffer,pUStru->ulBufferLen );
      }
      memcpy( pUStru->pBuffer, szData, ulLen );
      *(pUStru->pBuffer+ulLen) = '\r';
      *(pUStru->pBuffer+ulLen+1) = '\n';
      *(pUStru->pBuffer+ulLen+2) = '\0';
      ulLen += 2;
      hb_ipSend( pUStru->hSocket, (char*)pUStru->pBuffer, ulLen, -1 );
   }
   else
      hb_ipSend( pUStru->hSocket, (char*)szData, ulLen, -1 );
   // writelog(pUStru->pBuffer, 0);
   ulBytesSent += ulLen;

}

static void lfc_Mgmt( PUSERSTRU pUStru, BYTE* szData )
{
   BYTE * ptr;
   BYTE * pData;
   BOOL bShow;
   BYTE * pp1, * pp2;
   int nParam = lfc_GetParam( szData, &pp1, &pp2, NULL, NULL );

   if( !nParam || *szData != '0' )
   {
      lfc_SendAnswer( pUStru, szErr2, 4 );
   }
   else
   {
      switch( *(szData+1) )
      {
         case '0':
         {
            char s[90];
            sprintf( s,"+%d;%d;%d;%d;%lu;%lu;%lu;%lu;%lu;%lu;",
               uiUsersCurr,uiUsersMax,0,0,
               (lfc_Date()-lStartDate)*86400+(long)(hb_dateSeconds()-dStartsec),
               ulOperations,ulBytesSent,ulBytesRead,0,0 );
               lfc_SendAnswer( pUStru, (BYTE*)s, strlen(s) );
            break;
         }
         default:
         {
            lfc_SendAnswer( pUStru, szErr2, 4 );
         }
      }

   }
   uiAnswerSent = 1;
}

static void lfc_Intro( PUSERSTRU pUStru, BYTE* szData )
{
   BYTE * pData;
   BYTE * pp1, * pp2, * pp3, * ptr;
   int nParam = lfc_GetParam( szData, &pp1, &pp2, &pp3, &ptr );

   if( nParam < 3 )
      pData = szErr2;
   else
   {
      if( pUStru->szVersion )
         hb_xfree( pUStru->szVersion );
      pUStru->szVersion = (BYTE*) hb_xgrab( pp1 - szData );
      memcpy( pUStru->szVersion, szData, pp1 - szData - 1 );
      pUStru->szVersion[pp1-szData-1] = '\0';
      pUStru->nSystem = ( *(pUStru->szVersion) == 'l' )? 1 : ( ( *(pUStru->szVersion) == 'w' )? 2 : 0 );

      if( pUStru->szNetname )
         hb_xfree( pUStru->szNetname );
      pUStru->szNetname = (BYTE*) hb_xgrab( pp2 - pp1 );
      memcpy( pUStru->szNetname, pp1, pp2-pp1-1 );
      pUStru->szNetname[pp2-pp1-1] = '\0';

      if( pUStru->szExename )
         hb_xfree( pUStru->szExename );
      pUStru->szExename = (BYTE*) hb_xgrab( pp3 - pp2 );
      memcpy( pUStru->szExename, pp2, pp3-pp2-1 );
      pUStru->szExename[pp3-pp2-1] = '\0';

      pData = szOk;
   }
   lfc_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

static void lfc_Flist( PUSERSTRU pUStru, BYTE* szData )
{
   BYTE * pData;
   BYTE * ptr;
   int nParam = lfc_GetParam( szData, &ptr, NULL, NULL, NULL );

   if( nParam < 1 )
      pData = szErr2;
   else
   {
      BYTE szModname[32];
      PMODSTRU pMStru = s_modules;
      USHORT ui;

      if( uiUsersCurr < 2 )
         lfc_checkIni();

      memcpy( szModname, szData, ptr-szData-1 );
      szModname[ptr-szData-1] = '\0';

      for( ui=0; ui<uiModulesCurr; ui++,pMStru++ )
      {
         if( !strcmp( szModname, pMStru->szModname ) )
         {
            pUStru->pMStru = pMStru;
            break;
         }
      }
      if( ui<uiModulesCurr )
      {
         BYTE * ptr;
         USHORT ui, uiItems, uiLen;
         ULONG ulLen = 10;
         double dTime;
         BYTE szFile[_POSIX_PATH_MAX + 1];
         PMITEMSTRU pMiStru;

         for( ui=0,uiItems=0; ui<pMStru->uiItems; ui++ )
         {
            pMiStru = pMStru->pMitemStru+ui;
            if( pMiStru->nSystem == 0 || pMiStru->nSystem == pUStru->nSystem )
            {
               ulLen += strlen( pMiStru->szClnName ) + 16;
               uiItems ++;
            }
         }

         pData = (BYTE*) malloc( ulLen );
         modf( hb_dateSeconds(), &dTime );
         sprintf( pData, "+%lu;%u;", (LONG)dTime, uiItems );
         // writelog(pData, 0);
         ptr = pData + strlen( pData );

         for( ui=0; ui<pMStru->uiItems; ui++ )
         {
            pMiStru = pMStru->pMitemStru+ui;
            /*
            writelog( "flist-1",0);
            writelog( pUStru->szVersion,0);
            writelog( (pMiStru->nSystem==0)? "0":((pMiStru->nSystem==1)? "1":"2") ,0);
            */
            if( pMiStru->nSystem == 0 || pMiStru->nSystem == pUStru->nSystem )
            {
               memcpy( ptr,pMiStru->szClnName,
                       strlen(pMiStru->szClnName) );
               ptr += strlen( pMiStru->szClnName );
               *ptr++ = ';';
               if( pMiStru->szSrvName )
               {
                  if( pDataPath )
                  {
                     uiLen = strlen( pDataPath );
                     memcpy( szFile, pDataPath, uiLen );
                  }
                  else
                     uiLen = 0;
                  memcpy( szFile+uiLen, pMiStru->szSrvName, strlen( pMiStru->szSrvName ) );
                  szFile[strlen(pMiStru->szSrvName)+uiLen] = '\0';

                  lfc_fileInfo( szFile );
                  sprintf( ptr, "%lu;%lu;", lfc_filedate(), lfc_filetime() );
               }
               else
                  sprintf( ptr, "%lu;%lu;", 0, 0 );
               ptr += strlen( ptr );
            }
         }
         sprintf( ptr, "%lu;", lfc_Date() );
         ptr += strlen( ptr );

         lfc_SendAnswer( pUStru, pData, ptr - pData );
         uiAnswerSent = 1;
         free( pData );
         return;
      }
      else
      {
         pData = szErr3;
      }
   }
   lfc_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}

static void lfc_Fget( PUSERSTRU pUStru, BYTE* szData )
{
   BYTE * pData = NULL;
   BYTE * pp1, * ptr;
   BYTE szFile[_POSIX_PATH_MAX + 1];
   FHANDLE fhnd;
   USHORT uiLen;
   int nParam = lfc_GetParam( szData, &pp1, &ptr, NULL, NULL );

   if( nParam < 2 )
      pData = szErr2;
   else
   {
      if( pDataPath )
      {
         uiLen = strlen( pDataPath );
         memcpy( szFile, pDataPath, uiLen );
      }
      else
         uiLen = 0;
      
      if( *szData == 'n' )
      {
         LONG lNum, l, lReal;
         PMODSTRU pMStru = pUStru->pMStru;
         PMITEMSTRU pMiStru;
         if( pMStru && pMStru->pMitemStru )
         {
            sscanf( pp1, "%d", &lNum );
            if( lNum && lNum <= pMStru->uiItems )
            {
               for( l=0,lReal=0; l<pMStru->uiItems; l++ )
               {
                  pMiStru = pMStru->pMitemStru + l;
                  if( pMiStru->nSystem == 0 || pMiStru->nSystem == pUStru->nSystem )
                  {
                     lReal ++;
                     if( lReal == lNum )
                     {
                        memcpy( szFile+uiLen, pMiStru->szSrvName, strlen( pMiStru->szSrvName ) );
                        szFile[strlen(pMiStru->szSrvName)+uiLen] = '\0';
                        break;
                     }
                  }
               }
               if( lReal != lNum )
                  pData = szErr4;
            }
            else
               pData = szErr4;
         }
         else
            pData = szErr3;
      }
      else
      {
         memcpy( szFile+uiLen, pp1, ptr-pp1-1 );
         szFile[ptr-pp1-1+uiLen] = '\0';
      }

      if( !pData )
      {
         ULONG ul, ul1, ul2;
         char s[256];
#if defined( HB_OS_WIN_32 )
         for( ptr = szFile; *ptr; ptr++ )
            if( *ptr == '/' )
               *ptr = '\\';
#else
         for( ptr = szFile; *ptr; ptr++ )
            if( *ptr == '\\' )
               *ptr = '/';
#endif
       ul = lfc_MilliSec();
         fhnd = hb_fsOpen( szFile, FO_READ | FO_SHARED );
       ul1 = lfc_MilliSec();
         if( fhnd != FS_ERROR )
         {
            ULONG ulLen = hb_fsSeek( fhnd, 0, FS_END );

            if( ulLen != 0 )
            {
               BYTE * pBuffer = ( BYTE * ) hb_xgrab( ulLen + 10 );
               char szTime[ 9 ];
               int iYear, iMonth, iDay;

               hb_fsSeek( fhnd, 0, FS_SET );
               pData = pBuffer+8;
               hb_fsReadLarge( fhnd, pData, ulLen );
               ul2 = lfc_MilliSec();
               *(--pData) = '+';
               ulLen ++;
               pData = lfc_AddLen( pData, &ulLen, 1 );
               lfc_SendAnswer( pUStru, pData, ulLen );
               hb_dateToday( &iYear, &iMonth, &iDay );
               hb_dateTimeStr( szTime );
               sprintf( s,"%02d.%02d %s %s %lu %lu\t%lu\t%s",iMonth,iDay,szTime,pUStru->szAddr,ul1-ul,ul2-ul1,lfc_MilliSec()-ul2,szFile );
               writelog( s,0 );
               uiAnswerSent = 1;
               hb_xfree( pBuffer );
               return;
            }
            else
               pData = szErr4;

            hb_fsClose( fhnd );
         }
      }
   }
   lfc_SendAnswer( pUStru, pData, 4 );
   uiAnswerSent = 1;

}


void lfc_ScanUS( void )
{
   PUSERSTRU pUStru;
   USHORT ui, uiLenLen;
   ULONG ulLen;
   int iLen;
   BOOL bRes;

   for( ui=0,pUStru=s_users; ui<uiUsersMax; ui++,pUStru++ )
   {
      if( pUStru->pBuffer && hb_ip_rfd_isset( pUStru->hSocket ) )
      {
         if( pUStru->ulBufferLen - pUStru->ulStartPos < HB_SENDRECV_BUFFER_SIZE )
         {
            pUStru->ulBufferLen += HB_SENDRECV_BUFFER_SIZE;
            pUStru->pBuffer = (BYTE*) hb_xrealloc( pUStru->pBuffer,pUStru->ulBufferLen );
         }
         ulLen = 0;
         iLen = hb_ipRecv( pUStru->hSocket, (char*)pUStru->pBuffer + pUStru->ulStartPos,
                     HB_SENDRECV_BUFFER_SIZE );
         if( !hb_iperrorcode() )
         {
            pUStru->ulStartPos += iLen;
            ulBytesRead += iLen;

            if( ( uiLenLen = (((int)*pUStru->pBuffer) & 0xFF) ) < 10 &&
                       pUStru->ulStartPos > (ULONG)uiLenLen )
               ulLen = lfc_b2n( (unsigned char *)(pUStru->pBuffer+1), uiLenLen );
            if( ( ulLen > 0 && ulLen+uiLenLen+1 <= pUStru->ulStartPos ) ||
                  ( uiLenLen >= 10 &&
                  *(pUStru->pBuffer+pUStru->ulStartPos-1) == '\n' &&
                  *(pUStru->pBuffer+pUStru->ulStartPos-2) == '\r' ) )
            {
               BYTE * ptr;

               ulOperations ++;
               if( ( uiLenLen = (((int)*pUStru->pBuffer) & 0xFF) ) < 10 )
               {
                  ptr = pUStru->pBuffer + uiLenLen + 1;
                  ulLen = pUStru->ulStartPos - uiLenLen - 1;
               }
               else
               {
                  ptr = pUStru->pBuffer;
                  ulLen = pUStru->ulStartPos - 2;
               }
               *(ptr+ulLen) = '\0';
               if( *ptr == 'i' && !strncmp( (char*)ptr,"intro;",6 ) )
                  lfc_Intro( pUStru, ptr+6 );
               else if( *ptr == 'f' )
               {
                  if( !strncmp( (char*)ptr,"flist;",6 ) )
                     lfc_Flist( pUStru, ptr+6 );
                  else if( !strncmp( (char*)ptr,"fget;",5 ) )
                     lfc_Fget( pUStru, ptr+5 );
               }
               else if( *ptr == 'm' && !strncmp( (char*)ptr,"mgmt;",5 ) )
                  lfc_Mgmt( pUStru, ptr+5 );
               else if( *ptr == 'p' && !strncmp( (char*)ptr,"ping;",5 ) )
                  lfc_SendAnswer( pUStru, pUStru->pBufRead, pUStru->ulDataLen );
               else if( *ptr == 'q' && !strncmp( (char*)ptr,"quit;",5 ) )
                  lfc_CloseUS( pUStru );
               else
                  lfc_SendAnswer( pUStru, (BYTE*)"-001", 4 );
               pUStru->ulStartPos = 0;
            }
         }
         else
         {
            // Socket error while reading
            lfc_CloseUS( pUStru );
         }
      }
   }
}

HB_FUNC( LFC_SERVER )
{
   char szBuffer[32];
   HB_SOCKET_T incoming;
   long int lTemp;

   static double dSec4MA = 0;
   double dSec;

   hb_ip_rfd_zero();

   iServerPort = hb_parni(1);
   if( ( hSocketMain = hb_ipServer( iServerPort, NULL, 10 ) ) == -1 )
      return;
   hb_ip_rfd_set( hSocketMain );

   while( TRUE )
   {
      if( hb_ip_rfd_select( 1 ) > 0 )
      {
         if( hb_ip_rfd_isset( hSocketMain ) )
         {
            incoming = hb_ipAccept( hSocketMain, -1, szBuffer, &lTemp );
            ulOperations ++;
            if( !hb_iperrorcode() )
            {
               PUSERSTRU pUStru = lfc_InitUS( incoming );

               hb_ip_rfd_set( incoming );

               lTemp = strlen( szBuffer );
               pUStru->szAddr = (BYTE*) hb_xgrab( lTemp + 1 );
               memcpy( pUStru->szAddr, szBuffer, lTemp );
               pUStru->szAddr[lTemp] = '\0';
               lfc_SendAnswer( pUStru, (BYTE*)szRelease, strlen(szRelease) );
            }
         }
         lfc_ScanUS();
      }
#ifdef __CONSOLE__
      if( hb_inkey( FALSE,0,hb_set.HB_SET_EVENTMASK ) == 27 )
         break;
#endif
      lfc_ReadMemArea( szBuffer, 0, 1 );
      if( *szBuffer == '0' )
      {
         // writelog( "break",0);
         break;
      }
      if( ( dSec = hb_numInt( hb_dateSeconds() ) ) - dSec4MA > 1 )
      {
         dSec4MA = dSec;
         sprintf( szBuffer, "%06lu%03d", (ULONG)dSec, uiUsersCurr );
         lfc_WriteMemArea( szBuffer, 1, 9 );
      }

   }
}
