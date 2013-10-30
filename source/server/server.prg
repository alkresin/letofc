/*  $Id: server.prg,v 1.80 2008/07/06 15:36:38 ptsarenko Exp $  */

/*
 * Harbour Project source code:
 * Leto file check server
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

#include "hbclass.ch"
#include "dbstruct.ch"
#include "rddsys.ch"
#include "common.ch"
#include "dbinfo.ch"
#include "fileio.ch"

Memvar oApp

#ifdef __LINUX__
ANNOUNCE HB_GTSYS
REQUEST HB_GT_STD_DEFAULT
#else
#ifndef __CONSOLE__
#ifndef __XHARBOUR__
ANNOUNCE HB_GTSYS
REQUEST HB_GT_GUI_DEFAULT
#endif
#endif
#endif

#ifdef __CONSOLE__
Procedure Main
Local nRes

   CLS
   IF ( nRes := lfc_CreateMemArea() ) == -1
      ? "Initialization Error ..."
      Return
   ENDIF
   @ 1, 5 SAY "Server listening ..."
   @ 2, 5 SAY "Press [ESC] to terminate the program"

   StartServer()

Return
#endif

#ifdef __WIN_DAEMON__
Procedure Main( cCommand )
Local nRes, i, arr

   IF ( nRes := lfc_CreateMemArea() ) == -1
      MsgStop( "Initialization Error ..." )
      Return
   ENDIF
   IF cCommand != Nil .AND. Lower( cCommand ) == "stop"
      IF nRes > 0
         MsgStop( "Server isn't found..." )
      ELSE
         lfc_WriteMemArea( '0', 0, 1 )
      ENDIF
      Return
   ELSE
      IF nRes == 0
         MsgStop( "Server already running!" )
         Return
      ENDIF    
   ENDIF

   StartServer()

Return
#endif

#ifdef __LINUX_DAEMON__
Procedure Main( cCommand )
Local nRes, i, arr

   IF cCommand != Nil .AND. Lower( cCommand ) == "stop"

      IF ( nRes := lfc_CreateMemArea() ) == -1
         WrLog( "Initialization Error" )
         Return
      ENDIF
      IF nRes > 0
         WrLog( "Server isn't found" )
      ELSE
         lfc_WriteMemArea( '0', 0, 1 )
      ENDIF
      Return

   ELSE
      IF !lfc_Daemon()       
         WrLog( "Can't become a daemon" )
         Return
      ENDIF
      IF ( nRes := lfc_CreateMemArea() ) == -1
         WrLog( "Initialization Error" )
         Return
      ENDIF
      IF nRes == 0
         IF cCommand != Nil .AND. Lower( cCommand ) == "force"
            lfc_SetAreaNew()
            lfc_WriteMemArea( 'F', 0, 1 )
         ELSE
            WrLog( "Server already running!" )
            Return
         ENDIF
      ENDIF    
   ENDIF

   StartServer()

Return
#endif


Procedure StartServer()
Local Socket, ns, oSock, aSock
PUBLIC oApp

   REQUEST DBFCDX
   RDDSETDEFAULT( "DBFCDX" )
   SET AUTOPEN ON

   oApp := HApp():New()

   lfc_CreateData()
   InitSrv()

   WrLog( "Leto FC Server has been started." )

   IF oApp:lLower
      SET( _SET_FILECASE,1 )
      SET( _SET_DIRCASE,1 )
   ENDIF

   lfc_Server( oApp:nPort )

   lfc_ReleaseData()
   hb_ipCleanup()
   lfc_CloseMemArea()

   WrLog( "Server has been closed." )

RETURN

Static Function InitSrv
Local cIniName := "letofc.ini"
Local aIni, i, j, j1, cModule, cFile, nPos, aFiles
Local nPort

#ifdef __LINUX__
   IF File( cIniName )
      aIni := rdIni( cIniName )
   ELSEIF File( "/etc/" + cIniName )
      aIni := rdIni( "/etc/" + cIniName )
   ELSE
      Return Nil
   ENDIF
#else
   aIni := rdIni( cIniName )
#endif
   IF !Empty( aIni )
      FOR i := 1 TO Len( aIni )
         IF aIni[i,1] == "MAIN"
            FOR j := 1 TO Len( aIni[i,2] )
               IF aIni[i,2,j,1] == "PORT"
                  IF ( nPort := Val( aIni[i,2,j,2] ) ) >= 2000
                     oApp:nPort := nPort
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "DATAPATH"
                  lfc_SetAppOptions( aIni[i,2,j,2] )
               ELSEIF aIni[i,2,j,1] == "LOG"
                  IF GetExten( aIni[i,2,j,2] ) == "log"
                     oApp:LogFile := aIni[i,2,j,2]
                  ELSE
                     WrLog( "Wrong logfile extension" )
                  ENDIF
               ELSEIF aIni[i,2,j,1] == "LOWER_PATH"
                  oApp:lLower := ( aIni[i,2,j,2] == '1' )
               ENDIF
            NEXT
         /*
         ELSEIF aIni[i,1] == "MODULE"
            aFiles := {}
            FOR j := 1 TO Len( aIni[i,2] )
               IF aIni[i,2,j,1] == "NAME"
                  cModule := Lower( aIni[i,2,j,2] )
               ELSEIF aIni[i,2,j,1] == "FILE"
                  cFile := aIni[i,2,j,2]
                  IF ( nPos := At( ",", cFile ) ) == 0
                     Aadd( aFiles, { cFile, Nil } )
                  ELSE
                     Aadd( aFiles, { Left(cFile,nPos-1), Substr(cFile,nPos+1) } )
                  ENDIF
               ENDIF
            NEXT
            lfc_addModule( cModule,Len(aFiles) )
            FOR j1 := 1 TO Len( aFiles )
               lfc_addItem2module( aFiles[j1,1],aFiles[j1,2] )
            NEXT
         */
         ENDIF
      NEXT
   ENDIF
   
RETURN Nil

Static Function FilePath( fname )
LOCAL i
RETURN IIF( ( i := RAT( '\', fname ) ) = 0, ;
           IIF( ( i := RAT( '/', fname ) ) = 0, "", LEFT( fname, i ) ), ;
           LEFT( fname, i ) )

Static Function CutExten( fname )
Local i, j

   IF ( i := RAT( '.', fname ) ) == 0
      Return fname
   ELSEIF ( j := Max( RAT( '/', fname ), RAT( '\', fname ) ) ) > i
      Return fname
   ENDIF

Return Left( fname, i - 1 )

Static Function GetExten( fname )
Local i, j

   IF ( i := RAT( '.', fname ) ) == 0
      Return ""
   ELSEIF ( j := Max( RAT( '/', fname ), RAT( '\', fname ) ) ) > i
      Return ""
   ENDIF

Return Lower( Substr( fname, i + 1 ) )


INIT PROCEDURE INITP
   hb_ipInit()
Return

EXIT PROCEDURE EXITP

   lfc_ReleaseData()
   hb_ipCleanup()
   lfc_CloseMemArea()

Return

CLASS HApp

   DATA nPort     INIT 2912
   DATA LogFile   INIT ""
   DATA lLower    INIT .F.

   METHOD New
ENDCLASS

METHOD New CLASS HApp
Return Self

