# Builds PowerCollage.ofx.bundle against the OpenFX SDK that ships with DaVinci Resolve.
#   make            build the bundle
#   make install    copy it to the system OpenFX folder (needs sudo)
#   make link       symlink the system OpenFX folder to this build (needs sudo once)
#   make release    copy the built bundle to ../Plugin for distribution

UNAME_SYSTEM := $(shell uname -s)

ifeq ($(UNAME_SYSTEM), Darwin)
	OFX_SDK ?= /Library/Application Support/Blackmagic Design/DaVinci Resolve/Developer/OpenFX
	OFX_PLUGINS ?= /Library/OFX/Plugins
	ARCH_FLAGS = -arch arm64 -arch x86_64
	PLATFORM_CXXFLAGS = $(ARCH_FLAGS) -mmacosx-version-min=11.0
	LDFLAGS = -bundle -fvisibility=hidden -framework Metal -framework AppKit $(ARCH_FLAGS) -mmacosx-version-min=11.0
	BUNDLE_DIR = PowerCollage.ofx.bundle/Contents/MacOS
	METAL_OBJ = MetalKernel.o
else
	OFX_SDK ?= /opt/resolve/Developer/OpenFX
	OFX_PLUGINS ?= /usr/OFX/Plugins
	PLATFORM_CXXFLAGS = -fPIC
	LDFLAGS = -shared -fvisibility=hidden
	BUNDLE_DIR = PowerCollage.ofx.bundle/Contents/Linux-x86-64
endif

# The SDK path contains spaces, which make cannot handle in rules, so it is
# reached through a symlink.
SDK = .ofxsdk
CXXFLAGS = -std=c++17 -O2 -fvisibility=hidden -I$(SDK)/OpenFX-1.4/include -I$(SDK)/Support/include -I. $(PLATFORM_CXXFLAGS)
SUPPORT_OBJS = ofxsCore.o ofxsImageEffect.o ofxsInteract.o ofxsLog.o ofxsMultiThread.o ofxsParams.o ofxsProperty.o ofxsPropertyValidation.o

all: PowerCollage.ofx

PowerCollage.ofx: PowerCollagePlugin.o $(METAL_OBJ) $(SUPPORT_OBJS)
	$(CXX) $^ -o $@ $(LDFLAGS)
	mkdir -p $(BUNDLE_DIR)
	cp PowerCollage.ofx $(BUNDLE_DIR)/
ifeq ($(UNAME_SYSTEM), Darwin)
	codesign --force --sign - PowerCollage.ofx.bundle
endif

# The shared kernel is embedded as a string for the Metal compiler.
PowerCollageKernelSource.h: PowerCollageKernel.inc
	{ printf 'static const char kPowerCollageKernelSource[] = R"PCK('; cat $<; printf ')PCK";\n'; } > $@

$(SDK):
	ln -sfn "$(OFX_SDK)" $(SDK)

PowerCollagePlugin.o: PowerCollagePlugin.cpp PowerCollageGeometry.h PowerCollageKernel.inc | $(SDK)
	$(CXX) -c $< $(CXXFLAGS)

MetalKernel.o: MetalKernel.mm PowerCollageGeometry.h PowerCollageKernelSource.h | $(SDK)
	$(CXX) -c $< $(CXXFLAGS)

$(SUPPORT_OBJS): %.o: | $(SDK)
	$(CXX) -c $(SDK)/Support/Library/$*.cpp $(CXXFLAGS)

clean:
	rm -f *.o *.ofx PowerCollageKernelSource.h $(SDK)
	rm -rf PowerCollage.ofx.bundle

install: PowerCollage.ofx
	mkdir -p "$(OFX_PLUGINS)"
	rm -rf "$(OFX_PLUGINS)/PowerCollage.ofx.bundle"
	cp -R PowerCollage.ofx.bundle "$(OFX_PLUGINS)/"

link: PowerCollage.ofx
	mkdir -p "$(OFX_PLUGINS)"
	rm -rf "$(OFX_PLUGINS)/PowerCollage.ofx.bundle"
	ln -s "$(CURDIR)/PowerCollage.ofx.bundle" "$(OFX_PLUGINS)/PowerCollage.ofx.bundle"

release: PowerCollage.ofx
	rm -rf ../Plugin/PowerCollage.ofx.bundle
	mkdir -p ../Plugin
	cp -R PowerCollage.ofx.bundle ../Plugin/

.PHONY: all clean install link release
