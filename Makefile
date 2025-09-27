GIMPTOOL = gimptool-3.1
GIMP = gimp-3.1
PLUGIN = vera_tileset
GCC = gcc
GIMPCFLAGS = $(shell ${GIMPTOOL} --cflags)
GIMPLIBS = $(shell ${GIMPTOOL} --libs)
WARNING_POLICY = -Wno-deprecated-declarations -w
XML2CFLAGS = $(shell xml2-config --cflags)
XML2LIBS = $(shell xml2-config --libs)
TILE_UI_FILE = plug-in-file-vera-tiles.ui
SELECTOR_UI_FILE = plug-in-file-vera-selector.ui
BITMAP_UI_FILE = plug-in-file-vera-bitmap.ui

all: $(PLUGIN)

$(PLUGIN): vera_tileset_3.o vera-lib.o
	$(GCC) vera_tileset_3.o vera-lib.o $(WARNING_POLICY) -o $(PLUGIN) $(GIMPLIBS) $(XML2LIBS)

vera_tileset_3.o: vera_tileset_3.c vera-lib.o
	$(GCC) -c $(GIMPCFLAGS) $(XML2CFLAGS) $(WARNING_POLICY) vera_tileset_3.c

vera-lib.o: vera-lib.c
	$(GCC) -c $(GIMPCFLAGS) $(XML2CFLAGS) $(WARNING_POLICY) vera-lib.c 

install: $(PLUGIN)
	$(GIMPTOOL) --install-bin $(PLUGIN)

install-ui:
	cp $(TILE_UI_FILE) `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(TILE_UI_FILE)
	cp $(SELECTOR_UI_FILE) `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(SELECTOR_UI_FILE)
	cp $(BITMAP_UI_FILE) `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(BITMAP_UI_FILE)

uninstall: $(PLUGIN)
	$(GIMPTOOL) --uninstall-bin $(PLUGIN)
	rm `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(TILE_UI_FILE)
	rm `$(GIMPTOOL) --gimpdatadir`/ui/plug-ins/$(SELECTOR_UI_FILE)

run: install
	$(GIMP)

tags:
	ctags * --recurse

clean:
	rm -f *.o $(PLUGIN)

