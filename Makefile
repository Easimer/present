CFLAGS_X11=$(shell pkg-config x11-xcb xcb-image xcb-util cairo xcb-ewmh --cflags)
LDFLAGS_X11=$(shell pkg-config x11-xcb xcb-image xcb-util cairo xcb-ewmh --libs)

CXXFLAGS=$(CFLAGS_X11) -Wall -g -O0
LDFLAGS=$(LDFLAGS_X11)

OBJECTS=main.o arena.o render_queue.o present.o display.o

all: present

present: $(OBJECTS)
	$(CXX) -o present $(OBJECTS) $(LDFLAGS)

clean:
	rm -f present $(OBJECTS)

.PHONY: clean
