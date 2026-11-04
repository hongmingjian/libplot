cflags	= /nologo /std:c++17 /EHsc /O2 /DLIBPLOT_EXPORTS /I.
lflags	= /nologo /DLL


program=libplot

all: $(program).dll

$(program).dll: $(program).obj plot.obj
	link $(lflags) /OUT:$@ $?

$(program).obj: $$(@B).cpp
	cl /c $(cflags) $?

plot.obj: $$(@B).cpp
	cl /c $(cflags) $?

clean:
	del $(program).dll $(program).lib $(program).exp $(program).obj plot.obj
