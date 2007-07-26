ifneq ($(KERNELRELEASE),)
include Kbuild
else
KERNELDIR := /lib/modules/`uname -r`/build

modules:
	$(MAKE) -C $(KERNELDIR) M=`pwd` $@
install:
	$(MAKE) -C $(KERNELDIR) M=`pwd` modules_install
clean:
	$(MAKE) -C $(KERNELDIR) M=`pwd` $@
	-rm Module.symvers *~ TAGS
etags:
	etags *.[ch]
tar: clean etags
	cd ..;	tar zcvf rddma.tar.gz --exclude=.git --exclude=TAGS rddma
publish: tar doc
	cp ../rddma.tar.gz /mnt/Public/Rincon/PhilsWeb/
	cp ChangeLog /mnt/Public/Rincon/PhilsWeb/
	cp README /mnt/Public/Rincon/PhilsWeb/READMEdrv
	cp TODO /mnt/Public/Rincon/PhilsWeb/TODOdrv
	cp $(KERNELDIR)/Documentation/DocBook/index.html /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/rddma /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/kernel-api /mnt/Public/Rincon/PhilsWeb/kerneldoc
	cp -r $(KERNELDIR)/Documentation/DocBook/rapidio /mnt/Public/Rincon/PhilsWeb/kerneldoc

doc:
	$(MAKE) -C $(KERNELDIR) htmldocs
	$(MAKE) -C $(KERNELDIR) mandocs
	sudo $(MAKE) -C $(KERNELDIR) installmandocs
endif
