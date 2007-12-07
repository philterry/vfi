
obj-$(CONFIG_RDDMA_DMA_6460) += rddma_6460.o
rddma_6460-objs := \
	rddma_dma_6460.o 

obj-$(CONFIG_RDDMA_DMA_RIO) += rddma_dma_rio.o

obj-$(CONFIG_RDDMA_FABRIC_NET) += rddma_fabric_net.o

obj-$(CONFIG_RDDMA_FABRIC_RIONET) += rddma_fabric_rionet.o

obj-$(CONFIG_RDDMA) += rddma.o
rddma-objs := \
              rddma_drv.o \
              rddma_bus.o \
              ringbuf.o \
              rddma_class.o \
                 rddma_fabric.o \
                 rddma_dma.o \
                 rddma_cdev.o \
              rddma_parse.o \
              rddma_dump.o \
              rddma_ops.o \
                   rddma_fabric_ops.o \
                   rddma_local_ops.o \
	      rddma_subsys.o \
                   rddma_location.o \
			rddma_readies.o \
                        rddma_dones.o \
			rddma_events.o \
			rddma_event.o \
                        rddma_smbs.o \
                             rddma_smb.o \
				rddma_mmaps.o \
					rddma_mmap.o \
                        rddma_xfers.o \
                             rddma_xfer.o \
                                  rddma_binds.o \
                                       rddma_bind.o \
                                            rddma_dsts.o \
                                                 rddma_dst.o \
                                                      rddma_srcs.o \
                                                           rddma_src.o


