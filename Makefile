# Builds the Fliqlo screen saver add-on on Haiku. On an x86_gcc2 hybrid the
# default g++ is gcc2, which is what the screen_blanker there loads.
NAME     = Fliqlo
CXX     ?= g++
CXXFLAGS = -O2 -Wall -Wno-multichar -fPIC
LIBS     = -lbe -lscreensaver

ADDONS = $(HOME)/config/non-packaged/add-ons/Screen Savers
FONTS  = $(HOME)/config/non-packaged/data/fonts/otfonts

$(NAME): src/Fliqlo.cpp
	$(CXX) $(CXXFLAGS) -shared -o $(NAME) src/Fliqlo.cpp $(LIBS)

# Then pick Fliqlo in ScreenSaver preferences.
install: $(NAME)
	mkdir -p "$(ADDONS)" "$(FONTS)"
	cp $(NAME) "$(ADDONS)/$(NAME)"
	cp fonts/texgyreheroscn-bold.otf "$(FONTS)/Fliqlo-texgyreheroscn-bold.otf"

# Renders the add-on into PNG files (test/render.cpp), for checking over ssh.
render: test/render.cpp
	$(CXX) -O2 -Wno-multichar -o render test/render.cpp -lbe -ltranslation -lscreensaver

clean:
	rm -f $(NAME) render

.PHONY: install clean
