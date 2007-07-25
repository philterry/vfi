#ifndef RDDMA_PARSE_H
#define RDDMA_PARSE_H
#include <linux/types.h>
/*
 * offset    := <hexstr>
 * extent    := <hexstr>
 * name      := <string>
 * var       := <string>
 * val       := <string>
 * option    := var[=val]
 * options   := ?option[,option]*
 * location  := name[.location][options]
 * smb       := name[.location][options]
 * src | dst := smb[#offset][:extent][options]
 * bind      := dst = src[options]
 * xfer      := name[.location][#offset][:extent] / bind[options]
 */

#define RDDMA_MAX_QUERY_STRINGS 5
struct rddma_desc_param {
	char *name;
	char *location;
	u64 offset;
	int extent;
/* DO NOT SEPERATE THE FOLLOWING TWO LINES */
	char *query[RDDMA_MAX_QUERY_STRINGS];		/* var[=val] */
	char *rest;		/* var[=val][,var[=val]]* */
/* rest is aliased as query[RDDMA_MAX_QUERY_STRINGS] in the parse routine! Naughty code! */

	struct rddma_ops *ops;
	struct rddma_dma_ops *dma_ops;
};

struct rddma_bind_param {
	struct rddma_desc_param dst;
	struct rddma_desc_param src;
};

struct rddma_xfer_param {
	struct rddma_desc_param xfer;
	struct rddma_bind_param bind;
};

extern int   rddma_parse_xfer( struct rddma_xfer_param *, const char *);
extern int   rddma_parse_bind( struct rddma_bind_param *, const char *);
extern int   rddma_parse_desc( struct rddma_desc_param *, const char *);
extern char *rddma_get_option( struct rddma_desc_param *, const char *);

#endif	/* RDDMA_PARSE_H */
