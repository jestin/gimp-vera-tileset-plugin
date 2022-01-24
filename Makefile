GIMPTOOL = gimptool-2.0
PROGRAM = vera_tileset
GCC = gcc
CFLAGS = $(shell gimptool-2.0 --cflags)
LIBS = $(shell gimptool-2.0 --libs)
UI_FILE = plug-in-file-vera.ui

$(PROGRAM):
	$(GCC) $(CFLAGS) -o $(PROGRAM) vera_tileset.c $(LIBS)

install: $(PROGRAM)
	$(GIMPTOOL) --install-bin $(PROGRAM)

install-ui:
	cp $(UI_FILE) `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(UI_FILE)

uninstall: $(PROGRAM)
	$(GIMPTOOL) --uninstall-bin $(PROGRAM)

all: $(PROGRAM)

run: install
	gimp

tags:
	ctags * --recurse

clean:
	rm -f *.o $(PROGRAM)

