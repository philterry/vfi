/* 
 * 
 * Copyright 2008 Vmetro
 * Phil Terry <pterry@vmetro.com> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define MY_DEBUG      VFI_DBG_PARSE | VFI_DBG_FUNCALL | VFI_DBG_DEBUG
#define MY_LIFE_DEBUG VFI_DBG_PARSE | VFI_DBG_LIFE    | VFI_DBG_DEBUG

#include <linux/vfi.h>
#include <linux/vfi_parse.h>
#include <linux/vfi_ops.h>
#include <linux/vfi_fabric.h>
#include <linux/slab.h>
#include <linux/string.h>

#define strtoul simple_strtoul


/**
 * vfi_str_dup - Safe copy of name to max truncated length of 4095 plus null termination in a page.
 * @name: null terminated string to be duplicated.
 *
 * @name is duplicated into a freshly allocated buffer. This
 * allocation is limited to a 4096 byte page allocation and must be
 * null terminated so the allocation is max of @name length and 4095
 * plus 1.
 */
static char *vfi_str_dup(const char *name, struct vfi_desc_param *d)
{
	char *ret = NULL;
	const char *p = name;
	size_t oldsize, size;
	int argc = 1;

	if (d)
		d->buflen = 0;

	if (name == NULL)
		return ret;

	oldsize = strnlen(name,4096) + 1;

	if (oldsize > 4096)
		return ret;

	while (*p) {
		if (*p == '?' || *p == ',') 
			argc++;
		p++;
	}
		
	size = (((oldsize >> 2) + 1) << 2) + (argc << 2);

	if (size > 4096)
		return ret;

	if ((ret = kzalloc(size, GFP_KERNEL)) ) {
		strncpy(ret, name, oldsize);
		if (d) {
			d->buflen = size;
			if (argc) {
				d->query = (char **)(ret + (((oldsize >> 2) + 1) << 2));
				d->end = d->query + (argc - 1);
			}
		}
	}

	VFI_DEBUG(MY_DEBUG,"%s %p(%d) to %p(%d)\n",__FUNCTION__,name,oldsize,ret,size);
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
	VFI_DEBUG(MY_DEBUG,"%s %s %c %p -> \n\t\t%s\n",__FUNCTION__,name,c,remainder,p);
	return p;
}
/**
 * vfi_get_option - returns value of var or var if present.
 * @desc: An vfi_desc_param structure into which a string has been
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
char *vfi_get_option(struct vfi_desc_param *desc, const char *needle)
{
	char *found_var = NULL;
	char *found_val = NULL;
	char **query = desc->query;

	VFI_DEBUG(MY_DEBUG,"%s desc(%p) desc->query(%p)\n",__FUNCTION__,desc,desc->query);

	if (query)
		while (*query && (found_var == NULL)) {
			VFI_DEBUG(MY_DEBUG,"%s query(%p) *query(%p)\n",__FUNCTION__,query,*query);
			if ((found_var = strstr(*query,needle)))
				found_val = strstr(found_var,"=");
			query++;
		}

	VFI_DEBUG(MY_DEBUG,"%s %p,%s->%s\n",__FUNCTION__,desc,needle,found_val ? found_val+1 : found_var);

	return found_val ? found_val+1 : found_var ;
}
EXPORT_SYMBOL(vfi_get_option);

int vfi_add_option(struct vfi_desc_param *desc, char *opt)
{
	int additional,opt_space,opt_len,nl,ol;
	char *ob, *nb, *nopt;
	char **oq, **nq;
	
	opt_len = strlen(opt) + 1;
	opt_space = ((opt_len >> 2) + 1) << 2;
	additional =  opt_space + 4;

	ol = desc->buflen;
	nl = ol + additional;

	nb = kzalloc(nl, GFP_KERNEL);

	if (nb == NULL)
		return -ENOMEM;

	ob = desc->buf;
	oq = desc->query;
	nq = (char **)(nb + ((char *)oq - ob) + opt_space);

	memcpy(nb, ob, ol);
	nopt = nb + ((char *)oq - ob);
	memcpy(nopt,opt,opt_len);
	
	desc->query = nq;
	while (*oq) {
		*nq = nb + ((char *)oq - ob);
		oq++;
		nq++;
	}

	*nq = nopt;
	desc->buf = nb;
	desc->buflen = nl;
	desc->name = nb + (desc->name - ob);
	desc->location = nb + (desc->location - ob);
	desc->end = nq+1;
	kfree(ob);
	return 0;
}

static void vfi_parse_options(char **query)
{
	char *q;

	while (*query) {
		name_remainder(*query, ',', query+1);
		if ((q = strstr(*query,"("))) {
			*q = '=';
			if ((q = strstr(q,")")))
				*q = '\0';
		}
		VFI_DEBUG(MY_DEBUG,"%s: %s\n",__FUNCTION__,*query);
		query++;
	}
}

/**
 * vfi_parse_desc - Takes a string which is then duped and parsed into the desc struct.
 * @d: vfi_desc_param struct pointer into which the string will be
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

static int _vfi_parse_desc(struct vfi_desc_param *d, char *desc)
{
	int ret = 0;
	char *sextent=NULL;
	char *soffset=NULL;
	char *ops;
	char *fabric_name;
	char *dma_engine_name;
	
	VFI_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,d,desc);

	d->extent = 0;
	d->offset = 0;
	d->soffset = NULL;
	d->sextent = NULL;
	d->location = NULL;
	d->ops = NULL;
	d->rde = NULL;
	d->address = NULL;
	d->ploc = NULL;
	d->buf = desc;
	ops = strstr(desc,"://");
	d->name = ops ? ops+3 : desc;

	name_remainder(d->name,     '?', &d->query[0]);
	name_remainder(d->name,     '.', &d->location);

	/* If desc name started with ., above will give a null name
	 * value and a location pointer, so try again, while solves a string of .s */
	while ( (*d->name == '\0') && d->location) {
		d->name = d->location;
		name_remainder(d->name, '.', &d->location);
	}

	if (d->location) {
		name_remainder(d->location, ':', &sextent);
		name_remainder(d->location, '#', &soffset);
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
		d->extent = simple_strtoul(sextent,&sextent,16);
		d->sextent = sextent;
 		VFI_ASSERT(('\0' == *sextent),"Dodgy extent string(%d) contains %s", (ret = (sextent - d->name)), sextent); 
	}

	if (soffset) {
		d->offset = simple_strtoul(soffset,&soffset,16);
		d->soffset = soffset;
 		VFI_ASSERT(('\0' == *soffset),"Dodgy offset string(%d) contains %s", (ret = (soffset - d->name)), soffset); 
	}

	vfi_parse_options(d->query);

	if ( (ops = vfi_get_option(d,"default_ops")) ) {
		if (!strncmp(ops,"private",7))
			d->ops = &vfi_local_ops;
		else if (!strncmp(ops,"public",6)) {
			d->ops = &vfi_fabric_ops;
		}
	}

	if ( (fabric_name = vfi_get_option(d,"fabric")) ) 
		d->address = vfi_fabric_find(fabric_name);
	
	if ( (dma_engine_name = vfi_get_option(d,"dma_name")) ) 
		d->rde = vfi_dma_find(dma_engine_name);

	return ret;
}

/**
* vfi_parse_desc - duplicate, then parse, command string
* @d    - descriptor to receive results
* @desc - string to be parsed
*
* This function provides the front-end command string parser. 
* It duplicates the original @desc string, and parses the duplicate, 
* saving the results in the caller-provided @d.
*
* Duplication allows the string to be sliced and diced without
* affecting the original, and allows the string to be safely
* discarded later via its dewscriptor. In short, every descriptor
* ought to refer to its own copy of a string.
*
**/
int vfi_parse_desc(struct vfi_desc_param *d, const char *desc)
{
	int ret = -EINVAL;
	char *mydesc;

	VFI_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,d,desc);
	if ( (mydesc = vfi_str_dup(desc,d)) ) {
		if ( (ret = _vfi_parse_desc(d,mydesc)) ) {
			d->buf = NULL;
			d->buflen = 0;
			kfree(mydesc);
		}
	}
	return ret;
}

/**
 * vfi_parse_bind - Dups string and parses it into bind_param struct.
 * @x: A struct vfi_bind_param pointer into which the string is
 * parsed.
 *@desc: A string describing a transfer.
 *
 * A bind is defined by three terms, with optional parameters embedded in
 * each. The general form is:
 *
 *	<xfer-spec>/<dst-spec>=<src-spec>
 *
 * Where the three terms are fully-qualified Xfer, Destination, and Source
 * specifications.
 *
 * We parse a bind by isolating its three required terms, then parsing each
 * of those as a separate descriptor.
 *
 * As illustrated above, the <xfer-spec> term is terminated by a '/', the
 * <dst-spec> term is terminated by '=', and the <src-spec> term is terminated
 * by the end-of-string.
 *
 */
int vfi_parse_bind(struct vfi_bind_param *x, const char *desc)
{
	int ret;
	char *myxfer, *mydst, *mysrc;

	VFI_DEBUG(MY_DEBUG,"%s %p,%s\n",__FUNCTION__,x,desc);

	memset(x,0,sizeof(*x));

	myxfer = vfi_str_dup(desc,0);

	if ( myxfer == NULL ) 
		return -ENOMEM;

	/*
	* Separate <xfer-spec> from <dst-spec>=<src-spec>
	*/
	mydst = strstr(myxfer,"://");
	if (mydst)
		mydst = mydst+3;
	else
		mydst = myxfer;

	name_remainder(mydst, '/', &mydst);

	/*
	* Separate <dst-spec> from <src-spec> and parse
	* each as a standalone descriptor.
	*/
	if (mydst) {
		name_remainder(mydst, '=', &mysrc);

		if (mysrc) {
			ret = vfi_parse_desc( &x->src, mysrc);
			if (ret)
				goto out;
		}

		ret = vfi_parse_desc( &x->dst, mydst);
		if (ret)
			goto out;
	}

	/*
	* Parse <xfer-spec> as a standalone descriptor.
	*/
	ret = vfi_parse_desc( &x->xfer, myxfer );
out:
	kfree(myxfer);

	return ret;
}

void vfi_update_ploc(struct vfi_desc_param *desc, struct vfi_location *loc)
{
	if (desc) {
		vfi_location_put(desc->ploc);
		desc->ploc = vfi_location_get(loc);
	}
}

void vfi_inherit(struct vfi_desc_param *c, struct vfi_desc_param *p)
{
	vfi_fabric_put(c->address);
	c->address = vfi_fabric_get(p->address);
	
	vfi_dma_put(c->rde);
	c->rde = vfi_dma_get(p->rde);

	vfi_location_put(c->ploc);
	c->ploc = vfi_location_get(p->ploc);

	c->ops = p->ops;
}

int vfi_clone_desc(struct vfi_desc_param *new, struct vfi_desc_param *old)
 {
	int ret = -ENOMEM;
	int i;
	VFI_DEBUG(MY_DEBUG,"%s new(%p) old(%p)\n",__FUNCTION__,new,old);

	vfi_fabric_put(new->address);
	vfi_dma_put(new->rde);
	vfi_location_put(new->ploc);
	*new = *old;
	vfi_fabric_get(new->address);
	vfi_dma_get(new->rde);
	vfi_location_get(new->ploc);

	if ( old->buf && old->buflen && (new->buf = kzalloc(old->buflen, GFP_KERNEL)) ) {
		memcpy(new->buf, old->buf, old->buflen);
		if (old->location)
			new->location = new->buf + (old->location - old->buf);
		new->name = new->buf + (old->name - old->buf);
		new->query = (char **)(new->buf + ((char *)old->query - old->buf));
		new->soffset = ((old->soffset) ? ((char *)old->soffset - old->buf) : NULL);
		new->sextent = ((old->sextent) ? ((char *)old->sextent - old->buf) : NULL);
		i = 0;
		while (old->query[i]) {
			new->query[i] = new->buf + (old->query[i] - old->buf);
			i++;
		}

		return 0;
	}
	return ret;
}

int vfi_clone_bind(struct vfi_bind_param *new, struct vfi_bind_param *old)
{
	int ret = -EINVAL;
	if ( !(ret = vfi_clone_desc(&new->dst, &old->dst)) )
		if (!(ret = vfi_clone_desc(&new->src, &old->src)))
			ret = vfi_clone_desc(&new->xfer, &old->xfer);
	return ret;
}

void vfi_clean_desc(struct vfi_desc_param *p)
{
	if (p) {
		if (p->buf && p->buflen)
			kfree(p->buf);

		vfi_fabric_put(p->address);
		vfi_dma_put(p->rde);
		vfi_location_put(p->ploc);
	}
}

void vfi_clean_bind(struct vfi_bind_param *p)
{
	vfi_clean_desc(&p->xfer);
	vfi_clean_desc(&p->dst);
	vfi_clean_desc(&p->src);
}

