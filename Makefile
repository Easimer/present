CFLAGS_X11=$(shell pkg-config x11-xcb cairo xcb-ewmh --cflags)
LDFLAGS_X11=$(shell pkg-config x11-xcb cairo xcb-ewmh --libs)

CXXFLAGS=$(CFLAGS_X11) -Wall -g -O0
LDFLAGS=$(LDFLAGS_X11)

OBJECTS=main.o arena.o render_queue.o present.o display_x11.o

all: present

present: $(OBJECTS)
	$(CXX) -o present $(OBJECTS) $(LDFLAGS)

clean:
	rm -f present $(OBJECTS)

install: present
	install -o root -g root -m 555 -s -v present /usr/local/bin/

uninstall:
	rm -v -f /usr/local/bin/present

.PHONY: clean install uninstall
