# Config
WITHOUT_INTF_CURSES =
WITHOUT_INTF_GUI =


CXX = g++
CFLAGS = -g -Wall -Wextra -Werror
CFLAGS_WIN += -D_WIN32_WINNT=0x0501
# avoid to define the ERROR macro
CFLAGS_WIN += -DNOGDI
CFLAGS_POSIX = -D_XOPEN_SOURCE=500
LDFLAGS = -lprotobuf -lboost_system$(BOOST_VERSION_SUFFIX)
LDFLAGS_WIN = -lws2_32 -lmswsock -static-libgcc -static-libstdc++ -Wl,--enable-auto-import
LDFLAGS_POSIX = -lpthread

BOOST_VERSION_SUFFIX =
INKSCAPE = inkscape


src_dir = src

CFLAGS += -I$(src_dir)
cxx_names = netplay game instance config log optget main
PROTOBUF_NAME = netplay
TARGET = panettopon

BUILD_DIR = pnp-build
BUILD_WIN_DLL =

cxx_names += server client intf_server

ifndef WITHOUT_INTF_CURSES
cxx_names += intf_curses
LDFLAGS_WIN += -lpdcurses
LDFLAGS_POSIX += -lcurses
BUILD_WIN_DLL += pdcurses
else
CFLAGS += -DWITHOUT_INTF_CURSES
endif

ifndef WITHOUT_INTF_GUI
cxx_names += gui/intf gui/resources
LDFLAGS +=
CFLAGS += -DSFML_STATIC
# use static SFML: avoid an error on exit with ATI cards
LDFLAGS_WIN += -lopengl32 -lsfml-graphics-s -lsfml-window-s -lsfml-system-s
LDFLAGS_POSIX += -lsfml-graphics -lsfml-window -lsfml-system
CFLAGS_WIN += -DUSE_WINMAIN
BUILD_WIN_DLL += 
else
CFLAGS += -DWITHOUT_INTF_GUI
endif


ifeq ($(OS),Windows_NT)
CFLAGS += $(CFLAGS_WIN)
LDFLAGS += $(LDFLAGS_WIN)
RES := $(src_dir)/$(TARGET).res
TARGET := $(TARGET).exe
BOOST_VERSION_SUFFIX = -mgw45-mt-1_44
else
CFLAGS += $(CFLAGS_POSIX)
LDFLAGS += $(LDFLAGS_POSIX)
endif

.PHONY: default all resources clean mostlyclean

PROTOBUF_OBJ = $(src_dir)/$(PROTOBUF_NAME).pb.o
OBJS = $(patsubst %,$(src_dir)/%.o,$(cxx_names))


default: $(TARGET)

all: default resources

-include $(OBJS:.o=.d)

$(TARGET): $(PROTOBUF_OBJ) $(OBJS) $(RES)
	$(CXX) $(CFLAGS) $(OBJS) $(PROTOBUF_OBJ) $(RES) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -MMD -c $< -o $@

ifneq ($(RES),)
%.res: %.rc
	windres $< -O coff -o $@
endif


$(src_dir)/$(PROTOBUF_NAME).pb.cc: $(src_dir)/$(PROTOBUF_NAME).pb.h

$(src_dir)/$(PROTOBUF_NAME).pb.h: $(src_dir)/$(PROTOBUF_NAME).proto
	cd $(src_dir) && protoc --cpp_out=. $(PROTOBUF_NAME).proto

$(PROTOBUF_OBJ): $(src_dir)/$(PROTOBUF_NAME).pb.cc $(src_dir)/$(PROTOBUF_NAME).pb.h
	$(CXX) $(CFLAGS) -c $< -o $@


## resources

RES_DIR = res
RES_SVG_FILE = $(RES_DIR)/res.svg  # relative path
RES_SVG_OBJS =
RES_DPI = 45  # change default export ratio (90 is 1:1)

# $(call svg2png_rule,svg-id[,area])
define svg2png_rule
$(RES_DIR)/$(1).png : $(RES_SVG_FILE)
	$(INKSCAPE) $(PWD)/$(RES_SVG_FILE) -e $(PWD)/$$@ -j -i $(1) $(if $(2),-a $(2)) -d $(RES_DPI) -b black -y 0
RES_SVG_OBJS += $(RES_DIR)/$(1).png
endef

# define rules
$(eval $(call svg2png_rule,BkColor-map))
$(eval $(call svg2png_rule,BkGarbage-map))
$(eval $(call svg2png_rule,SwapCursor))
$(eval $(call svg2png_rule,Labels))
$(eval $(call svg2png_rule,GbWaiting-map))
$(eval $(call svg2png_rule,Field-Frame))


resources: $(RES_SVG_OBJS)

##

ICON_SIZES = 16 32 64

# $(call svg2icon_rule,size)
define svg2icon_rule
$(RES_DIR)/icon-$(1).png : $(RES_SVG_FILE)
	$(INKSCAPE) $(PWD)/$(RES_SVG_FILE) -e $(PWD)/$$@ -j -i App-icon -w $(1) -b black -y 0
ICON_FILES += $(RES_DIR)/icon-$(1).png
endef

$(foreach s,$(ICON_SIZES),$(eval $(call svg2icon_rule,$s)))

icons : $(ICON_FILES)

##

build: $(TARGET)
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/$(RES_DIR)
	cp -f $(TARGET) $(BUILD_DIR)/
	strip $(BUILD_DIR)/$(TARGET)
	cp -f panettopon.ini $(BUILD_DIR)/
ifeq ($(OS),Windows_NT)
	rm -f $(BUILD_DIR)/*.dll
	for dll in $(BUILD_WIN_DLL); do cp "`which $$dll.dll`" $(BUILD_DIR)/ ; done;
endif
	cp -f -r $(RES_DIR)/*.png $(RES_DIR)/*.ttf $(BUILD_DIR)/$(RES_DIR)
	zip -r -q -9 $(BUILD_DIR).zip $(BUILD_DIR)

##

clean:
	rm -f $(TARGET) $(OBJS) $(addprefix $(src_dir)/$(PROTOBUF_NAME).pb.,o cc h) $(OBJS:.o=.d) $(RES)

mostlyclean: clean
	rm -f $(RES_SVG_OBJS)


