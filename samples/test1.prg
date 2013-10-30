
Function Main
local arr

   ? lfc_Connect( "//127.0.0.1:2912/" )
   arr := lfc_getFileList( "x1" )
   ? Valtype(arr)
   if arr != Nil
      ? Len( arr )
      ? valtype( arr[1] ), arr[1]
      ? valtype( arr[2] ), arr[2]
      ? len(arr[2]), valtype( arr[2,1] )
      ? arr[2,1,1], arr[2,1,2], arr[2,1,3]
      ? arr[2,2,1], arr[2,2,2], arr[2,2,3]
   endif
   ? "------------"
   ? lfc_getfile( 1,arr[2,1,1] )
   ? lfc_getfile( 3,arr[2,3,1] )
   // ? lfc_dateJulian( "\myapps\sale\blanks.txt" )
   ?
Return
