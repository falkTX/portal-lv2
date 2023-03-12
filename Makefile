#!/usr/bin/make -f
# Makefile for portal.lv2 #
# ----------------------- #

include Makefile.base.mk

NAME = portal
TARGET = $(NAME).lv2/$(NAME).so

# ---------------------------------------------------------------------------------------------------------------------
# Installation path

PREFIX ?= /usr/local
BUNDLE_INSTALL_PATH = $(DESTDIR)$(PREFIX)/lib/lv2/$(NAME).lv2

# ---------------------------------------------------------------------------------------------------------------------
# Build rules

BUILD_C_FLAGS += -Wno-unused-parameter
BUILD_C_FLAGS += -pthread

ifeq ($(MACOS),true)
BUILD_C_FLAGS += -Wno-deprecated-declarations
endif

LINK_FLAGS += -pthread

# ---------------------------------------------------------------------------------------------------------------------
# Build target

all: $(TARGET)

$(TARGET): build/$(NAME).c.o
	$(CC) $^ $(LINK_FLAGS) $(SHARED) -o $@

build/$(NAME).c.o: $(NAME).c
	@mkdir -p $(@D)
	$(CC) $^ $(BUILD_C_FLAGS) -c -o $@

# ---------------------------------------------------------------------------------------------------------------------
# Install target

install: build
	install -d $(BUNDLE_INSTALL_PATH)
	install -m 644 $(NAME).lv2/*.so $(BUNDLE_INSTALL_PATH)
	install -m 644 $(NAME).lv2/*.ttl $(BUNDLE_INSTALL_PATH)
# 	cp -rv $(NAME).lv2/modgui $(BUNDLE_INSTALL_PATH)/

# ---------------------------------------------------------------------------------------------------------------------
# Clean target

clean:
	rm -f $(TARGET)
	rm -rf build

# ---------------------------------------------------------------------------------------------------------------------

# -include build/$(NAME).c.d

# ---------------------------------------------------------------------------------------------------------------------
