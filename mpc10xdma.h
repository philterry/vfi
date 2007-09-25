
#define MPC10X_DMA_NCHANS 2
#define DMA_MAX_XFER ((1<<26) - 1)

/* Per-channel DMA registers */
#define DMA_MR 		0x00
#define DMA_SR 		0x04
#define DMA_CDAR	0x08
#define DMA_HCDAR 	0x0c
#define DMA_SAR 	0x10
#define DMA_HSAR 	0x14
#define DMA_DAR 	0x18
#define DMA_HDAR 	0x1c
#define DMA_BCR 	0x20
#define DMA_NDAR 	0x24
#define DMA_HNDAR 	0x28

/* Memory format of DMA link descriptor 
 * (must be 32-byte aligned) 
 */
struct dma_link {
	unsigned int saddr;
	unsigned int saddr_ext;
	unsigned int daddr;
	unsigned int daddr_ext;
	unsigned int next;
	unsigned int next_ext;
	unsigned int nbytes;
	int dummy;
};

struct seg_desc {
	struct dma_link hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes */
	u64 paddr;              /* 8 bytes */
	u32 user2;		/* 16 extra bytes */
	u32 user3;
	u32 user4;
	u32 user5;
};

/* Mode Register bits 
 *
 * Not defining them all because address holding unsupported 
 * in chaining mode
 */
#define DMA_MODE_PCI_DELAY2 	0x000000
#define DMA_MODE_PCI_DELAY4 	0x100000
#define DMA_MODE_PCI_DELAY16 	0x200000
#define DMA_MODE_PCI_DELAY32 	0x300000
#define DMA_MODE_PCI_DELAY_MASK 0x300000
#define DMA_MODE_USE_INTA 	0x80000
#define DMA_MODE_PCI_READ 	0x000
#define DMA_MODE_PCI_READ_LINE 	0x400
#define DMA_MODE_PCI_READ_MULT  0x800
#define DMA_MODE_PCI_READ 	0x000
#define DMA_MODE_PCI_READ_MASK 	0xc00
#define DMA_MODE_ERR_INT_EN	0x100
#define DMA_MODE_CHAIN_INT_EN   0x80
#define DMA_MODE_PCI_DESC       0x8 
#define DMA_MODE_DIRECT	        0x4 
#define DMA_MODE_CONTINUE       0x2
#define DMA_MODE_START 	        0x1

/* Status register bits
 */
#define DMA_STAT_MEM_ERR		0x80
#define DMA_STAT_PCI_ERR		0x10
#define DMA_STAT_CB			0x4
#define DMA_STAT_SEGMENT_INTERRUPT	0x2
#define DMA_STAT_CHAIN_INTERRUPT	0x1


/* Channel states */
#define DMA_UNINIT 	0
#define DMA_IDLE 	1
#define DMA_RUNNING	2
#define DMA_ERR_STOP	3
#define DMA_SHUTDOWN 	5

#if 0
/*  Note:  The last 8 bytes of the H/W descriptor are reserved,
 *  so we could move "struct list_head" there and use a full
 *  32 bytes of user area if necessary.
 */
struct list_desc {
	struct dma_list hw;	/* 32 bytes, h/w descriptor */
	struct list_head node;  /* 8 bytes - for driver use */
	u32 paddr;              /* 4 bytes - phys addr of first desc in DMA chain */
	u32 driver1;
	void (*cb)(struct list_desc *);	/* 4 - rddma callback */
	u32 rc;			/* 4 - return code */
	u32 flags;
	u32 user2;
};
#endif

/* NB: must be same size as struct rddma_dma_descriptor */
struct my_xfer_object {
	u32 hw[8];	/* 32 bytes, not used */
	struct seg_desc *desc;	/* ptr to first descriptor in DMA chain */
	u32 driver1;
	struct rddma_xf xf;	/* RDDMA */
} __attribute__((aligned(RDDMA_DESC_ALIGN)));

/* Mode Register bits 
 *
 * Not defining them all because address holding unsupported 
 * in chaining mode
 */
#define DMA_MODE_PCI_DELAY2 	0x000000
#define DMA_MODE_PCI_DELAY4 	0x100000
#define DMA_MODE_PCI_DELAY16 	0x200000
#define DMA_MODE_PCI_DELAY32 	0x300000
#define DMA_MODE_PCI_DELAY_MASK 0x300000
#define DMA_MODE_USE_INTA 	0x80000
#define DMA_MODE_PCI_READ 	0x000
#define DMA_MODE_PCI_READ_LINE 	0x400
#define DMA_MODE_PCI_READ_MULT  0x800
#define DMA_MODE_PCI_READ 	0x000
#define DMA_MODE_PCI_READ_MASK 	0xc00
#define DMA_MODE_ERR_INT_EN	0x100
#define DMA_MODE_XFER_INT_EN    0x80
#define DMA_MODE_PCI_DESC       0x8 
#define DMA_MODE_DIRECT	        0x4 
#define DMA_MODE_CONTINUE       0x2
#define DMA_MODE_START 	        0x1

/* Status register bits
 */
#define DMA_STAT_MEM_ERR		0x80
#define DMA_STAT_PCI_ERR		0x10
#define DMA_STAT_CHAN_BUSY		0x4
#define DMA_STAT_SEGMENT_INTERRUPT	0x2
#define DMA_STAT_CHAIN_INTERRUPT	0x1

/* Interrupt at the end of every DMA chain */
#define DMA_INTERRUPTS \
	(DMA_MODE_ERR_INT | DMA_MODE_CHAIN_INTERRUPT)

/* Channel states */
#define DMA_UNINIT 	0
#define DMA_IDLE 	1
#define DMA_RUNNING	2
#define DMA_ERR_STOP	3
#define DMA_SHUTDOWN 	5

/* End of list bit in next descriptor address register */
#define DMA_EOL 0x1
#define DMA_END_OF_CHAIN DMA_EOL

/* Status of completed descriptors */
#define DMA_OK 0x1
#define DMA_ERR_PE  0x2
#define DMA_ERR_TE  0x4
#define DMA_CHANNEL_DOWN  0x8
#define DMA_ERROR  0x6

/* Descriptor flags -- so far just 1 */
#define DMA_NO_CALLBACK 0x1

/* Descriptor bits */
#define DMA_BUS_LOCAL_MEM 	0x0
#define DMA_BUS_LOCAL_TO_PCI 	0x2
#define DMA_BUS_PCI_TO_LOCAL	0x4
#define DMA_BUS_PCI_TO_PCI 	0x6
#define DMA_SEG_INT	0x8
#define DMA_SNOOP	0x10

struct client {
	int dummy;
};

struct ppc_dma_event {
	struct my_xfer_object *desc;
	int chan_num;
	int status;
};

struct ppc_dma_chan {
	struct ppcdma_device *device;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_entry;
#endif
	void *regbase;
	int state;    
	unsigned int op_mode;
	int priority; /* 4 values? */
	unsigned int xfercap;
	char name[16];
	int num;
	int irq;
	int flags;
	struct list_head dma_q;  
	spinlock_t queuelock;
	/* void (*client_cb)(struct list_desc *, int);  */
	struct list_desc *null_desc;
	dma_addr_t null_desc_phys;
	spinlock_t cleanup_lock;  /* not used for now */
	int seg_int;  		 /* counters */
	int list_int;
	int chain_int;
	int bogus_int;
	int err_int;
	int errors;
	int bytes_queued;
	u64 bytes_tx;
};

#define to_xfer_object(lh) ((lh) ? list_entry((lh), struct my_xfer_object, xf.node) : 0)
#define to_sdesc(sh) ((sh) ? list_entry((sh), struct seg_desc, node) : 0)
