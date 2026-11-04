
CROSS=
CC=$(CROSS)gcc
CXX=$(CROSS)g++
LD=$(CROSS)gcc

ifeq ($(shell uname -s),Darwin)
CFLAGS	+= -std=c++17 -O3 -fPIC  -fvisibility=hidden -DLIBPLOT_EXPORTS
LDFLAGS += -dynamiclib -lstdc++
EXT=dylib
else
CFLAGS	+= -std=c++17 -O -fPIC -fvisibility=hidden -DLIBPLOT_EXPORTS
LDFLAGS += -shared -lstdc++
EXT=so
endif

CXXFLAGS+= $(CFLAGS)
program=libplot
all: $(program).$(EXT)

$(program).$(EXT): $(program).o plot.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) *.o *.$(EXT)
