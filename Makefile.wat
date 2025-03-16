#******************************************************************************
# @file             Makefile.wat
#******************************************************************************
SRCDIR              ?=  $(CURDIR)
VPATH               :=  $(SRCDIR)

SRC                 :=  append.c ar.c conv.c delete.c display.c extract.c lib.c ranlib.c replace.c report.c

all: sar.exe

sar.exe: $(SRC)

	wcl -fe=$@ $^

clean:

	for /r %%f in (*.obj) do ( if exist %%f ( del /q %%f ) )
	if exist sar.exe ( del /q sar.exe )
