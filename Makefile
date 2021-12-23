# --- PROJECT
CONTIKI_PROJECT=app

# --- CONFIGURATION
# Enable or Disable pedantic checks
CHECKS?=true
# - Contiki
DEFINES=PROJECT_CONF_H=\"src/config.h\"
CONTIKI_WITH_RIME=1
CONTIKI?=../../contiki
ifndef TARGET
# TMote Sky (Cooja)
TARGET=sky
else
ifeq ($(TARGET), zoul)
# Zolertia Firefly (Testbed)
BOARD?=firefly
LDFLAGS+=-specs=nosys.specs
endif
endif
# - CC
ifdef CHECKS
ifeq ($(CHECKS), true)
# C version
CFLAGS+=-std=c99
# Enable 'asm'
CFLAGS+=-fasm
# - Warnings
CFLAGS+=-Wextra -Wall -pedantic -Wundef \
		-Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes \
		-Wstrict-overflow=5 -Wwrite-strings -Wwrite-strings -Waggregate-return \
		-Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion \
		-Wunreachable-code -Wfloat-equal
endif
endif

# --- SOURCE FILES
PROJECTDIRS+=src src/etc src/tools
PROJECT_SOURCEFILES+=etc.c simple-energest.c

# --- RECIPES
all: $(CONTIKI_PROJECT)

cleanall: distclean
	rm -f symbols.c symbols.h COOJA.log

include $(CONTIKI)/Makefile.include
