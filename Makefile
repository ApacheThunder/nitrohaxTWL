#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/ds_rules

export TARGET		:=	NitroHax
export TOPDIR		:=	$(CURDIR)

export VERSION_MAJOR	:= 0
export VERSION_MINOR	:= 95
export VERSTRING	:=	$(VERSION_MAJOR).$(VERSION_MINOR)


.PHONY: dsi checkarm7 checkarm9

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all: $(TARGET).nds $(TARGET).dsi checkarm7 checkarm9

#---------------------------------------------------------------------------------
checkarm7:
	$(MAKE) -C arm7

#---------------------------------------------------------------------------------
checkarm9:
	$(MAKE) -C arm9

$(TARGET).nds	:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).nds -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b $(CURDIR)/icon.bmp "Nitro Hax;DS Game Cheat Tool;Created by Chishm"

			
$(TARGET).dsi	:	arm7/$(TARGET).elf arm9/$(TARGET).elf
	ndstool	-c $(TARGET).dsi -7 arm7/$(TARGET).elf -9 arm9/$(TARGET).elf \
			-b $(CURDIR)/icon.bmp "Nitro Hax for DSi;DS Game Cheat Tool;Created by Chishm" \
			-g CHTE 01 "NITROHAXTWL" -z 80040000 -u 00030004 -a 00000038 -p 0000

#---------------------------------------------------------------------------------
# Create boot loader and link raw binary into ARM9 ELF
#---------------------------------------------------------------------------------
BootLoader/load.bin	:	BootLoader/source/*
	$(MAKE) -C BootLoader

BootLoaderNTR/loadntr.bin	:	BootLoaderNTR/source/*
	$(MAKE) -C BootLoaderNTR
	
	
arm9/data/load.bin	:	BootLoader/load.bin
	mkdir -p $(@D)
	cp $< $@

	
arm9/data/loadntr.bin	:	BootLoaderNTR/loadntr.bin
	mkdir -p $(@D)
	cp $< $@

#---------------------------------------------------------------------------------
arm9/source/version.h : Makefile
	@echo "#ifndef VERSION_H" > $@
	@echo "#define VERSION_H" >> $@
	@echo >> $@
	@echo '#define VERSION_STRING "v'$(VERSION_MAJOR).$(VERSION_MINOR)'"' >> $@
	@echo >> $@
	@echo "#endif // VERSION_H" >> $@

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	$(MAKE) -C arm7
	
#---------------------------------------------------------------------------------
arm9/$(TARGET).elf	:	arm9/data/load.bin arm9/data/loadntr.bin arm9/source/version.h
	$(MAKE) -C arm9

#---------------------------------------------------------------------------------
dist-bin	: $(TARGET).nds $(TARGET).dsi README.md LICENSE
	zip -X -9 $(TARGET)_v$(VERSTRING).zip $^

dist-src	:
	tar --exclude=*~ -cvjf $(TARGET)_src_v$(VERSTRING).tar.bz2 \
	--transform 's,^,$(TARGET)/,' \
	Makefile icon.bmp LICENSE README.md \
	arm7/Makefile arm7/source \
	arm9/Makefile arm9/source arm9/graphics \
	BootLoader/Makefile BootLoader/load.ld BootLoader/source \
	BootLoaderNTR/Makefile BootLoaderNTR/load.ld BootLoaderNTR/source

dist	:	dist-bin dist-src

#---------------------------------------------------------------------------------
clean:
	$(MAKE) -C arm9 clean
	$(MAKE) -C arm7 clean
	$(MAKE) -C BootLoader clean
	$(MAKE) -C BootLoaderNTR clean
	rm -f arm9/data/load.bin
	rm -f arm9/data/loadntr.bin
	rm -f $(TARGET).ds.gba $(TARGET).nds $(TARGET).dsi $(TARGET).arm7 $(TARGET).arm9
