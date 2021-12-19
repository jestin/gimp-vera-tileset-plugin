GIMPTOOL = gimptool-2.0

PROGRAM = vera_tileset

$(PROGRAM):
	$(GIMPTOOL) --build vera_tileset.c

install: $(PROGRAM)
	$(GIMPTOOL) --install-bin $(PROGRAM)

all: install

run: all
	gimp

tags:
	ctags * --recurse

clean:
	rm -f *.o $(PROGRAM)

