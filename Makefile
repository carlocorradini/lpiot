# For TMote Sky (emulated in Cooja) use the following target
ifndef TARGET
	TARGET = sky
else
	# For Zolertia Firefly (testbed) use the following target and board
	# Don't forget to make clean when you are changing the board
	ifeq ($(TARGET), zoul)
		BOARD	?= firefly
		LDFLAGS += -specs=nosys.specs # For Zolertia Firefly only
	endif
endif

# --- PROJECT
CONTIKI_PROJECT = app

# --- CONFIGURATION
DEFINES=PROJECT_CONF_H=\"src/project-conf.h\"

# --- SOURCE FILES
PROJECTDIRS += src src/tools
PROJECT_SOURCEFILES += etc.c simple-energest.c

# --- RECIPES
all: $(CONTIKI_PROJECT)

# --- CONTIKI
CONTIKI_WITH_RIME = 1
CONTIKI ?= ../../contiki

include $(CONTIKI)/Makefile.include
