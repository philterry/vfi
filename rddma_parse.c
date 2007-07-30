#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_ops.h>

#include <linux/slab.h>
#include <linux/string.h>

#define strtol simple_strtol

/**
 * rddma_str_dup: Safe copy of name to max truncated length of 4095 plus null termination in a page.
 */
static char *rddma_str_dup(const char *name)
{
	char *ret = NULL;
	if (name)
		if ((ret = kzalloc(strnlen(name,4095)+1, GFP_KERNEL)) )
			strncpy(ret, name, 4095);
	return ret;
}

/**
 * name_remainder: Poor mans strtok butchers input string.
 */
static char *name_remainder(char *name, int c, char **remainder)
{
	char *p = NULL;

	if (name)
		p = strchr(name,c);

	if (p) 
		*p++ = 0;

	if (remainder)
		*remainder = p;

	return p;
}
/**
 * rddma_get_option: returns value of var or var if present.
 */
char *rddma_get_option(struct rddma_desc_param *desc, const char *needle)
{
	char *found_var = NULL;
	char *found_val = NULL;
	int i;

	for (i = 0; i <= RDDMA_MAX_QUERY_STRINGS; i++)
		if (desc->query[i])
			if ((found_var = strstr(desc->query[i],needle)))
				found_val = strstr(found_var,"=");

	return found_val ? found_val+1 : found_var ;
}

/**
 * rddma_parse_desc: Takes a string which is duped and parsed into the desc struct.
 * Chops string into components and assigns to param struct parts.
 * Pass an rddma_str_dup'd name if the string needs to be preserved intact
 * or you don't own it or can't otherwise ensure its lifetime.
 *
 * name[.loc]*[#offset][:extent]
 * name[.loc]*[:extent][#offset]
 *
 */

int rddma_parse_desc(struct rddma_desc_param *d, const char *desc)
{
	int ret = -EINVAL;
	char *sextent=NULL;
	char *soffset=NULL;
	char *ops;
	int i;

	d->extent = 0;
	d->offset = 0;
	d->location = NULL;
	d->query[0] = NULL;
	d->rest = NULL;
	d->ops = NULL;

	if ( NULL == (d->name = rddma_str_dup(desc)) )
		goto out;

	name_remainder(d->name,     '?', &d->query[0]);
	name_remainder(d->name,     '.', &d->location);
	name_remainder(d->location, ':', &sextent);
	name_remainder(d->location, '#', &soffset);

	if (d->location)
		*(d->location - 1) = '.';

	if (NULL == sextent)
		name_remainder(soffset, ':', &sextent);
	
	if (NULL == soffset)
		name_remainder(sextent, '#', &soffset);

	if (sextent) {
		d->extent = simple_strtol(sextent,&sextent,0);
 		RDDMA_ASSERT((NULL == sextent),"Dodgy extent string(%d) contains %s", ret = 1, sextent); 
	}

	if (soffset) {
		d->offset = simple_strtol(soffset,&soffset,0);
 		RDDMA_ASSERT((NULL == soffset),"Dodgy offset string(%d) contains %s", ret = 1, soffset); 
	}

	i = 0;
	while (name_remainder(d->query[i], ',', &d->query[i+1]) && i < RDDMA_MAX_QUERY_STRINGS);
	/* The above naughty loop will leave any remaining options in d->rest */
	
	if ( (ops = rddma_get_option(d,"default_ops")) ) {
		if (!strncmp(ops,"private",7))
			d->ops = &rddma_local_ops;
		else if (!strncmp(ops,"public",6))
			d->ops = &rddma_fabric_ops;
	}

	return 0;
out:
	return ret;
}

/**
 * rddma_parse_bind: Dupe string and parse it into bind_param struct.
 */
int rddma_parse_bind( struct rddma_bind_param *b, const char *desc)
{
	int ret = -EINVAL;
	char *mydesc;

	if ( NULL == (mydesc = rddma_str_dup(desc)) )
		goto out;

	if ( name_remainder(mydesc, '=', &b->src.name) )
			if ((ret = rddma_parse_desc( &b->src, b->src.name )))
				ret = rddma_parse_desc( &b->dst, mydesc );
	kfree(mydesc);
out:
	return ret;
}

/**
 * rddma_parse_xfer: Dups string and parses it into xfer_param struct.
 */
int rddma_parse_xfer(struct rddma_xfer_param *x, const char *desc)
{
	int ret = -EINVAL;
	char *mydesc;

	if ( NULL == (mydesc = rddma_str_dup(desc)) )
		goto out;

	if ( name_remainder(mydesc, '/', &x->bind.dst.name) ) 
		if (!(ret = rddma_parse_bind( &x->bind, x->bind.dst.name )))
			ret = rddma_parse_desc( &x->xfer, mydesc );
out:
	return ret;
}


