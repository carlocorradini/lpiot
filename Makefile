# --- PROJECT
CONTIKI_PROJECT=app

# --- CONFIGURATION
# Enable or Disable pedantic checks
CHECKS?=false
# - Contiki
DEFINES=PROJECT_CONF_H=\"src/configs/project_conf.h\"
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
# Warnings
CFLAGS+=-Wextra -Wall -pedantic -Wundef \
		-Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes \
		-Wstrict-overflow=5 -Wwrite-strings -Wwrite-strings -Waggregate-return \
		-Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion \
		-Wunreachable-code -Wfloat-equal
endif
endif

# --- SOURCE FILES
PROJECTDIRS+=src src/configs src/etc src/tools src/nodes/controller src/nodes/forwarder src/nodes/sensor
PROJECT_SOURCEFILES+=config.c etc.c simple-energest.c controller.c forwarder.c sensor.c

# --- RECIPES
all: $(CONTIKI_PROJECT)

cleanall: distclean
	rm -f symbols.c symbols.h COOJA.log

include $(CONTIKI)/Makefile.include
