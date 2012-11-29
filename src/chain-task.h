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

#ifndef MSU_CHAIN_TASK_H__
#define MSU_CHAIN_TASK_H__

#include <libgupnp/gupnp-service-proxy.h>

#include "media-service-upnp.h"

typedef struct msu_chain_task_t_ msu_chain_task_t;

typedef GUPnPServiceProxyAction * (*msu_chain_task_action)
				(msu_chain_task_t *chain, gboolean *failed);

typedef void (*msu_chain_task_end)(msu_chain_task_t *chain, gpointer data);

msu_chain_task_t *msu_chain_task_new(msu_chain_task_end end_func,
				     gpointer end_data);

void msu_chain_task_delete(msu_chain_task_t *chain);

void msu_chain_task_add(msu_chain_task_t *chain,
			msu_chain_task_action action,
			msu_device_t *device,
			GUPnPServiceProxyActionCallback action_cb,
			GDestroyNotify free_func,
			gpointer cb_user_data);

void msu_chain_task_start(msu_chain_task_t *chain);

void msu_chain_task_begin_action_cb(GUPnPServiceProxy *proxy,
				    GUPnPServiceProxyAction *action,
				    gpointer user_data);

void msu_chain_task_cancel(msu_chain_task_t *chain);
gboolean msu_chain_task_is_canceled(msu_chain_task_t *chain);

msu_device_t *msu_chain_task_get_device(msu_chain_task_t *chain);
gpointer *msu_chain_task_get_user_data(msu_chain_task_t *chain);

#endif /* MSU_CHAIN_TASK_H__ */
