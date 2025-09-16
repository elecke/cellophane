CXX = clang++
CXXFLAGS = -Wall -Wextra -O3 -ffast-math -fmath-errno -march=native -mtune=native
LIBS = -lmimalloc

FLTK_CXXFLAGS = $(shell fltk-config --cxxflags)
FLTK_LDFLAGS = $(shell fltk-config --ldflags)
GTK_FLAGS = $(shell pkg-config --cflags --libs gtk+-3.0)

SOURCES = cellophane.cpp
SHIT = cellophane

all: $(SHIT)

$(SHIT): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(FLTK_CXXFLAGS) -o $@ $^ $(FLTK_LDFLAGS) $(GTK_FLAGS) $(LIBS)
	strip --strip-unneeded $(SHIT)

install: $(SHIT)
	install -Dm755 $(SHIT) /usr/local/bin/$(SHIT)

clean:
	rm -f $(SHIT)

.PHONY: all clean install
