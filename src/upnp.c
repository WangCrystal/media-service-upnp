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
 * Mark Ryan <mark.d.ryan@intel.com>
 *
 */

#include <string.h>

#include <libgssdp/gssdp-resource-browser.h>
#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-error.h>

#include "async.h"
#include "chain-task.h"
#include "device.h"
#include "error.h"
#include "interface.h"
#include "log.h"
#include "path.h"
#include "search.h"
#include "sort.h"
#include "upnp.h"

struct msu_upnp_t_ {
	GDBusConnection *connection;
	msu_interface_info_t *interface_info;
	GHashTable *filter_map;
	GHashTable *property_map;
	msu_upnp_callback_t found_server;
	msu_upnp_callback_t lost_server;
	GUPnPContextManager *context_manager;
	void *user_data;
	GHashTable *server_udn_map;
	GHashTable *server_uc_map;
	guint counter;
};

/* Private structure used in chain task */
typedef struct prv_device_new_ct_t_ prv_device_new_ct_t;
struct prv_device_new_ct_t_ {
	msu_upnp_t *upnp;
	char *udn;
	msu_device_t *device;
	msu_chain_task_t *chain;
};

static gchar **prv_subtree_enumerate(GDBusConnection *connection,
				     const gchar *sender,
				     const gchar *object_path,
				     gpointer user_data);

static GDBusInterfaceInfo **prv_subtree_introspect(
	GDBusConnection *connection,
	const gchar *sender,
	const gchar *object_path,
	const gchar *node,
	gpointer user_data);

static const GDBusInterfaceVTable *prv_subtree_dispatch(
	GDBusConnection *connection,
	const gchar *sender,
	const gchar *object_path,
	const gchar *interface_name,
	const gchar *node,
	gpointer *out_user_data,
	gpointer user_data);


static const GDBusSubtreeVTable gSubtreeVtable = {
	prv_subtree_enumerate,
	prv_subtree_introspect,
	prv_subtree_dispatch
};

static gchar **prv_subtree_enumerate(GDBusConnection *connection,
				     const gchar *sender,
				     const gchar *object_path,
				     gpointer user_data)
{
	return g_malloc0(sizeof(gchar *));
}

static GDBusInterfaceInfo **prv_subtree_introspect(
	GDBusConnection *connection,
	const gchar *sender,
	const gchar *object_path,
	const gchar *node,
	gpointer user_data)
{
	msu_upnp_t *upnp = user_data;
	GDBusInterfaceInfo **retval = g_new(GDBusInterfaceInfo *,
					    MSU_INTERFACE_INFO_MAX + 1);
	GDBusInterfaceInfo *info;
	unsigned int i;
	gboolean root_object = FALSE;
	const gchar *slash;

	/* All objects in the hierarchy support the same interface.  Strictly
	   speaking this is not correct as it will allow ListChildren to be
	   executed on a mediaitem object.  However, returning the correct
	   interface here would be too inefficient.  We would need to either
	   cache the type of all objects encountered so far or issue a UPnP
	   request here to determine the objects type.  Best to let the client
	   call ListChildren on a item.  This will lead to an error when we
	   execute the UPnP command and we can return an error then.

	   We do know however that the root objects are containers.  Therefore
	   we can remove the MediaItem2 interface from the root containers.  We
	   also know that only the root objects suport the MediaDevice
	   interface.
	*/

	if (msu_path_get_non_root_id(object_path, &slash))
		root_object = !slash;

	i = 0;
	info = upnp->interface_info[MSU_INTERFACE_INFO_PROPERTIES].interface;
	retval[i++] =  g_dbus_interface_info_ref(info);

	info = upnp->interface_info[MSU_INTERFACE_INFO_OBJECT].interface;
	retval[i++] =  g_dbus_interface_info_ref(info);

	info = upnp->interface_info[MSU_INTERFACE_INFO_CONTAINER].interface;
	retval[i++] =  g_dbus_interface_info_ref(info);

	if (!root_object) {
		info = upnp->interface_info[MSU_INTERFACE_INFO_ITEM].interface;
		retval[i++] =  g_dbus_interface_info_ref(info);
	} else {
		info = upnp->interface_info[
			MSU_INTERFACE_INFO_DEVICE].interface;
		retval[i++] =  g_dbus_interface_info_ref(info);
	}

	retval[i] = NULL;

	return retval;
}

static const GDBusInterfaceVTable *prv_subtree_dispatch(
	GDBusConnection *connection,
	const gchar *sender,
	const gchar *object_path,
	const gchar *interface_name,
	const gchar *node,
	gpointer *out_user_data,
	gpointer user_data)
{
	msu_upnp_t *upnp = user_data;
	const GDBusInterfaceVTable *retval = NULL;

	*out_user_data = upnp->user_data;

	if (!strcmp(MSU_INTERFACE_MEDIA_CONTAINER, interface_name))
		retval = upnp->interface_info[
			MSU_INTERFACE_INFO_CONTAINER].vtable;
	else if (!strcmp(MSU_INTERFACE_MEDIA_OBJECT, interface_name))
		retval = upnp->interface_info[
			MSU_INTERFACE_INFO_OBJECT].vtable;
	else if (!strcmp(MSU_INTERFACE_PROPERTIES, interface_name))
		retval = upnp->interface_info[
			MSU_INTERFACE_INFO_PROPERTIES].vtable;
	else if (!strcmp(MSU_INTERFACE_MEDIA_ITEM, interface_name))
		retval = upnp->interface_info[
			MSU_INTERFACE_INFO_ITEM].vtable;
	else if (!strcmp(MSU_INTERFACE_MEDIA_DEVICE, interface_name))
		retval = upnp->interface_info[
			MSU_INTERFACE_INFO_DEVICE].vtable;

	return retval;
}

static void prv_device_chain_end(msu_chain_task_t *chain,
				 GUPnPServiceProxy *proxy,
				 gpointer data)
{
	msu_device_t *device;
	gboolean canceled;
	prv_device_new_ct_t *priv_t = (prv_device_new_ct_t *)data;

	MSU_LOG_DEBUG("Enter");

	device = priv_t->device;
	canceled = msu_chain_task_is_canceled(chain);

	if (canceled)
		goto on_clear;

	MSU_LOG_DEBUG("Notify new server available: %s", device->path);
	g_hash_table_insert(priv_t->upnp->server_udn_map, g_strdup(priv_t->udn),
			    device);
	priv_t->upnp->found_server(device->path, priv_t->upnp->user_data);

on_clear:

	g_hash_table_remove(priv_t->upnp->server_uc_map, priv_t->udn);

	if (canceled)
		msu_device_delete(device);

	MSU_LOG_DEBUG_NL();
}

static void prv_device_new_free(gpointer data)
{
	prv_device_new_ct_t *priv_t = (prv_device_new_ct_t *)data;

	if (priv_t) {
		g_free(priv_t->udn);
		g_free(priv_t);
	}
}

static void prv_server_available_cb(GUPnPControlPoint *cp,
				    GUPnPDeviceProxy *proxy,
				    gpointer user_data)
{
	msu_upnp_t *upnp = user_data;
	const char *udn;
	msu_device_t *device;
	const gchar *ip_address;
	msu_device_context_t *context;
	msu_chain_task_t *chain;
	unsigned int i;
	prv_device_new_ct_t *priv_t;

	udn = gupnp_device_info_get_udn((GUPnPDeviceInfo *)proxy);
	if (!udn)
		goto on_error;

	ip_address = gupnp_context_get_host_ip(
		gupnp_control_point_get_context(cp));

	MSU_LOG_DEBUG("UDN %s", udn);
	MSU_LOG_DEBUG("IP Address %s", ip_address);

	device = g_hash_table_lookup(upnp->server_udn_map, udn);

	if (!device) {
		priv_t = g_hash_table_lookup(upnp->server_uc_map, udn);

		if (priv_t)
			device = priv_t->device;
	}

	if (!device) {
		MSU_LOG_DEBUG("Device not found. Adding");
		MSU_LOG_DEBUG_NL();

		priv_t = g_new0(prv_device_new_ct_t, 1);
		chain = msu_chain_task_new();
		msu_chain_task_set_end(chain, prv_device_chain_end, NULL,
				       prv_device_new_free, priv_t);

		device = msu_device_new(upnp->connection, proxy, ip_address,
					&gSubtreeVtable, upnp,
					upnp->property_map, upnp->counter,
					chain);

		upnp->counter++;

		priv_t->upnp = upnp;
		priv_t->udn = g_strdup(udn);
		priv_t->chain = chain;
		priv_t->device = device;

		g_hash_table_insert(upnp->server_uc_map, g_strdup(udn), priv_t);
	} else {
		MSU_LOG_DEBUG("Device Found");

		for (i = 0; i < device->contexts->len; ++i) {
			context = g_ptr_array_index(device->contexts, i);
			if (!strcmp(context->ip_address, ip_address))
				break;
		}

		if (i == device->contexts->len) {
			MSU_LOG_DEBUG("Adding Context");
			(void) msu_device_append_new_context(device, ip_address,
							     proxy);
		}

		MSU_LOG_DEBUG_NL();
	}

on_error:

	return;
}

static gboolean prv_subscribe_to_contents_change(gpointer user_data)
{
	msu_device_t *device = user_data;

	device->timeout_id = 0;
	msu_device_subscribe_to_contents_change(device);

	return FALSE;
}

static void prv_server_unavailable_cb(GUPnPControlPoint *cp,
				      GUPnPDeviceProxy *proxy,
				      gpointer user_data)
{
	msu_upnp_t *upnp = user_data;
	const char *udn;
	msu_device_t *device;
	const gchar *ip_address;
	unsigned int i;
	msu_device_context_t *context;
	gboolean subscribed;
	gboolean under_construction = FALSE;
	prv_device_new_ct_t *priv_t;

	MSU_LOG_DEBUG("Enter");

	udn = gupnp_device_info_get_udn((GUPnPDeviceInfo *)proxy);
	if (!udn)
		goto on_error;

	ip_address = gupnp_context_get_host_ip(
		gupnp_control_point_get_context(cp));

	MSU_LOG_DEBUG("UDN %s", udn);
	MSU_LOG_DEBUG("IP Address %s", ip_address);

	device = g_hash_table_lookup(upnp->server_udn_map, udn);

	if (!device) {
		priv_t = g_hash_table_lookup(upnp->server_uc_map, udn);

		if (priv_t) {
			device = priv_t->device;
			under_construction = TRUE;
		}
	}

	if (!device) {
		MSU_LOG_WARNING("Device not found. Ignoring");
		goto on_error;
	}

	for (i = 0; i < device->contexts->len; ++i) {
		context = g_ptr_array_index(device->contexts, i);
		if (!strcmp(context->ip_address, ip_address))
			break;
	}

	if (i >= device->contexts->len)
		goto on_error;

	subscribed = context->subscribed;

	(void) g_ptr_array_remove_index(device->contexts, i);

	if (device->contexts->len == 0) {
		if (!under_construction) {
			MSU_LOG_DEBUG("Last Context lost. Delete device");
			upnp->lost_server(device->path, upnp->user_data);
			g_hash_table_remove(upnp->server_udn_map, udn);
		} else {
			MSU_LOG_WARNING(
				"Device under construction. Cancelling");

			msu_chain_task_cancel(priv_t->chain);

			if (!msu_main_is_running()) {
				MSU_LOG_DEBUG("GMain Loop not running");
				prv_device_chain_end(priv_t->chain, NULL,
						     priv_t);
				msu_chain_task_delete(priv_t->chain);
			}
		}
	} else if (subscribed && !device->timeout_id) {
		MSU_LOG_DEBUG("Subscribe on new context");

		device->timeout_id = g_timeout_add_seconds(1,
				prv_subscribe_to_contents_change,
				device);
	}

on_error:

	MSU_LOG_DEBUG("Exit");
	MSU_LOG_DEBUG_NL();

	return;
}

static void prv_on_context_available(GUPnPContextManager *context_manager,
				     GUPnPContext *context,
				     gpointer user_data)
{
	msu_upnp_t *upnp = user_data;
	GUPnPControlPoint *cp;

	cp = gupnp_control_point_new(
		context,
		"urn:schemas-upnp-org:device:MediaServer:1");

	g_signal_connect(cp, "device-proxy-available",
			 G_CALLBACK(prv_server_available_cb), upnp);

	g_signal_connect(cp, "device-proxy-unavailable",
			 G_CALLBACK(prv_server_unavailable_cb), upnp);

	gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(cp), TRUE);
	gupnp_context_manager_manage_control_point(upnp->context_manager, cp);
	g_object_unref(cp);
}

msu_upnp_t *msu_upnp_new(GDBusConnection *connection,
			 msu_interface_info_t *interface_info,
			 msu_upnp_callback_t found_server,
			 msu_upnp_callback_t lost_server,
			 void *user_data)
{
	msu_upnp_t *upnp = g_new0(msu_upnp_t, 1);

	upnp->connection = connection;
	upnp->interface_info = interface_info;
	upnp->user_data = user_data;
	upnp->found_server = found_server;
	upnp->lost_server = lost_server;

	upnp->server_udn_map = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free,
						     msu_device_delete);

	upnp->server_uc_map = g_hash_table_new_full(g_str_hash, g_str_equal,
						     g_free, NULL);

	msu_prop_maps_new(&upnp->property_map, &upnp->filter_map);

	upnp->context_manager = gupnp_context_manager_create(0);

	g_signal_connect(upnp->context_manager, "context-available",
			 G_CALLBACK(prv_on_context_available),
			 upnp);

	return upnp;
}

void msu_upnp_delete(msu_upnp_t *upnp)
{
	if (upnp) {
		g_object_unref(upnp->context_manager);
		g_hash_table_unref(upnp->property_map);
		g_hash_table_unref(upnp->filter_map);
		g_hash_table_unref(upnp->server_udn_map);
		g_hash_table_unref(upnp->server_uc_map);
		g_free(upnp->interface_info);
		g_free(upnp);
	}
}

GVariant *msu_upnp_get_server_ids(msu_upnp_t *upnp)
{
	GVariantBuilder vb;
	GHashTableIter iter;
	gpointer value;
	msu_device_t *device;
	GVariant *retval;

	MSU_LOG_DEBUG("Enter");

	g_variant_builder_init(&vb, G_VARIANT_TYPE("ao"));

	g_hash_table_iter_init(&iter, upnp->server_udn_map);
	while (g_hash_table_iter_next(&iter, NULL, &value)) {
		device = value;
		MSU_LOG_DEBUG("Have device %s", device->path);
		g_variant_builder_add(&vb, "o", device->path);
	}

	retval = g_variant_ref_sink(g_variant_builder_end(&vb));

	MSU_LOG_DEBUG("Exit");

	return retval;
}

GHashTable *msu_upnp_get_server_udn_map(msu_upnp_t *upnp)
{
	return upnp->server_udn_map;
}

void msu_upnp_get_children(msu_upnp_t *upnp, msu_client_t *client,
			   msu_task_t *task,
			   GCancellable *cancellable,
			   msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_async_bas_t *cb_task_data;
	gchar *upnp_filter = NULL;
	gchar *sort_by = NULL;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Path: %s", task->target.path);
	MSU_LOG_DEBUG("Start: %u", task->ut.get_children.start);
	MSU_LOG_DEBUG("Count: %u", task->ut.get_children.count);

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.bas;

	cb_task_data->filter_mask =
		msu_props_parse_filter(upnp->filter_map,
				       task->ut.get_children.filter,
				       &upnp_filter);

	MSU_LOG_DEBUG("Filter Mask 0x%"G_GUINT64_FORMAT"x",
		      cb_task_data->filter_mask);

	sort_by = msu_sort_translate_sort_string(upnp->filter_map,
						 task->ut.get_children.sort_by);
	if (!sort_by) {
		MSU_LOG_WARNING("Invalid Sort Criteria");

		cb_data->error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_QUERY,
					     "Sort Criteria are not valid");
		goto on_error;
	}

	MSU_LOG_DEBUG("Sort By %s", sort_by);

	cb_task_data->protocol_info = client->protocol_info;

	msu_device_get_children(client, task, cb_data,
				upnp_filter, sort_by, cancellable);

on_error:

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	g_free(sort_by);
	g_free(upnp_filter);

	MSU_LOG_DEBUG("Exit with %s", !cb_data->action ? "FAIL" : "SUCCESS");
}

void msu_upnp_get_all_props(msu_upnp_t *upnp, msu_client_t *client,
			    msu_task_t *task,
			    GCancellable *cancellable,
			    msu_upnp_task_complete_t cb)
{
	gboolean root_object;
	msu_async_cb_data_t *cb_data;
	msu_async_get_all_t *cb_task_data;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Path: %s", task->target.path);
	MSU_LOG_DEBUG("Interface %s", task->ut.get_prop.interface_name);

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.get_all;

	root_object = task->target.id[0] == '0' && task->target.id[1] == 0;

	MSU_LOG_DEBUG("Root Object = %d", root_object);

	cb_task_data->protocol_info = client->protocol_info;

	msu_device_get_all_props(client, task, cb_data, root_object,
				 cancellable);

	MSU_LOG_DEBUG("Exit with SUCCESS");
}

void msu_upnp_get_prop(msu_upnp_t *upnp, msu_client_t *client,
		       msu_task_t *task,
		       GCancellable *cancellable,
		       msu_upnp_task_complete_t cb)
{
	gboolean root_object;
	msu_async_cb_data_t *cb_data;
	msu_async_get_prop_t *cb_task_data;
	msu_prop_map_t *prop_map;
	msu_task_get_prop_t *task_data;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Path: %s", task->target.path);
	MSU_LOG_DEBUG("Interface %s", task->ut.get_prop.interface_name);
	MSU_LOG_DEBUG("Prop.%s", task->ut.get_prop.prop_name);

	task_data = &task->ut.get_prop;
	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.get_prop;

	root_object = task->target.id[0] == '0' && task->target.id[1] == 0;

	MSU_LOG_DEBUG("Root Object = %d", root_object);

	cb_task_data->protocol_info = client->protocol_info;
	prop_map = g_hash_table_lookup(upnp->filter_map, task_data->prop_name);

	msu_device_get_prop(client, task, cb_data, prop_map,
			    root_object, cancellable);

	MSU_LOG_DEBUG("Exit with SUCCESS");
}

void msu_upnp_search(msu_upnp_t *upnp, msu_client_t *client,
		     msu_task_t *task,
		     GCancellable *cancellable,
		     msu_upnp_task_complete_t cb)
{
	gchar *upnp_filter = NULL;
	gchar *upnp_query = NULL;
	gchar *sort_by = NULL;
	msu_async_cb_data_t *cb_data;
	msu_async_bas_t *cb_task_data;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Path: %s", task->target.path);
	MSU_LOG_DEBUG("Query: %s", task->ut.search.query);
	MSU_LOG_DEBUG("Start: %u", task->ut.search.start);
	MSU_LOG_DEBUG("Count: %u", task->ut.search.count);

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.bas;

	cb_task_data->filter_mask =
		msu_props_parse_filter(upnp->filter_map,
				       task->ut.search.filter, &upnp_filter);

	MSU_LOG_DEBUG("Filter Mask 0x%"G_GUINT64_FORMAT"x",
		      cb_task_data->filter_mask);

	upnp_query = msu_search_translate_search_string(upnp->filter_map,
							task->ut.search.query);
	if (!upnp_query) {
		MSU_LOG_WARNING("Query string is not valid:%s",
				task->ut.search.query);

		cb_data->error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_QUERY,
					     "Query string is not valid.");
		goto on_error;
	}

	MSU_LOG_DEBUG("UPnP Query %s", upnp_query);

	sort_by = msu_sort_translate_sort_string(upnp->filter_map,
						 task->ut.search.sort_by);
	if (!sort_by) {
		MSU_LOG_WARNING("Invalid Sort Criteria");

		cb_data->error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_QUERY,
					     "Sort Criteria are not valid");
		goto on_error;
	}

	MSU_LOG_DEBUG("Sort By %s", sort_by);

	cb_task_data->protocol_info = client->protocol_info;

	msu_device_search(client, task, cb_data, upnp_filter,
			  upnp_query, sort_by, cancellable);
on_error:

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	g_free(sort_by);
	g_free(upnp_query);
	g_free(upnp_filter);

	MSU_LOG_DEBUG("Exit with %s", !cb_data->action ? "FAIL" : "SUCCESS");
}

void msu_upnp_get_resource(msu_upnp_t *upnp, msu_client_t *client,
			   msu_task_t *task,
			   GCancellable *cancellable,
			   msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_async_get_all_t *cb_task_data;
	gchar *upnp_filter = NULL;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Protocol Info: %s ", task->ut.resource.protocol_info);

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.get_all;

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	cb_task_data->filter_mask =
		msu_props_parse_filter(upnp->filter_map,
				       task->ut.resource.filter, &upnp_filter);

	MSU_LOG_DEBUG("Filter Mask 0x%"G_GUINT64_FORMAT"x",
		      cb_task_data->filter_mask);

	msu_device_get_resource(client, task, cb_data, upnp_filter,
				cancellable);

	MSU_LOG_DEBUG("Exit");
}

static gboolean prv_compute_mime_and_class(msu_task_t *task,
					   msu_async_upload_t *cb_task_data,
					   GError **error)
{
	gchar *content_type = NULL;

	if (!g_file_test(task->ut.upload.file_path,
			 G_FILE_TEST_IS_REGULAR | G_FILE_TEST_EXISTS)) {

		MSU_LOG_WARNING(
			"File %s does not exist or is not a regular file",
			task->ut.upload.file_path);

		*error = g_error_new(MSU_ERROR, MSU_ERROR_OBJECT_NOT_FOUND,
				     "File %s does not exist or is not a regular file",
				     task->ut.upload.file_path);
		goto on_error;
	}

	content_type = g_content_type_guess(task->ut.upload.file_path, NULL, 0,
					    NULL);

	if (!content_type) {

		MSU_LOG_WARNING("Unable to determine Content Type for %s",
				task->ut.upload.file_path);

		*error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_MIME,
				     "Unable to determine Content Type for %s",
				     task->ut.upload.file_path);
		goto on_error;
	}

	cb_task_data->mime_type = g_content_type_get_mime_type(content_type);
	g_free(content_type);

	if (!cb_task_data->mime_type) {

		MSU_LOG_WARNING("Unable to determine MIME Type for %s",
				task->ut.upload.file_path);

		*error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_MIME,
				     "Unable to determine MIME Type for %s",
				     task->ut.upload.file_path);
		goto on_error;
	}

	if (g_content_type_is_a(cb_task_data->mime_type, "image/*")) {
		cb_task_data->object_class = "object.item.imageItem";
	} else if (g_content_type_is_a(cb_task_data->mime_type, "audio/*")) {
		cb_task_data->object_class = "object.item.audioItem";
	} else if (g_content_type_is_a(cb_task_data->mime_type, "video/*")) {
		cb_task_data->object_class = "object.item.videoItem";
	} else {

		MSU_LOG_WARNING("Unsupported MIME Type %s",
				cb_task_data->mime_type);

		*error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_MIME,
				     "Unsupported MIME Type %s",
				     cb_task_data->mime_type);
		goto on_error;
	}

	return TRUE;

on_error:

	return FALSE;
}

void msu_upnp_upload_to_any(msu_upnp_t *upnp, msu_client_t *client,
			    msu_task_t *task,
			    GCancellable *cancellable,
			    msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_async_upload_t *cb_task_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.upload;

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		cb_data->error =
			g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
				    "UploadToAnyContainer must be executed on a root path");
		goto on_error;
	}

	if (!prv_compute_mime_and_class(task, cb_task_data, &cb_data->error))
		goto on_error;

	MSU_LOG_DEBUG("MIME Type %s", cb_task_data->mime_type);
	MSU_LOG_DEBUG("Object class %s", cb_task_data->object_class);

	msu_device_upload(client, task, "DLNA.ORG_AnyContainer",
			  cb_data, cancellable);

on_error:

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_upload(msu_upnp_t *upnp, msu_client_t *client, msu_task_t *task,
		     GCancellable *cancellable,
		     msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_async_upload_t *cb_task_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.upload;

	if (!prv_compute_mime_and_class(task, cb_task_data, &cb_data->error))
		goto on_error;

	MSU_LOG_DEBUG("MIME Type %s", cb_task_data->mime_type);
	MSU_LOG_DEBUG("Object class %s", cb_task_data->object_class);

	msu_device_upload(client, task, task->target.id, cb_data,
			  cancellable);

on_error:

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_get_upload_status(msu_upnp_t *upnp, msu_task_t *task)
{
	GError *error = NULL;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
				    "GetUploadStatus must be executed on a root path");
		goto on_error;
	}

	(void) msu_device_get_upload_status(task, &error);

on_error:

	if (error) {
		msu_task_fail(task, error);
		g_error_free(error);
	} else {
		msu_task_complete(task);
	}

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_get_upload_ids(msu_upnp_t *upnp, msu_task_t *task)
{
	GError *error = NULL;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
				    "GetUploadIDs must be executed on a root path");
		goto on_error;
	}

	 msu_device_get_upload_ids(task);

on_error:

	if (error) {
		msu_task_fail(task, error);
		g_error_free(error);
	} else {
		msu_task_complete(task);
	}

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_cancel_upload(msu_upnp_t *upnp, msu_task_t *task)
{
	GError *error = NULL;

	MSU_LOG_DEBUG("Enter");

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
				    "CancelUpload must be executed on a root path");
		goto on_error;
	}

	(void) msu_device_cancel_upload(task, &error);

on_error:

	if (error) {
		msu_task_fail(task, error);
		g_error_free(error);
	} else {
		msu_task_complete(task);
	}

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_delete_object(msu_upnp_t *upnp, msu_client_t *client,
			    msu_task_t *task,
			    GCancellable *cancellable,
			    msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	msu_device_delete_object(client, task, cb_data, cancellable);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_create_container(msu_upnp_t *upnp, msu_client_t *client,
			       msu_task_t *task,
			       GCancellable *cancellable,
			       msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	msu_device_create_container(client, task, task->target.id,
				    cb_data, cancellable);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_create_container_in_any(msu_upnp_t *upnp, msu_client_t *client,
				      msu_task_t *task,
				      GCancellable *cancellable,
				      msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		cb_data->error =
			g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
				    "CreateContainerInAnyContainer must be executed on a root path");
		goto on_error;
	}

	msu_device_create_container(client, task,
				    "DLNA.ORG_AnyContainer",
				    cb_data, cancellable);

on_error:

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_update_object(msu_upnp_t *upnp, msu_client_t *client,
			    msu_task_t *task,
			    GCancellable *cancellable,
			    msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_async_update_t *cb_task_data;
	msu_upnp_prop_mask mask;
	gchar *upnp_filter = NULL;
	msu_task_update_t *task_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);
	cb_task_data = &cb_data->ut.update;
	task_data = &task->ut.update;

	MSU_LOG_DEBUG("Root Path %s Id %s", task->target.root_path,
		      task->target.id);

	if (!msu_props_parse_update_filter(upnp->filter_map,
					   task_data->to_add_update,
					   task_data->to_delete,
					   &mask, &upnp_filter)) {
		MSU_LOG_WARNING("Invalid Parameter");

		cb_data->error = g_error_new(MSU_ERROR,
					     MSU_ERROR_OPERATION_FAILED,
					     "Invalid Parameter");
		goto on_error;
	}

	cb_task_data->map = upnp->filter_map;

	MSU_LOG_DEBUG("Filter = %s", upnp_filter);
	MSU_LOG_DEBUG("Mask = 0x%"G_GUINT64_FORMAT"x", mask);

	if (mask == 0) {
		MSU_LOG_WARNING("Empty Parameters");

		cb_data->error = g_error_new(MSU_ERROR,
					     MSU_ERROR_OPERATION_FAILED,
					     "Empty Parameters");

		goto on_error;
	}

	msu_device_update_object(client, task, cb_data, upnp_filter,
				 cancellable);

on_error:

	g_free(upnp_filter);

	if (!cb_data->action)
		(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit");
}

void msu_upnp_create_playlist(msu_upnp_t *upnp, msu_client_t *client,
			      msu_task_t *task,
			      GCancellable *cancellable,
			      msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_task_create_playlist_t *task_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);
	task_data = &task->ut.playlist;

	MSU_LOG_DEBUG("Root Path: %s - Id: %s", task->target.root_path,
		      task->target.id);

	if (!task_data->title || !*task_data->title)
		goto on_param_error;

	if (!g_variant_n_children(task_data->item_path))
		goto on_param_error;

	MSU_LOG_DEBUG_NL();
	MSU_LOG_DEBUG("Title = %s", task_data->title);
	MSU_LOG_DEBUG("Creator = %s", task_data->creator);
	MSU_LOG_DEBUG("Genre = %s", task_data->genre);
	MSU_LOG_DEBUG("Desc = %s", task_data->desc);
	MSU_LOG_DEBUG_NL();

	msu_device_playlist_upload(client, task, task->target.id, cb_data,
				   cancellable);

	MSU_LOG_DEBUG("Exit");

	return;

on_param_error:

	MSU_LOG_WARNING("Invalid Parameter");

	cb_data->error = g_error_new(MSU_ERROR,
				     MSU_ERROR_OPERATION_FAILED,
				     "Invalid Parameter");

	(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit failure");
}

void msu_upnp_create_playlist_in_any(msu_upnp_t *upnp, msu_client_t *client,
				     msu_task_t *task,
				     GCancellable *cancellable,
				     msu_upnp_task_complete_t cb)
{
	msu_async_cb_data_t *cb_data;
	msu_task_create_playlist_t *task_data;

	MSU_LOG_DEBUG("Enter");

	cb_data = msu_async_cb_data_new(task, cb);
	task_data = &task->ut.playlist;

	MSU_LOG_DEBUG("Root Path: %s - Id: %s", task->target.root_path,
		      task->target.id);

	if (strcmp(task->target.id, "0")) {
		MSU_LOG_WARNING("Bad path %s", task->target.path);

		cb_data->error = g_error_new(MSU_ERROR, MSU_ERROR_BAD_PATH,
					     "CreatePlayListInAny must be executed on a root path");

		goto on_error;
	}

	if (!task_data->title || !*task_data->title)
		goto on_param_error;

	if (!g_variant_n_children(task_data->item_path))
		goto on_param_error;

	MSU_LOG_DEBUG_NL();
	MSU_LOG_DEBUG("Title = %s", task_data->title);
	MSU_LOG_DEBUG("Creator = %s", task_data->creator);
	MSU_LOG_DEBUG("Genre = %s", task_data->genre);
	MSU_LOG_DEBUG("Desc = %s", task_data->desc);
	MSU_LOG_DEBUG_NL();

	msu_device_playlist_upload(client, task, "DLNA.ORG_AnyContainer",
				   cb_data, cancellable);

	MSU_LOG_DEBUG("Exit");

	return;

on_param_error:

	MSU_LOG_WARNING("Invalid Parameter");

	cb_data->error = g_error_new(MSU_ERROR,
				     MSU_ERROR_OPERATION_FAILED,
				     "Invalid Parameter");
on_error:

	(void) g_idle_add(msu_async_complete_task, cb_data);

	MSU_LOG_DEBUG("Exit failure");
}

static gboolean prv_device_uc_find(gpointer key, gpointer value,
				   gpointer user_data)
{
	prv_device_new_ct_t *priv_t = (prv_device_new_ct_t *)value;

	return (priv_t->device == user_data) ? TRUE : FALSE;
}

static gboolean prv_device_find(gpointer key, gpointer value,
				gpointer user_data)
{
	return (value == user_data) ? TRUE : FALSE;
}

gboolean msu_upnp_device_context_exist(msu_device_t *device,
				       msu_device_context_t *context)
{
	gpointer result;
	guint i;
	gboolean found = FALSE;
	msu_upnp_t *upnp = msu_media_service_get_upnp();

	if (upnp == NULL)
		goto on_exit;

	/* Check if the device still exist */
	result = g_hash_table_find(upnp->server_udn_map, prv_device_find,
				   device);

	if (result == NULL)
		if (g_hash_table_find(upnp->server_uc_map, prv_device_uc_find,
				      device) == NULL)
			goto on_exit;

	/* Search if the context still exist in the device */
	for (i = 0; i < device->contexts->len; ++i) {
		if (g_ptr_array_index(device->contexts, i) == context) {
			found = TRUE;
			break;
		}
	}

on_exit:
	return found;
}
