# --- PROJECT
CONTIKI_PROJECT = app

# --- CONFIGURATION
# Statistics
STATS ?= false
# - Contiki
DEFINES = PROJECT_CONF_H=\"src/config/project_conf.h\"
CONTIKI_WITH_RIME = 1
CONTIKI ?= ../../contiki
ifndef TARGET
# TMote Sky (Cooja)
TARGET = sky
else
ifeq ($(TARGET), zoul)
# Zolertia Firefly (Testbed)
BOARD? = firefly
LDFLAGS += -specs=nosys.specs
endif
endif
# - CC
# Statistics
ifeq ($(STATS), true)
CFLAGS += -DSTATS
endif

# --- SOURCE FILES
PROJECTDIRS += src \
			   src/config \
			   src/connection src/connection/beacon \
			   src/etc \
			   src/logger \
			   src/node src/node/controller src/node/forwarder src/node/sensor \
			   src/tool
PROJECT_SOURCEFILES += \
					   config.c \
					   connection.c beacon.c \
					   etc.c \
					   logger.c \
					   node.c controller.c forwarder.c sensor.c
ifeq ($(STATS), true)
PROJECT_SOURCEFILES += simple-energest.c
endif

# --- RECIPES
all: $(CONTIKI_PROJECT)

cleanall: distclean
	rm -f symbols.c symbols.h
	rm -f app.elf app.hex app.zoul
	rm -rf obj_zoul
	rm -f COOJA.log COOJA.testlog
	rm -f *_mrm*.csv *_mrm*.log

include $(CONTIKI)/Makefile.include
