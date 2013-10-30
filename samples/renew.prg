
Function Main( cAddr, cModName, cPar )
Local nPos, cName, lSetTime

   IF cAddr == Nil .OR. cModName == Nil
      ? "Parameters missing ..."
      ?
      Return Nil
   ENDIF

   cAddr := "//" + cAddr
   IF !( Right( cAddr,1 ) $ "/\" )
      cAddr += "/"
   ENDIF

   ? "Connecting to cAddr"
   IF lfc_Connect( cAddr ) > 0
      lSetTime := ( cPar!=Nil .AND. Lower(cPar)=="/s" )
      DO WHILE ( nPos := At( ",", cModname ) ) > 0
         cName := Left( cModName,nPos-1 )
         cModName := Substr( cModName,nPos+1 )
         Renew( cName, lSetTime )
         lSetTime := .F.
      ENDDO
      Renew( cModName, lSetTime )
   ELSE
      ? "Can't connect to " + cAddr
   ENDIF
   ?

Return Nil

FUNCTION Renew( cModName, lSetTime )
Local aFiles, i, nDelta, lRes := .T., cExt, dDate, nMin, ns

   ? "Get the files list"
   IF ( aFiles := lfc_getFileList( Lower( cModName ) ) ) == Nil
      ? "List is empty ..."
      Return .F.
   ENDIF

   dDate := lfc_Long2Date(aFiles[3])
   ? dDate, Long2Time( aFiles[1] )
   IF lSetTime
      IF aFiles[3] > 0 .AND. Date() != dDate
         SetDate( dDate )
      ENDIF
      IF Abs( Seconds() - aFiles[1] ) > 10
         ns := aFiles[1]
#ifdef __LINUX__
         IF ns < 10800
            ns += 75600
         ELSE       
            ns -= 10800
         ENDIF
#endif
         nMin := Int( aFiles[1]/60 )
         SetTime( Long2Time( ns ) )
      ENDIF
   ENDIF
   nDelta := aFiles[1] - Int( Seconds() )
   FOR i := 1 TO Len( aFiles[2] )

      IF aFiles[ 2,i,2 ] == 0
         IF !lfc_isFileExist( aFiles[ 2,i,1 ] )
            ? "Makedir " + aFiles[ 2,i,1 ]
            MakeDir( aFiles[ 2,i,1 ] )
            ?? " - Ok"
         ENDIF
      ELSE
         IF lfc_IsFileOld( aFiles[ 2,i,1 ], aFiles[ 2,i,2 ], aFiles[ 2,i,3 ], nDelta )
            ? "Copy " + aFiles[ 2,i,1 ]
            cExt := GetExten( aFiles[ 2,i,1 ] )
            lRes := lfc_getfile( i, aFiles[ 2,i,1 ], ( Empty(cExt).OR.Lower(cExt)=="sh" ) )
            ?? Iif( lRes, " - Ok", " - Error" )
         ENDIF
      ENDIF
   NEXT

Return lRes

Static Function GetExten( cFile )
Local i1 := Rat( ".", cFile ), i2, i3

   IF i1 == 0 .OR. ( i2 := Rat( "/", cFile ) ) > i1  .OR. ( i3 := Rat( "\", cFile ) ) > i1
      Return ""
   ENDIF

Return Substr( cFile,i1+1 )

Static Function Long2Time( ns )
Local nMin := Int( ns/60 )
Return Padl( Ltrim(Str(Int(nMin/60),2)),2,'0' ) + ":" + ;
                  Padl( Ltrim(Str(nMin%60,2)),2,'0' ) + ":" + ;
                  Padl( Ltrim(Str(ns%60,2)),2,'0' )
