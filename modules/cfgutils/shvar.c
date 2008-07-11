/*
 * $Id$
 *
 * Copyright (C) 2007 Elena-Ramona Modroiu
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../pvar.h"

#include "shvar.h"

int shvar_locks_no=16;
gen_lock_set_t* shvar_locks=0;

static sh_var_t *sh_vars = 0;
static str shv_cpy = {0, 0};
static script_var_t *sh_local_vars = 0;
static pv_spec_list_t *sh_pv_list = 0;
static int shvar_initialized = 0;

/*
 * Initialize locks
 */
int shvar_init_locks(void)
{
	int i;
	i = shvar_locks_no;
	do {
		if ((( shvar_locks=lock_set_alloc(i))!=0)&&
				(lock_set_init(shvar_locks)!=0))
		{
			shvar_locks_no = i;
			LM_INFO("locks array size %d\n", shvar_locks_no);
			return 0;

		}
		if (shvar_locks){
			lock_set_dealloc(shvar_locks);
			shvar_locks=0;
		}
		i--;
		if(i==0)
		{
			LM_ERR("failed to allocate locks\n");
			return -1;
		}
	} while (1);
}


void shvar_unlock_locks(void)
{
	unsigned int i;

	if (shvar_locks==0)
		return;

	for (i=0;i<shvar_locks_no;i++) {
#ifdef GEN_LOCK_T_PREFERED
		lock_release(&shvar_locks->locks[i]);
#else
		shvar_release_idx(i);
#endif
	};
}


void shvar_destroy_locks(void)
{
	if (shvar_locks !=0){
		lock_set_destroy(shvar_locks);
		lock_set_dealloc(shvar_locks);
	}
}

#ifndef GEN_LOCK_T_PREFERED
void shvar_lock_idx(int idx)
{
	lock_set_get(shvar_locks, idx);
}

void shvar_release_idx(int idx)
{
	lock_set_release(shvar_locks, idx);
}
#endif

/*
 * Get lock
 */
void lock_shvar(sh_var_t *shv)
{
	if(shv==NULL)
		return;
#ifdef GEN_LOCK_T_PREFERED
	lock_get(shv->lock);
#else
	ul_lock_idx(shv->lockidx);
#endif
}


/*
 * Release lock
 */
void unlock_shvar(sh_var_t *shv)
{
	if(shv==NULL)
		return;
#ifdef GEN_LOCK_T_PREFERED
	lock_release(shv->lock);
#else
	ul_release_idx(shv->lockidx);
#endif
}


sh_var_t* add_shvar(str *name)
{
	sh_var_t *sit;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(sit=sh_vars; sit; sit=sit->next)
	{
		if(sit->name.len==name->len
				&& strncmp(name->s, sit->name.s, name->len)==0)
			return sit;
	}
	sit = (sh_var_t*)shm_malloc(sizeof(sh_var_t));
	if(sit==0)
	{
		LM_ERR("out of shm\n");
		return 0;
	}
	memset(sit, 0, sizeof(sh_var_t));
	sit->name.s = (char*)shm_malloc((name->len+1)*sizeof(char));

	if(sit->name.s==0)
	{
		LM_ERR("out of shm!\n");
		shm_free(sit);
		return 0;
	}
	sit->name.len = name->len;
	strncpy(sit->name.s, name->s, name->len);
	sit->name.s[sit->name.len] = '\0';

	if(sh_vars!=0)
		sit->n = sh_vars->n + 1;
	else
		sit->n = 1;

#ifdef GEN_LOCK_T_PREFERED
	sit->lock = &shvar_locks->locks[sit->n%shvar_locks_no];
#else
	sit->lockidx = sit->n%shvar_locks_no;
#endif

	sit->next = sh_vars;

	sh_vars = sit;

	return sit;
}

script_var_t* add_local_shvar(str *name)
{
	script_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(it=sh_local_vars; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	it = (script_var_t*)pkg_malloc(sizeof(script_var_t));
	if(it==0)
	{
		LM_ERR("out of pkg mem\n");
		return 0;
	}
	memset(it, 0, sizeof(script_var_t));
	it->name.s = (char*)pkg_malloc((name->len+1)*sizeof(char));

	if(it->name.s==0)
	{
		LM_ERR("out of pkg mem!\n");
		return 0;
	}
	it->name.len = name->len;
	strncpy(it->name.s, name->s, name->len);
	it->name.s[it->name.len] = '\0';

	it->next = sh_local_vars;

	sh_local_vars = it;

	return it;
}


int init_shvars(void)
{
	script_var_t *lit = 0;
	sh_var_t *sit = 0;
	pv_spec_list_t *pvi = 0;
	pv_spec_list_t *pvi0 = 0;

	if(shvar_init_locks()!=0)
		return -1;

	LM_DBG("moving shvars in share memory\n");
	for(lit=sh_local_vars; lit; lit=lit->next)
	{
		sit = (sh_var_t*)shm_malloc(sizeof(sh_var_t));
		if(sit==0)
		{
			LM_ERR("out of sh mem\n");
			return -1;
		}
		memset(sit, 0, sizeof(sh_var_t));
		sit->name.s = (char*)shm_malloc((lit->name.len+1)*sizeof(char));

		if(sit->name.s==0)
		{
			LM_ERR("out of pkg mem!\n");
			shm_free(sit);
			return -1;
		}
		sit->name.len = lit->name.len;
		strncpy(sit->name.s, lit->name.s, lit->name.len);
		sit->name.s[sit->name.len] = '\0';

		if(sh_vars!=0)
			sit->n = sh_vars->n + 1;
		else
			sit->n = 1;

#ifdef GEN_LOCK_T_PREFERED
		sit->lock = &shvar_locks->locks[sit->n%shvar_locks_no];
#else
		sit->lockidx = sit->n%shvar_locks_no;
#endif

		if(set_shvar_value(sit, &lit->v.value, lit->v.flags)==NULL)
		{
			shm_free(sit->name.s);
			shm_free(sit);
			return -1;
		}

		pvi0 = 0;
		pvi = sh_pv_list;
		while(pvi!=NULL)
		{
			if(pvi->spec->pvp.pvn.u.dname == lit)
			{
				pvi->spec->pvp.pvn.u.dname = (void*)sit;
				if(pvi0!=NULL)
				{
					pvi0->next = pvi->next;
					pkg_free(pvi);
					pvi = pvi0->next;
				} else {
					sh_pv_list = pvi->next;
					pkg_free(pvi);
					pvi = sh_pv_list;
				}
			} else {
				pvi0 = pvi;
				pvi = pvi->next;
			}
		}

		sit->next = sh_vars;
		sh_vars = sit;
	}
	destroy_vars_list(sh_local_vars);
	if(sh_pv_list != NULL)
	{
		LM_ERR("sh_pv_list not null!\n");
		return -1;
	}
	shvar_initialized = 1;
	return 0;
}

/* call it with lock set */
sh_var_t* set_shvar_value(sh_var_t* shv, int_str *value, int flags)
{
	if(shv==NULL)
		return NULL;
	if(value==NULL)
	{
		if(shv->v.flags&VAR_VAL_STR)
		{
			shm_free(shv->v.value.s.s);
			shv->v.flags &= ~VAR_VAL_STR;
		}
		memset(&shv->v.value, 0, sizeof(int_str));

		return shv;
	}

	if(flags&VAR_VAL_STR)
	{
		if(shv->v.flags&VAR_VAL_STR)
		{ /* old and new value is str */
			if(value->s.len>shv->v.value.s.len)
			{ /* not enough space to copy */
				shm_free(shv->v.value.s.s);
				memset(&shv->v.value, 0, sizeof(int_str));
				shv->v.value.s.s =
					(char*)shm_malloc((value->s.len+1)*sizeof(char));
				if(shv->v.value.s.s==0)
				{
					LM_ERR("out of shm\n");
					goto error;
				}
			}
		} else {
			memset(&shv->v.value, 0, sizeof(int_str));
			shv->v.value.s.s =
					(char*)shm_malloc((value->s.len+1)*sizeof(char));
			if(shv->v.value.s.s==0)
			{
				LM_ERR("out of shm!\n");
				goto error;
			}
			shv->v.flags |= VAR_VAL_STR;
		}
		strncpy(shv->v.value.s.s, value->s.s, value->s.len);
		shv->v.value.s.len = value->s.len;
		shv->v.value.s.s[value->s.len] = '\0';
	} else {
		if(shv->v.flags&VAR_VAL_STR)
		{
			shm_free(shv->v.value.s.s);
			shv->v.flags &= ~VAR_VAL_STR;
			memset(&shv->v.value, 0, sizeof(int_str));
		}
		shv->v.value.n = value->n;
	}

	return shv;
error:
	/* set the var to init value */
	memset(&shv->v.value, 0, sizeof(int_str));
	shv->v.flags &= ~VAR_VAL_STR;
	return NULL;
}

sh_var_t* get_shvar_by_name(str *name)
{
	sh_var_t *it;

	if(name==0 || name->s==0 || name->len<=0)
		return 0;

	for(it=sh_vars; it; it=it->next)
	{
		if(it->name.len==name->len
				&& strncmp(name->s, it->name.s, name->len)==0)
			return it;
	}
	return 0;
}

void reset_shvars(void)
{
	sh_var_t *it;
	for(it=sh_vars; it; it=it->next)
	{
		if(it->v.flags&VAR_VAL_STR)
		{
			shm_free(it->v.value.s.s);
			it->v.flags &= ~VAR_VAL_STR;
		}
		memset(&it->v.value, 0, sizeof(int_str));
	}
}

void destroy_shvars(void)
{
	sh_var_t *it;
	sh_var_t *it0;

	it = sh_vars;
	while(it)
	{
		it0 = it;
		it = it->next;
		shm_free(it0->name.s);
		if(it0->v.flags&VAR_VAL_STR)
			shm_free(it0->v.value.s.s);
		shm_free(it0);
	}

	sh_vars = 0;
}


/********* PV functions *********/
int pv_parse_shvar_name(pv_spec_p sp, str *in)
{
	pv_spec_list_t *pvi = 0;
	
	if(in==NULL || in->s==NULL || sp==NULL)
		return -1;
	
	sp->pvp.pvn.type = PV_NAME_PVAR;
	if(shvar_initialized)
		sp->pvp.pvn.u.dname = (void*)add_shvar(in);
	else
		sp->pvp.pvn.u.dname = (void*)add_local_shvar(in);
	if(sp->pvp.pvn.u.dname==NULL)
	{
		LM_ERR("cannot register shvar [%.*s] (%d)\n", in->len, in->s,
				shvar_initialized);
		return -1;
	}

	if(shvar_initialized==0)
	{
		pvi = (pv_spec_list_t*)pkg_malloc(sizeof(pv_spec_list_t));
		if(pvi == NULL)
		{
			LM_ERR("cannot index shvar [%.*s]\n", in->len, in->s);
			return -1;
		}
		pvi->spec = sp;
		pvi->next = sh_pv_list;
		sh_pv_list = pvi;
	}

	return 0;
}

int pv_get_shvar(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	int len = 0;
	char *sval = NULL;
	sh_var_t *shv=NULL;
	
	if(msg==NULL || res==NULL)
		return -1;

	if(param==NULL || param->pvn.u.dname==0)
		return pv_get_null(msg, param, res);
	
	shv= (sh_var_t*)param->pvn.u.dname;

	lock_shvar(shv);
	if(shv->v.flags&VAR_VAL_STR)
	{
		if(shv_cpy.s==NULL || shv_cpy.len < shv->v.value.s.len)
		{
			if(shv_cpy.s!=NULL)
				pkg_free(shv_cpy.s);
			shv_cpy.s = (char*)pkg_malloc(shv->v.value.s.len*sizeof(char));
			if(shv_cpy.s==NULL)
			{
				unlock_shvar(shv);
				LM_ERR("no more pkg mem\n");
				return pv_get_null(msg, param, res);
			}
		}
		strncpy(shv_cpy.s, shv->v.value.s.s, shv->v.value.s.len);
		shv_cpy.len = shv->v.value.s.len;
		
		unlock_shvar(shv);
		
		res->rs = shv_cpy;
		res->flags = PV_VAL_STR;
	} else {
		res->ri = shv->v.value.n;
		
		unlock_shvar(shv);
		
		sval = sint2str(res->ri, &len);
		res->rs.s = sval;
		res->rs.len = len;
		res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	}
	return 0;
}

int pv_set_shvar(struct sip_msg* msg, pv_param_t *param,
		int op, pv_value_t *val)
{
	int_str isv;
	int flags;

	if(param==NULL)
	{
		LM_ERR("bad parameters\n");
		return -1;
	}

	if(param->pvn.u.dname==0)
	{
		LM_ERR("error - cannot find shvar\n");
		goto error;
	}
	lock_shvar((sh_var_t*)param->pvn.u.dname);
	if(val == NULL)
	{
		isv.n = 0;
		set_shvar_value((sh_var_t*)param->pvn.u.dname, &isv, 0);
		goto done;
	}
	flags = 0;
	if(val->flags&PV_TYPE_INT)
	{
		isv.n = val->ri;
	} else {
		isv.s = val->rs;
		flags |= VAR_VAL_STR;
	}
	if(set_shvar_value((sh_var_t*)param->pvn.u.dname, &isv, flags)==NULL)
	{
		LM_ERR("error - cannot set shvar [%.*s] \n",
				((sh_var_t*)param->pvn.u.dname)->name.len,
				((sh_var_t*)param->pvn.u.dname)->name.s);
		goto error;
	}
done:
	unlock_shvar((sh_var_t*)param->pvn.u.dname);
	return 0;
error:
	unlock_shvar((sh_var_t*)param->pvn.u.dname);
	return -1;
}

struct mi_root* mi_shvar_set(struct mi_root* cmd_tree, void* param)
{
	str sp;
	str name;
	int ival;
	int_str isv;
	int flags;
	struct mi_node* node;
	sh_var_t *shv = NULL;

	node = cmd_tree->node.kids;
	if(node == NULL)
		return init_mi_tree( 400, MI_SSTR(MI_MISSING_PARM_S));
	name = node->value;
	if(name.len<=0 || name.s==NULL)
	{
		LM_ERR("bad shv name\n");
		return init_mi_tree( 500, MI_SSTR("bad shv name"));
	}
	shv = get_shvar_by_name(&name);
	if(shv==NULL)
		return init_mi_tree(404, MI_SSTR("Not found"));

	node = node->next;
	if(node == NULL)
		return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM_S));
	sp = node->value;
	if(sp.s == NULL)
		return init_mi_tree(500, MI_SSTR("type not found"));
	flags = 0;
	if(sp.s[0]=='s' || sp.s[0]=='S')
		flags = VAR_VAL_STR;

	node= node->next;
	if(node == NULL)
		return init_mi_tree(400, MI_SSTR(MI_MISSING_PARM_S));

	sp = node->value;
	if(sp.s == NULL)
	{
		return init_mi_tree(500, MI_SSTR("value not found"));
	}
	if(flags == 0)
	{
		if(str2sint(&sp, &ival))
		{
			LM_ERR("bad integer value\n");
			return init_mi_tree( 500, MI_SSTR("bad integer value"));
		}
		isv.n = ival;
	} else {
		isv.s = sp;
	}

	lock_shvar(shv);
	if(set_shvar_value(shv, &isv, flags)==NULL)
	{
		unlock_shvar(shv);
		LM_ERR("cannot set shv value\n");
		return init_mi_tree( 500, MI_SSTR("cannot set shv value"));
	}

	unlock_shvar(shv);
	LM_DBG("$shv(%.*s) updated\n", name.len, name.s);
	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

struct mi_root* mi_shvar_get(struct mi_root* cmd_tree, void* param)
{
	struct mi_root* rpl_tree = NULL;
	struct mi_node* node;
	struct mi_attr* attr = NULL;
	str name;
	int ival;
	sh_var_t *shv = NULL;

	node = cmd_tree->node.kids;
	if(node != NULL)
	{
		name = node->value;
		if(name.len<=0 || name.s==NULL)
		{
			LM_ERR("bad shv name\n");
			return init_mi_tree( 500, MI_SSTR("bad shv name"));
		}
		shv = get_shvar_by_name(&name);
		if(shv==NULL)
			return init_mi_tree(404, MI_SSTR("Not found"));
		
		rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==NULL)
			return NULL;

		node = add_mi_node_child(&rpl_tree->node, MI_DUP_VALUE,
				"VAR",3, name.s, name.len);
  		if(node == NULL)
  			goto error;
		lock_shvar(shv);
		if(shv->v.flags&VAR_VAL_STR)
		{
			attr = add_mi_attr (node, MI_DUP_VALUE, "type", 4, "string", 6);
			if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			attr = add_mi_attr (node, MI_DUP_VALUE, "value", 5,
					shv->v.value.s.s, shv->v.value.s.len);
	  		if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			unlock_shvar(shv);
		} else {
			ival = shv->v.value.n;
			unlock_shvar(shv);
			attr = add_mi_attr (node, MI_DUP_VALUE, "type",4, "integer", 7);
			if(attr == 0)
				goto error;
			name.s = sint2str(ival, &name.len);
			attr = add_mi_attr (node, MI_DUP_VALUE, "value",5,
					name.s, name.len);
	  		if(attr == 0)
				goto error;
		}

		goto done;
	}

	rpl_tree = init_mi_tree(200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL)
		return NULL;

	for(shv=sh_vars; shv; shv=shv->next)
	{
		node = add_mi_node_child(&rpl_tree->node, MI_DUP_VALUE,
				"VAR", 3, shv->name.s, shv->name.len);
  		if(node == NULL)
  			goto error;

		lock_shvar(shv);
		if(shv->v.flags&VAR_VAL_STR)
		{
			attr = add_mi_attr (node, MI_DUP_VALUE, "type", 4, "string", 6);
			if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			attr = add_mi_attr (node, MI_DUP_VALUE, "value", 5,
					shv->v.value.s.s, shv->v.value.s.len);
	  		if(attr == 0)
			{
				unlock_shvar(shv);
				goto error;
			}
			unlock_shvar(shv);
		} else {
			ival = shv->v.value.n;
			unlock_shvar(shv);
			attr = add_mi_attr (node, MI_DUP_VALUE, "type",4, "integer", 7);
			if(attr == 0)
				goto error;
			name.s = sint2str(ival, &name.len);
			attr = add_mi_attr (node, MI_DUP_VALUE, "value",5,
					name.s, name.len);
	  		if(attr == 0)
				goto error;
		}
	}

done:
	return rpl_tree;
error:
	if(rpl_tree!=NULL)
		free_mi_tree(rpl_tree);
	return NULL;
}

int param_set_xvar( modparam_t type, void* val, int mode)
{
	str s;
	char *p;
	int_str isv;
	int flags;
	int ival;
	script_var_t *sv;

	if(shvar_initialized!=0)
		goto error;

	s.s = (char*)val;
	if(s.s == NULL || s.s[0] == '\0')
		goto error;

	p = s.s;
	while(*p && *p!='=') p++;

	if(*p!='=')
		goto error;
	
	s.len = p - s.s;
	if(s.len == 0)
		goto error;
	p++;
	flags = 0;
	if(*p!='s' && *p!='S' && *p!='i' && *p!='I')
		goto error;

	if(*p=='s' || *p=='S')
		flags = VAR_VAL_STR;
	p++;
	if(*p!=':')
		goto error;
	p++;
	isv.s.s = p;
	isv.s.len = strlen(p);
	if(flags != VAR_VAL_STR) {
		if(str2sint(&isv.s, &ival)<0)
			goto error;
		isv.n = ival;
	}
	if(mode==0)
		sv = add_var(&s);
	else
		sv = add_local_shvar(&s);
	if(sv==NULL)
		goto error;
	if(set_var_value(sv, &isv, flags)==NULL)
		goto error;
	
	return 0;
error:
	LM_ERR("unable to set shv parame [%s]\n", s.s);
	return -1;
}

int param_set_var( modparam_t type, void* val)
{
	return param_set_xvar(type, val, 0);
}

int param_set_shvar( modparam_t type, void* val)
{
	return param_set_xvar(type, val, 1);
}


/*** $time(name) PV class */

int pv_parse_time_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 3: 
			if(strncmp(in->s, "sec", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else if(strncmp(in->s, "min", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else if(strncmp(in->s, "mon", 3)==0)
				sp->pvp.pvn.u.isname.name.n = 4;
			else goto error;
		break;
		case 4: 
			if(strncmp(in->s, "hour", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 2;
			else if(strncmp(in->s, "mday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 3;
			else if(strncmp(in->s, "year", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 5;
			else if(strncmp(in->s, "wday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 6;
			else if(strncmp(in->s, "yday", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 7;
			else goto error;
		break;
		case 5: 
			if(strncmp(in->s, "isdst", 5)==0)
				sp->pvp.pvn.u.isname.name.n = 8;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV time name %.*s\n", in->len, in->s);
	return -1;
}

static struct tm _cfgutils_ts;
static unsigned int _cfgutils_msg_id = 0;

int pv_get_time(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	time_t t;

	if(msg==NULL || param==NULL)
		return -1;

	if(_cfgutils_msg_id != msg->id)
	{
		pv_update_time(msg, &t);
		_cfgutils_msg_id = msg->id;
		if(localtime_r(&t, &_cfgutils_ts) == NULL)
		{
			LM_ERR("unable to break time to attributes\n");
			return -1;
		}
	}
	
	switch(param->pvn.u.isname.name.n)
	{
		case 1:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_min);
		case 2:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_hour);
		case 3:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_mday);
		case 4:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_mon+1));
		case 5:
			return pv_get_uintval(msg, param, res,
					(unsigned int)(_cfgutils_ts.tm_year+1900));
		case 6:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_wday+1));
		case 7:
			return pv_get_uintval(msg, param, res, 
					(unsigned int)(_cfgutils_ts.tm_yday+1));
		case 8:
			return pv_get_sintval(msg, param, res, _cfgutils_ts.tm_isdst);
		default:
			return pv_get_uintval(msg, param, res, (unsigned int)_cfgutils_ts.tm_sec);
	}
}

