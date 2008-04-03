ifneq ($(KERNELRELEASE),)
include Kbuild
else
KERNELDIR ?= /lib/modules/`uname -r`/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd` CONFIG_VFI_DEBUG=1 CONFIG_VFI_KOBJ_DEBUG=1 \
                   CONFIG_VFI_FABRIC_NET=m CONFIG_VFI_FABRIC_RIONET=m CONFIG_VFI=m CONFIG_VFI_DMA_NET=n CONFIG_VFI_DMA_RIO=m $@
install:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules_install
clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` $@
	-rm -f Module.symvers *~ TAGS
etags:
	etags *.[ch]
tar: clean etags
	cd ..;	tar zcvf fddma.tar.gz --exclude=.git --exclude=TAGS fddma
publish: tar
	cp ../fddma.tar.gz /mnt/Public/Rincon/PhilsWeb/dev
	cp ChangeLog /mnt/Public/Rincon/PhilsWeb/dev
	cp README /mnt/Public/Rincon/PhilsWeb/dev/READMEdrv
	cp TODO /mnt/Public/Rincon/PhilsWeb/devTODOdrv
publish_doc: doc
	cp $(KERNELDIR)/Documentation/DocBook/index.html /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/vfi /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/kernel-api /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/rapidio /mnt/Public/Rincon/PhilsWeb/kerneldoc
doc:
	$(MAKE) -C $(KERNELDIR) htmldocs
	$(MAKE) -C $(KERNELDIR) mandocs
	sudo $(MAKE) -C $(KERNELDIR) installmandocs
endif
