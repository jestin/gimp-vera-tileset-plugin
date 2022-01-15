GIMPTOOL = gimptool-2.0

PROGRAM = vera_tileset
UI_FILE = plug-in-file-vera.ui

$(PROGRAM):
	$(GIMPTOOL) --build vera_tileset.c

install: $(PROGRAM)
	$(GIMPTOOL) --install-bin $(PROGRAM)

install-ui:
	cp $(UI_FILE) `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(UI_FILE)

uninstall: $(PROGRAM)
	$(GIMPTOOL) --uninstall-bin $(PROGRAM)

all: install

run: all
	gimp

tags:
	ctags * --recurse

clean:
	rm -f *.o $(PROGRAM)

