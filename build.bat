@echo off

set CXXFLAGS=/Zi /O2 /GR- /nologo /FC /W4 /wd4310 /wd4100 /wd4201 /wd4505 /wd4996 /wd4127 /wd4510 /wd4512 /wd4610 /wd4457 /WX /FS
set LDFLAGS=/link /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:CONSOLE user32.lib kernel32.lib gdi32.lib Gdiplus.lib
set SOURCES=present.cpp main.cpp arena.cpp render_queue.cpp display_win32.cpp image_load.cpp qrcodegen.cpp

cl %CXXFLAGS% %SOURCES%  %LDFLAGS%