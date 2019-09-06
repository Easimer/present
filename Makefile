CFLAGS_X11=$(shell pkg-config x11-xcb --cflags)
LDFLAGS_X11=$(shell pkg-config x11-xcb --libs)

CXXFLAGS=$(CFLAGS_X11)
LDFLAGS=$(LDFLAGS_X11)

OBJECTS=main.o

all: present

present: $(OBJECTS)
	$(CXX) -o present $(OBJECTS) $(LDFLAGS)

clean:
	rm -f present $(OBJECTS)

.PHONY: clean
