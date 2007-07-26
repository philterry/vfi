#include <linux/rddma_dma_rio.h>
#include <linux/rddma_src.h>
#include <asm/io.h>

static void rddma_dma_rio_load(struct rddma_src *src)
{
	struct seg_desc *rio = (struct seg_desc *)&src->descriptor;
	rio->paddr = virt_to_phys(rio);
	rio->hw.saddr = src->desc.src.offset & 0xffffffff;
	rio->hw.src_attr = src->desc.src.offset >> 32;
	rio->hw.daddr = src->desc.dst.offset & 0xffffffff;
	rio->hw.dest_attr = src->desc.dst.offset >> 32;
	rio->hw.nbytes = src->desc.dst.extent;
	rio->hw.next_ext = 0;
	rio->hw.next = 1;
	INIT_LIST_HEAD(&rio->node);
}

static void rddma_dma_rio_link_src(struct rddma_src *first, struct rddma_src *second)
{
	struct seg_desc *rio1 = (struct seg_desc *)&first->descriptor;
	struct seg_desc *rio2 = (struct seg_desc *)&second->descriptor;
	struct seg_desc *riolast = to_sdesc(rio1->node.prev);
	list_add(&rio1->node, &rio2->node);
	riolast->hw.next = rio2->paddr &0xffffffe0;
}

static void rddma_dma_rio_link_dst(struct rddma_dst *first, struct rddma_dst *second)
{
	struct seg_desc *rio1 = (struct seg_desc *)&first->head_src->descriptor;
	struct seg_desc *rio2 = (struct seg_desc *)&second->head_src->descriptor;
	struct seg_desc *riolast = to_sdesc(rio1->node.prev);
	list_add(&rio1->node, &rio2->node);
	riolast->hw.next = rio2->paddr &0xffffffe0;
}

static void rddma_dma_rio_link_bind(struct rddma_bind *first, struct rddma_bind *second)
{
	struct seg_desc *rio1 = (struct seg_desc *)&first->head_dst->head_src->descriptor;
	struct seg_desc *rio2 = (struct seg_desc *)&second->head_dst->head_src->descriptor;
	struct seg_desc *riolast = to_sdesc(rio1->node.prev);
	list_add(&rio1->node, &rio2->node);
	riolast->hw.next = rio2->paddr &0xffffffe0;
}

struct rddma_dma_ops rddma_dma_ops_rio = {
	.load = rddma_dma_rio_load,
	.link_src = rddma_dma_rio_link_src,
	.link_dst = rddma_dma_rio_link_dst,
	.link_bind = rddma_dma_rio_link_bind,
};
