@echo off
@set HB_INSTALL=c:\harbour\

%HB_INSTALL%\bin\harbour %1.prg -n -i%HB_INSTALL%\include

bcc32 -e%1.exe -O2 -d -I%HB_INSTALL%\include -L%HB_INSTALL%\lib -L..\lib debug.lib vm.lib rtl.lib gtwin.lib lang.lib rdd.lib dbfntx.lib dbffpt.lib hbsix.lib macro.lib pp.lib common.lib codepage.lib hbct.lib letofc.lib %1.c
del %1.c
del %1.obj
del %1.tds
