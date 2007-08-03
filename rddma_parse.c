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

#include <linux/rddma.h>
#include <linux/rddma_parse.h>
#include <linux/rddma_ops.h>

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
static char *rddma_str_dup(const char *name)
{
	char *ret = NULL;
	if (name) {
		size_t size = strnlen(name,4095);
		if ((ret = kzalloc(size+1, GFP_KERNEL)) )
			strncpy(ret, name, size);
	}
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
		*p++ = 0;

	if (remainder)
		*remainder = p;

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
		if (desc->query[i])
			if ((found_var = strstr(desc->query[i],needle)))
				found_val = strstr(found_var,"=");

	return found_val ? found_val+1 : found_var ;
}

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
	d->dma_ops = NULL;

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

	if ( (ops = rddma_get_option(d,"dma_ops")) ) {
		if (!strncmp(ops,"fabric",7))
			d->ops = NULL; /* FIX ME */
		else if (!strncmp(ops,"debug",6))
			d->ops = NULL; /* FIX ME */
	}

	return 0;
out:
	return ret;
}

/**
 * rddma_parse_bind - Dupe string and parse it into bind_param struct.
 * @b: struct rddma_bind_param pointer into which @desc is parsed.
 * @desc: a string describing a bind.
 *
 * A bind is a string of the form desc_param=desc_param.
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
 * rddma_parse_xfer - Dups string and parses it into xfer_param struct.
 * @x: A struct rddma_xfer_param pointer into which the string is
 * parsed.
 *@desc: A string describing a transfer.
 *
 * A transfer is a string of the form desc_param/bind_param.
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


