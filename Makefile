cflags	= /nologo /std:c++17 /W3 /EHsc /I. /DLIBPLOT_EXPORTS
lflags	= /nologo /INCREMENTAL:NO /DLL

program=plot
target=lib$(program)

all: rel
dbg:
	@$(MAKE) /nologo BUILD=$@ $(target).dll

rel:
	@$(MAKE) /nologo BUILD=$@ $(target).dll

!IF "$(BUILD)" == "dbg"
cflags = $(cflags) /Zi /Od /MDd /RTC1
!ELSE
cflags = $(cflags) /O2 /MD /Gy
!ENDIF

.cpp.obj:
	cl /c $(cflags) $< /Fo:$*.obj

$(target).dll: $(program).obj $$(@B).obj
	link $(lflags) /OUT:$@ $**

$(target).obj: $$(@B).cpp $$(@B).h
$(program).obj: $$(@B).cpp $$(@B).hpp $(target).h

demo.exe: $$(@B).cpp $(program).cpp
	cl /nologo /std:c++17 /W3 /EHsc /Zi /Od /MDd /RTC1 /I. /D_DEBUG $** /link /INCREMENTAL:NO /DEBUG user32.lib gdi32.lib

clean:
	del $(target).dll $(target).lib demo.exe *.exp *.obj *.ilk *.pdb
