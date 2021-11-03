#Linux Recon Makefile
#Author: Bryan Poling
#Copyright (c) 2020 Sentek Systems, LLC. All rights reserved. 

CC = gcc
#CC = g++
#CC = clang++-3.5

#Build Type: Acceptable values are Debug and Release
BUILD_TYPE = Release

ifeq ($(BUILD_TYPE),Release)
	OPTFLAGS   = -O3 -march=broadwell -fopenmp
	DEBUGFLAGS = 
else
	OPTFLAGS   = -O0 -march=core2 -fopenmp
	DEBUGFLAGS = -g
endif

# ****************************************************   Include Paths   ****************************************************
RECON_INCLUDE_FLAGS1 = -I.. -I../eigen -I../Flexible-Raster-Format -I../imgui -I../restclient-cpp/include -I../cereal/include
RECON_INCLUDE_FLAGS2 = `pkg-config --cflags freetype2 libcurl gtk+-3.0` -I../glfw/include -I../imgui/examples/libs/gl3w -I../nativefiledialog/src/include
RECON_INCLUDE_FLAGS3 = -I../libtorch-cxx11-abi-shared-with-deps-1.8.1+cpu/libtorch/include/
RECON_INCLUDE_FLAGS4 = -I../libtorch-cxx11-abi-shared-with-deps-1.8.1+cpu/libtorch/include/torch/csrc/api/include/ -I../serial/include -I../tacopie/includes
RECON_INCLUDE_FLAGS5 = -I../soloud/include
RECON_INCLUDE_FLAGS  = $(RECON_INCLUDE_FLAGS1) $(RECON_INCLUDE_FLAGS2) $(RECON_INCLUDE_FLAGS3) $(RECON_INCLUDE_FLAGS4) $(RECON_INCLUDE_FLAGS5)

# ****************************   Set C++ Standard, profiling and linker trim flags and Defines   ****************************
STANDARDFLAGS = -std=c++17
PROFILEFLAGS =
#PROFILEFLAGS = -pg
LINKER_TRIM_FLAGS = 
#LINKER_TRIM_FLAGS = -Wl,--gc-sections -Wl,--strip-all
DEFINE_FLAGS = -DGSL_USE_STD_BYTE -DLOADGLFWICON -DIMGUIAPP_USE_FAS -DWITH_ALSA

# ******************************************   Combine all Compile and Link Flags   *****************************************
COMPILE_WARNING_FLAGS = -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-strict-aliasing \
                        -Wno-ignored-attributes -Wno-misleading-indentation
COMPILE_DEPGEN_FLAGS  = -MT $@ -MMD -MP -MF DEP/$(*F).d
ELF_FLAGS             = -fdata-sections -ffunction-sections
COMPILE_FLAGS         = -c -fdiagnostics-color=auto -pthread $(COMPILE_WARNING_FLAGS) $(RECON_INCLUDE_FLAGS) $(COMPILE_DEPGEN_FLAGS) \
                        $(DEBUGFLAGS) $(PROFILEFLAGS) $(LINKER_TRIM_FLAGS) $(DEFINE_FLAGS) $(STANDARDFLAGS) $(OPTFLAGS) $(ELF_FLAGS) `pkg-config --cflags opencv4`
LINK_FLAGS            = -fdiagnostics-color=auto -static-libstdc++ -static-libgcc -lstdc++ -lstdc++fs \
                        -Wl,-Bdynamic -lpthread -lm -ldl -luuid -fopenmp \
                        `pkg-config --static --libs freetype2 libcurl gtk+-3.0` ../glfw/Release/src/libglfw3.a `pkg-config --libs opencv4` \
                        -lGL -lGLEW -lGLU \
                        ../tacopie/build/lib/libtacopie.a -L ../libtorch-cxx11-abi-shared-with-deps-1.8.1+cpu/libtorch/lib/ -ltorch -ltorch_cpu -lc10 \
                        '-Wl,-rpath,$$ORIGIN/../../libtorch-cxx11-abi-shared-with-deps-1.8.1+cpu/libtorch/lib' -lasound

# **********************************************   Populate Source File Lists   *********************************************
#Populate source files that are part of Recon project
RECON_SRCFILES = $(wildcard SRC/*.cpp) $(wildcard SRC/UI/*.cpp) $(wildcard SRC/Maps/*.cpp) $(wildcard SRC/Modules/DJI-Drone-Interface/*.cpp) \
                 $(wildcard SRC/Modules/Guidance/*.cpp) $(wildcard SRC/Modules/Shadow-Detection/*.cpp) $(wildcard SRC/Modules/Shadow-Propagation/*.cpp) \
                 $(wildcard SRC/Modules/GNSS-Receiver/*.cpp) $(wildcard SRC/Modules/Coverage-planning/Navigation/*.cpp)

#Build list of additional (external) source files.
EXTERNAL_SRCFILES = ../restclient-cpp/source/connection.cc \
                    ../restclient-cpp/source/helpers.cc \
                    ../restclient-cpp/source/restclient.cc \
                    ../imgui/imgui_widgets.cpp \
                    ../imgui/imgui_draw.cpp \
                    ../imgui/imgui_demo.cpp \
                    ../imgui/imgui.cpp \
                    ../imgui/misc/cpp/imgui_stdlib.cpp \
                    ../imgui/misc/freetype/imgui_freetype.cpp \
                    ../imgui/examples/imgui_impl_glfw.cpp \
                    ../imgui/examples/imgui_impl_opengl3.cpp \
                    ../imgui/examples/libs/gl3w/GL/gl3w.c \
                    ../imgui/app/ImGuiApp.cpp \
                    ../imgui/app/imconfig.cpp \
                    ../imgui/app/main_GL.cpp \
                    ../nativefiledialog/src/nfd_common.c \
                    ../nativefiledialog/src/nfd_gtk.c \
                    ../Flexible-Raster-Format/FRF.cpp \
                    ../handycpp/Handy.cpp \
                    ../serial/src/serial.cc \
                    ../serial/src/impl/unix.cc \
                    ../implot/implot.cpp \
                    ../implot/implot_items.cpp \
                    $(wildcard ../soloud/src/core/*.cpp) \
                    $(wildcard ../soloud/src/backend/alsa/*.cpp) \
                    $(wildcard ../soloud/src/audiosource/wav/*.cpp) \
                    $(wildcard ../soloud/src/audiosource/wav/*.c)

# **********************************************   Populate Object File Lists   *********************************************
RECON_OBJFILES    = $(patsubst SRC/%.cpp,OBJ/%.o,$(RECON_SRCFILES))
EXTERNAL_OBJFILES = $(addprefix OBJ/External/,$(addsuffix .o,$(basename $(notdir $(EXTERNAL_SRCFILES)))))
OBJFILES          = $(RECON_OBJFILES) $(EXTERNAL_OBJFILES)

# *****************************************************   Build Rules   *****************************************************

#Top-level rule (default build rule)
all: recon

recon: folders $(OBJFILES)
	$(CC) -Wall $(DEBUGFLAGS) $(PROFILEFLAGS) $(LINKER_TRIM_FLAGS) -L/usr/local/lib -o BIN/Recon -I/usr/include/python3.8 -lpython3.8 $(OBJFILES) $(LINK_FLAGS)

#Reproduce the folder structure of SRC in OBJ and create an External sub-directory for external object files
folders:
	@mkdir -p DEP
	@mkdir -p OBJ/External
	@find SRC/ -type d -printf '%P\n' | sed 's/^/OBJ\//' | xargs mkdir -p

#Object file build rules
$(RECON_OBJFILES):
	@echo ""
	@echo "\033[1;34mCompiling file: \033[0m" $(shell echo $(RECON_SRCFILES) | grep -Eo '\<$(basename $(patsubst OBJ/%,SRC/%,$@))\.[^[:space:]]*|[[:space:]]$(basename $(patsubst OBJ/%,SRC/%,$@))\.[^[:space:]]*')
	@echo "\033[1;34mTarget Object:  \033[0m" $@
	$(CC) $(COMPILE_FLAGS) -I/usr/include/python3.8 -lpython3.8 -o $@ $(shell echo $(RECON_SRCFILES) | grep -Eo '\<$(basename $(patsubst OBJ/%,SRC/%,$@))\.[^[:space:]]*|[[:space:]]$(basename $(patsubst OBJ/%,SRC/%,$@))\.[^[:space:]]*')

$(EXTERNAL_OBJFILES):
	@echo ""
	@echo "\033[1;34mCompiling file: \033[0m" $(shell echo $(EXTERNAL_SRCFILES) | grep -Eo '\<$(*F)\.[^[:space:]]*|[[:space:]]$(*F)\.[^[:space:]]*|[^[:space:]]*/$(*F)\.[^[:space:]]*')
	@echo "\033[1;34mTarget Object:  \033[0m" $@
	$(CC) $(COMPILE_FLAGS) -o $@ $(shell echo $(EXTERNAL_SRCFILES) | grep -Eo '\<$(*F)\.[^[:space:]]*|[[:space:]]$(*F)\.[^[:space:]]*|[^[:space:]]*/$(*F)\.[^[:space:]]*')

#Clean build rule    *************************************************************************************
clean:
	/bin/rm -r OBJ/*
	/bin/rm DEP/*.d
	/bin/rm -f BIN/Recon

#Dependency file include directive    ********************************************************************
-include DEP/*.d

#Variable inspection rule: make print-VAR
print-%  : ; @echo $* = $($*)




