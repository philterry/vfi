/* 
 * 
 * Copyright 2007 MicroMemory, LLC.
 * Phil Terry <pterry@micromemory.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      RDDMA_DBG_PARSE | RDDMA_DBG_FUNCALL | RDDMA_DBG_DEBUG
#define MY_LIFE_DEBUG RDDMA_DBG_PARSE | RDDMA_DBG_LIFE    | RDDMA_DBG_DEBUG

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_ops.h>
#include <linux/rddma_fabric.h>
#include <linux/slab.h>
#include <linux/string.h>

#define strtol simple_strtol


/**
 * rddma_str_dup - Safe copy of name to max truncated length of 4095 plus null termination in a page.
 * @name: null terminated string to be duplicated.
 *
 * @name is duplicated into a freshly allocated buffer. This
 * allocation is limited to a 4096 byte page allocation and must be
 * null terminated so the allocation is max of @name length and 4095
 * plus 1.
 */
static char *rddma_str_dup(const char *name, size_t *newsize)
{
	char *ret = NULL;
	if (newsize)
		*newsize = 0;
	if (name) {
		size_t size = strnlen(name,4095);
		if ((ret = kzalloc(size+1, GFP_KERNEL)) ) {
			strncpy(ret, name, size);
			if (newsize)
				*newsize = size;
		}
	}

	RDDMA_DEBUG(MY_DEBUG,"%s %p to %p\n",__FUNCTION__,name,ret);
	return ret;
}

/**
 * name_remainder - Poor mans strtok butchers input string.
 * @name: string to be parsed into components.
 * @c: the character at which the division is to occur. This character
 * in the string is replaced with null.
 * @remainder: pointer to a string pointer in which the pointer to the
 * remainder of the string following @c is to be placed. If null the
 * remainder pointer is only available as the return value of the
 * function.
 *
 * The input string is modified by replacing the first occurence of
 * character c with a null. The pointer to the remainder of the string
 * is returned by the function and optionally place in the supplied
 * string pointer variable.
 * Return Value: the remainder of @name following @c or null if @c was
 * not found.
 */
static char *name_remainder(char *name, int c, char **remainder)
{
	char *p = NULL;

	if (name)
		p = strchr(name,c);

	if (p) 
		*p++ = '\0';

	if (remainder)
		*remainder = p;
	RDDMA_DEBUG(MY_DEBUG,"%s %s %c %p -> \n\t\t%s\n",__FUNCTION__,name,c,remainder,p);
	return p;
}
/**
 * rddma_get_option - returns value of var or var if present.
 * @desc: An rddma_desc_param structure into which a string has been
 * previously parsed. Any query option strings present in the original
 * will have been parsed into the options and rest members of the
 * struct.
 * @needle: the name of the option being searched for.
 *
 * Returns the value string of var=value options or the name of var
 * options if present in the structure.
 * Return Value: If the option was a var=val form then the return
 * string is a pointer to the val. If the option is merely a boolean
 * present option var then the pointer to the name var is returned. If
 * the option var is not found null is returned.
 */
char *rddma_get_option(struct rddma_desc_param *desc, const char *needle)
{
	char *found_var = NULL;
	char *found_val = NULL;
	int i;

	for (i = 0; i <= RDDMA_MAX_QUERY_STRINGS && desc->query[i]; i++)
		if (desc->query[i]) {
			if ((found_var = strstr(desc->query[i],needle)))
				found_val = strstr(found_var,"=");
		}
	RDDMA_DEBUG(MY_DEBUG,"%s %p,%s->%s%s\n",__FUNCTION__,desc,needle,found_var,found_val);
	return found_val ? found_val+1 : found_var ;
}
EXPORT_SYMBOL(rddma_get_option);

/**
 * rddma_parse_desc - Takes a string which is then duped and parsed into the desc struct.
 * @d: rddma_desc_param struct pointer into which the string will be
 * parsed.
 * @desc: the string to be parsed. The string is first duped and then
 * parsed into @d to enusre valid lifetimes.
 *
 * Chops string into components and assigns to param struct parts.
 * Example desc_param strings:
 * name[.loc]*[#offset][:extent][?var[=val][,var[=val]]*]
 * name[.loc]*[:extent][#offset][?var[=val][,var[=val]]*]
 *
 */

static int _rddma_parse_desc(struct rddma_desc_param *d, char *desc)
{
	int ret = 0;
	char *sextent=NULL;
	char *soffset=NULL;
	char *ops;
	char *fabric_name;
	char *dma_engine_name;
	int i;
	
	RDDMA_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,d,desc);

	d->extent = 0;
	d->offset = 0;
	d->location = NULL;
	d->query[0] = NULL;
	d->rest = NULL;
	d->ops = NULL;
	d->rde = NULL;
	d->address = NULL;
	d->buf = desc;
	d->name = desc;

	name_remainder(d->name,     '?', &d->query[0]);
	name_remainder(d->name,     '.', &d->location);

	if (d->location) {
		name_remainder(d->location, ':', &sextent);
		name_remainder(d->location, '#', &soffset);
/* 		*(d->location - 1) = '.'; */
	}
	else {
		name_remainder(d->name, ':', &sextent);
		name_remainder(d->name, '#', &soffset);
	}

	if (NULL == sextent)
		name_remainder(soffset, ':', &sextent);
	
	if (NULL == soffset)
		name_remainder(sextent, '#', &soffset);

	if (sextent) {
		d->extent = simple_strtol(sextent,&sextent,16);
 		RDDMA_ASSERT(('\0' == *sextent),"Dodgy extent string(%d) contains %s", (ret = (sextent - d->name)), sextent); 
	}

	if (soffset) {
		d->offset = simple_strtol(soffset,&soffset,16);
 		RDDMA_ASSERT(('\0' == *soffset),"Dodgy offset string(%d) contains %s", (ret = (soffset - d->name)), soffset); 
	}

	i = 0;
	while (name_remainder(d->query[i], ',', &d->query[i+1]) && i < RDDMA_MAX_QUERY_STRINGS) i++;
	/* The above naughty loop will leave any remaining options in d->rest */

	if ( (ops = rddma_get_option(d,"default_ops")) ) {
		if (!strncmp(ops,"private",7))
			d->ops = &rddma_local_ops;
		else if (!strncmp(ops,"public",6)) {
			d->ops = &rddma_fabric_ops;
		}
	}

	if ( (fabric_name = rddma_get_option(d,"fabric")) ) 
		d->address = rddma_fabric_find(fabric_name);
	
	if ( (dma_engine_name = rddma_get_option(d,"dma_name")) ) 
		d->rde = rddma_dma_find(dma_engine_name);

	return ret;
}

int rddma_parse_desc(struct rddma_desc_param *d, const char *desc)
{
	int ret = -EINVAL;
	char *mydesc;

	RDDMA_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,d,desc);
	if ( (mydesc = rddma_str_dup(desc,&d->buflen)) ) {
		if ( (ret = _rddma_parse_desc(d,mydesc)) ) {
			d->buf = NULL;
			d->buflen = 0;
			kfree(mydesc);
		}
	}
	return ret;
}

/**
 * rddma_parse_bind - Dups string and parses it into bind_param struct.
 * @x: A struct rddma_bind_param pointer into which the string is
 * parsed.
 *@desc: A string describing a transfer.
 *
 * A transfer is a string of the form desc_param/bind_param.
 */
int rddma_parse_bind(struct rddma_bind_param *x, const char *desc)
{
	int ret = -ENOMEM;
	char *mydesc;

	RDDMA_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,x,desc);
	if ( !(mydesc = rddma_str_dup(desc,0)) ) 
		goto dup_fail;
	if (! name_remainder(mydesc, '/', &x->dst.name) )
		goto xfer_fail;
	if (! name_remainder(x->dst.name, '=', &x->src.name) )
		goto bind_fail;
	if ( (ret = rddma_parse_desc( &x->xfer, mydesc )) ) 
		goto xfer_parse_fail;
	if ( (ret = rddma_parse_desc( &x->dst, x->dst.name )) )
		goto dst_parse_fail;
	if ( (ret = rddma_parse_desc( &x->src, x->src.name)) )
		goto src_parse_fail;
	
	kfree(mydesc);

	return ret;

src_parse_fail:
	rddma_clean_desc(&x->dst);
dst_parse_fail:
	rddma_clean_desc(&x->xfer);
xfer_parse_fail:
bind_fail:
xfer_fail:
	ret = -EINVAL;
	kfree(mydesc);
dup_fail:
	memset(x,0,sizeof(*x));
	return ret;
}


int rddma_clone_desc(struct rddma_desc_param *new, struct rddma_desc_param *old)
 {
	int ret = -ENOMEM;
	int i;
	RDDMA_DEBUG((RDDMA_DBG_PARSE | RDDMA_DBG_DEBUG),"%s \n",__FUNCTION__);
	*new = *old;

	if (new->address)
		rddma_fabric_get(new->address);

	if (new->rde)
		rddma_dma_get(new->rde);

	if ( old->buf && old->buflen && (new->buf = kzalloc(old->buflen, GFP_KERNEL)) ) {
		memcpy(new->buf, old->buf, old->buflen);
		if (old->location)
			new->location = new->buf + (old->location - old->buf);
		new->name = new->buf + (old->name - old->buf);
		i = 0;
		while (i <= RDDMA_MAX_QUERY_STRINGS) {
			if (old->query[i]) {
				new->query[i] = new->buf + (old->query[i] - old->buf);
			}
			i++;
		}

		return 0;
	}
	return ret;
}

int rddma_clone_bind(struct rddma_bind_param *new, struct rddma_bind_param *old)
{
	int ret = -EINVAL;
	if ( !(ret = rddma_clone_desc(&new->dst, &old->dst)) )
		if (!(ret = rddma_clone_desc(&new->src, &old->src)))
			ret = rddma_clone_desc(&new->xfer, &old->xfer);
	return ret;
}

void rddma_clean_desc(struct rddma_desc_param *p)
{
	if (p) {
		if (p->buf && p->buflen)
			kfree(p->buf);

		if (p->address)
			rddma_fabric_put(p->address);

		if (p->rde)
			rddma_dma_put(p->rde);
	}
}

void rddma_clean_bind(struct rddma_bind_param *p)
{
	rddma_clean_desc(&p->xfer);
	rddma_clean_desc(&p->dst);
	rddma_clean_desc(&p->src);
}

