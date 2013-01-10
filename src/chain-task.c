/*
 * media-service-upnp
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Ludovic Ferrandis <ludovic.ferrandis@intel.com>
 *
 */

#include <glib.h>

#include "chain-task.h"
#include "device.h"
#include "log.h"

typedef struct msu_chain_task_atom_t_ msu_chain_task_atom_t;
struct msu_chain_task_atom_t_ {
	GUPnPServiceProxyActionCallback callback;
	GUPnPServiceProxyAction *p_action;
	GDestroyNotify free_func;
	gpointer user_data;
	msu_chain_task_action t_action;
	msu_device_t *device;
	GUPnPServiceProxy *proxy;
};

struct msu_chain_task_t_ {
	GList *task_list;
	msu_chain_task_atom_t *current;
	msu_chain_task_end end_func;
	GUPnPServiceProxy *end_proxy;
	GDestroyNotify end_free_func;
	gpointer end_data;
	gboolean canceled;
	guint idle_id;
};

static void prv_free_atom(msu_chain_task_atom_t *atom)
{
	if (atom->free_func != NULL)
		atom->free_func(atom->user_data);

	if (atom->proxy != NULL)
		g_object_remove_weak_pointer((G_OBJECT(atom->proxy)),
					     (gpointer *)&atom->proxy);

	g_free(atom);
}

static gboolean prv_idle_end_func(gpointer user_data)
{
	msu_chain_task_t *chain = (msu_chain_task_t *)user_data;

	chain->end_func(chain, chain->end_proxy, chain->end_data);

	if (chain->end_free_func) {
		chain->end_free_func(chain->end_data);
		chain->end_data = NULL;
		chain->end_free_func = NULL;
	}

	msu_chain_task_delete(chain);

	return FALSE;
}

static gboolean prv_idle_next_task(gpointer user_data)
{
	msu_chain_task_t *chain = (msu_chain_task_t *)user_data;
	GList *head = chain->task_list;

	chain->current = NULL;
	chain->task_list = g_list_remove_link(chain->task_list, head);

	if (head != NULL) {
		prv_free_atom(head->data);
		g_list_free_1(head);
	}

	chain->idle_id = 0;
	msu_chain_task_start(chain);

	return FALSE;
}

static void prv_next_task(msu_chain_task_t *chain)
{
	chain->idle_id = g_idle_add(prv_idle_next_task, chain);
}

gboolean msu_chain_task_is_canceled(msu_chain_task_t *chain)
{
	return chain->canceled;
}

msu_device_t *msu_chain_task_get_device(msu_chain_task_t *chain)
{
	if ((chain != NULL) && (chain->current != NULL))
		return chain->current->device;

	return NULL;
}

gpointer *msu_chain_task_get_user_data(msu_chain_task_t *chain)
{
	if ((chain != NULL) && (chain->current != NULL))
		return chain->current->user_data;

	return NULL;
}

gpointer *msu_chain_task_get_end_data(msu_chain_task_t *chain)
{
	if (chain != NULL)
		return chain->end_data;

	return NULL;
}

void msu_chain_task_cancel(msu_chain_task_t *chain)
{
	if (chain->current && chain->current->p_action) {
		if (chain->current->proxy)
			gupnp_service_proxy_cancel_action(
						chain->current->proxy,
						chain->current->p_action);
		chain->current->p_action = NULL;

		prv_next_task(chain);
	}

	chain->canceled = TRUE;
}

void msu_chain_task_begin_action_cb(GUPnPServiceProxy *proxy,
				    GUPnPServiceProxyAction *action,
				    gpointer user_data)
{
	msu_chain_task_t *chain = (msu_chain_task_t *)user_data;
	msu_chain_task_atom_t *current;

	if (chain != NULL) {
		current = chain->current;

		if (chain->current != NULL) {
			chain->current->p_action = NULL;
			current->callback(proxy, action, current->user_data);
		}

		prv_next_task(chain);
	}
}

void msu_chain_task_start(msu_chain_task_t *chain)
{
	gboolean failed = FALSE;
	msu_chain_task_atom_t *current;

	if ((chain->task_list != NULL) && (!chain->canceled)) {
		current = chain->task_list->data;
		chain->current = current;
		current->p_action = current->t_action(chain,
						      current->proxy,
						      &failed);
		if (failed)
			chain->canceled = TRUE;

		/* Call next task if current task is synchronous or if
		 * current task is asynchronous, failed and
		 * gupnp_service_proxy_begin_action has not been called.
		 * In this latest case, prv_next_task will call end_func
		 */
		if (current->p_action == NULL &&
		    (current->callback == NULL ||
		     chain->canceled == TRUE))
			prv_next_task(chain);
	} else {
		if (chain->end_func)
			chain->idle_id = g_idle_add(prv_idle_end_func, chain);
	}
}

void msu_chain_task_set_end(msu_chain_task_t *chain,
			    msu_chain_task_end end_func,
			    GUPnPServiceProxy *proxy,
			    GDestroyNotify free_func,
			    gpointer end_data)
{
	chain->end_func = end_func;
	chain->end_proxy = proxy;
	chain->end_free_func = free_func;
	chain->end_data = end_data;
}

void msu_chain_task_add(msu_chain_task_t *chain,
			msu_chain_task_action action,
			msu_device_t *device,
			GUPnPServiceProxy *proxy,
			GUPnPServiceProxyActionCallback action_cb,
			GDestroyNotify free_func,
			gpointer cb_user_data)
{
	msu_chain_task_atom_t *atom;

	atom = g_new0(msu_chain_task_atom_t, 1);

	atom->t_action = action;
	atom->callback = action_cb;
	atom->free_func = free_func;
	atom->user_data = cb_user_data;
	atom->device = device;
	atom->proxy = proxy;

	if (proxy != NULL)
		g_object_add_weak_pointer((G_OBJECT(proxy)),
					  (gpointer *)&atom->proxy);

	chain->task_list = g_list_append(chain->task_list, atom);
}

void msu_chain_task_delete(msu_chain_task_t *chain)
{
	g_list_free_full(chain->task_list, (GDestroyNotify)prv_free_atom);
	chain->task_list = NULL;

	if (chain->end_free_func) {
		chain->end_free_func(chain->end_data);
		chain->end_data = NULL;
		chain->end_free_func = NULL;
	}

	g_free(chain);
}

msu_chain_task_t *msu_chain_task_new(void)
{
	return g_new0(msu_chain_task_t, 1);
}
