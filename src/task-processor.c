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
 * Regis Merlino <regis.merlino@intel.com>
 *
 */

#include <stdlib.h>
#include <string.h>

#include "task-processor.h"
#include "log.h"

struct msu_task_processor_t_ {
	GHashTable *task_queues;
	guint running_tasks;
	gboolean quitting;
	GSourceFunc on_quit_cb;
};

typedef struct msu_task_queue_t_ msu_task_queue_t;
struct msu_task_queue_t_ {
	GPtrArray *tasks;
	msu_task_process_cb_t task_process_cb;
	msu_task_cancel_cb_t task_cancel_cb;
	msu_task_delete_cb_t task_delete_cb;
	GCancellable *cancellable;
	msu_task_atom_t *current_task;
	guint idle_id;
	gboolean defer_remove;
	guint32 flags;
};

struct msu_task_queue_key_t_ {
	msu_task_processor_t *processor;
	gchar *source;
	gchar *sink;
};

static guint prv_task_queue_key_hash_cb(gconstpointer ptr)
{
	const msu_task_queue_key_t *queue_key = ptr;
	guint hash;

	hash = g_str_hash(queue_key->source);
	hash ^= g_str_hash(queue_key->sink);

	return hash;
}

static gboolean prv_task_queue_key_equal_cb(gconstpointer ptr1,
					    gconstpointer ptr2)
{
	const msu_task_queue_key_t *queue_key1 = ptr1;
	const msu_task_queue_key_t *queue_key2 = ptr2;

	return !strcmp(queue_key1->source, queue_key2->source) &&
		!strcmp(queue_key1->sink, queue_key2->sink);
}

static void prv_task_queue_key_free_cb(gpointer ptr)
{
	msu_task_queue_key_t *queue_key = ptr;

	g_free(queue_key->source);
	g_free(queue_key->sink);
	g_free(queue_key);
}

static void prv_task_free_cb(gpointer data, gpointer user_data)
{
	msu_task_queue_t *task_queue = user_data;

	task_queue->task_delete_cb(data);
}

static void prv_task_queue_free_cb(gpointer data)
{
	msu_task_queue_t *task_queue = data;

	MSU_LOG_DEBUG("Enter");

	g_ptr_array_foreach(task_queue->tasks, prv_task_free_cb, task_queue);
	g_ptr_array_unref(task_queue->tasks);
	if (task_queue->cancellable)
		g_object_unref(task_queue->cancellable);
	g_free(task_queue);

	MSU_LOG_DEBUG("Exit");
}

msu_task_processor_t *msu_task_processor_new(GSourceFunc on_quit_cb)
{
	msu_task_processor_t *processor;

	MSU_LOG_DEBUG("Enter");

	processor = g_malloc(sizeof(*processor));

	processor->task_queues = g_hash_table_new_full(
						prv_task_queue_key_hash_cb,
						prv_task_queue_key_equal_cb,
						prv_task_queue_key_free_cb,
						prv_task_queue_free_cb);
	processor->running_tasks = 0;
	processor->quitting = FALSE;
	processor->on_quit_cb = on_quit_cb;

	MSU_LOG_DEBUG("Exit");

	return processor;
}

void msu_task_processor_free(msu_task_processor_t *processor)
{
	MSU_LOG_DEBUG("Enter");

	g_hash_table_unref(processor->task_queues);
	g_free(processor);

	MSU_LOG_DEBUG("Exit");
}

const msu_task_queue_key_t *msu_task_processor_add_queue(
					msu_task_processor_t *processor,
					const gchar *source,
					const gchar *sink,
					guint32 flags,
					msu_task_process_cb_t task_process_cb,
					msu_task_cancel_cb_t task_cancel_cb,
					msu_task_delete_cb_t task_delete_cb)
{
	msu_task_queue_t *queue;
	msu_task_queue_key_t *key;

	MSU_LOG_DEBUG("Enter - queue <%s,%s>", source, sink);

	key = g_malloc(sizeof(*key));
	key->processor = processor;
	key->source = g_strdup(source);
	key->sink = g_strdup(sink);

	queue = g_malloc(sizeof(*queue));
	queue->task_process_cb = task_process_cb;
	queue->task_cancel_cb = task_cancel_cb;
	queue->task_delete_cb = task_delete_cb;
	queue->cancellable = NULL;
	queue->current_task = NULL;
	queue->idle_id = 0;
	queue->tasks = g_ptr_array_new();
	queue->flags = flags;
	queue->defer_remove = FALSE;

	g_hash_table_insert(processor->task_queues, key, queue);

	MSU_LOG_DEBUG("Exit");

	return key;
}

static void prv_task_cancel_and_free_cb(gpointer data, gpointer user_data)
{
	msu_task_queue_t *task_queue = user_data;

	task_queue->task_cancel_cb(data);
	task_queue->task_delete_cb(data);
}

static void prv_task_queue_cancel(msu_task_queue_t *task_queue)
{
	if (task_queue->current_task && task_queue->cancellable) {
		g_cancellable_cancel(task_queue->cancellable);
		g_object_unref(task_queue->cancellable);
		task_queue->cancellable = NULL;

		g_ptr_array_remove(task_queue->tasks, task_queue->current_task);

		task_queue->task_cancel_cb(task_queue->current_task);
	}

	if (task_queue->idle_id) {
		(void) g_source_remove(task_queue->idle_id);
		task_queue->idle_id = 0;
	}

	g_ptr_array_foreach(task_queue->tasks, prv_task_cancel_and_free_cb,
			    task_queue);
	g_ptr_array_set_size(task_queue->tasks, 0);
}

static void prv_task_queue_cancel_cb(gpointer key, gpointer value,
				     gpointer user_data)
{
	msu_task_queue_t *task_queue = value;

	prv_task_queue_cancel(task_queue);
}

static void prv_cancel_all_queues(msu_task_processor_t *processor)
{
	MSU_LOG_DEBUG("Enter");

	g_hash_table_foreach(processor->task_queues, prv_task_queue_cancel_cb,
			     NULL);

	MSU_LOG_DEBUG("Exit");
}

void msu_task_processor_set_quitting(msu_task_processor_t *processor)
{
	processor->quitting = TRUE;

	if (processor->running_tasks > 0)
		prv_cancel_all_queues(processor);
	else
		g_idle_add(processor->on_quit_cb, NULL);
}

void msu_task_processor_cancel_queue(const msu_task_queue_key_t *queue_id)
{
	msu_task_queue_t *queue;

	MSU_LOG_DEBUG("Cancel queue <%s,%s>", queue_id->source, queue_id->sink);

	queue = g_hash_table_lookup(queue_id->processor->task_queues,
				    queue_id);
	prv_task_queue_cancel(queue);

	MSU_LOG_DEBUG("Exit");
}

static void prv_free_queue_for_source(gpointer key, gpointer value,
				      gpointer user_data)
{
	msu_task_queue_key_t *queue_key = key;
	msu_task_queue_t *queue = value;
	const gchar *source = user_data;

	if (!strcmp(source, queue_key->source) && !queue->defer_remove) {
		queue->defer_remove = (queue->cancellable != NULL);

		prv_task_queue_cancel(queue);

		if (!queue->defer_remove)
			g_hash_table_remove(queue_key->processor->task_queues,
					    queue_key);
	}
}

void msu_task_processor_remove_queues_for_source(
						msu_task_processor_t *processor,
						const gchar *source)
{
	MSU_LOG_DEBUG("Enter - Source <%s>", source);

	g_hash_table_foreach(processor->task_queues, prv_free_queue_for_source,
			     (gpointer)source);

	MSU_LOG_DEBUG("Exit");
}

static void prv_free_queue_for_sink(gpointer key, gpointer value,
				    gpointer user_data)
{
	msu_task_queue_key_t *queue_key = key;
	msu_task_queue_t *queue = value;
	const gchar *sink = user_data;

	if (!strcmp(sink, queue_key->sink) && !queue->defer_remove) {
		queue->defer_remove = (queue->cancellable != NULL);

		prv_task_queue_cancel(queue);

		if (!queue->defer_remove)
			g_hash_table_remove(queue_key->processor->task_queues,
					    queue_key);
	}
}

void msu_task_processor_remove_queues_for_sink(msu_task_processor_t *processor,
					       const gchar *sink)
{
	MSU_LOG_DEBUG("Enter - Sink <%s>", sink);

	g_hash_table_foreach(processor->task_queues, prv_free_queue_for_sink,
			     (gpointer)sink);

	MSU_LOG_DEBUG("Exit");
}

const msu_task_queue_key_t *msu_task_processor_lookup_queue(
					const msu_task_processor_t *processor,
					const gchar *source,
					const gchar *sink)
{
	msu_task_queue_key_t key;
	msu_task_queue_key_t *orig_key = NULL;
	msu_task_queue_t *queue;

	key.source = (gchar *)source;
	key.sink = (gchar *)sink;

	g_hash_table_lookup_extended(processor->task_queues,
				     &key,
				     (gpointer *)&orig_key,
				     (gpointer *)&queue);

	return orig_key;
}

static gboolean prv_task_queue_process_task(gpointer user_data)
{
	msu_task_queue_key_t *queue_id = user_data;
	msu_task_queue_t *queue;

	MSU_LOG_DEBUG("Enter - Start task processing for queue <%s,%s>",
		      queue_id->source, queue_id->sink);

	queue = g_hash_table_lookup(queue_id->processor->task_queues,
				    queue_id);

	queue->idle_id = 0;
	queue->current_task = g_ptr_array_index(queue->tasks, 0);
	g_ptr_array_remove_index(queue->tasks, 0);
	queue_id->processor->running_tasks++;
	queue->task_process_cb(queue->current_task, &queue->cancellable);

	MSU_LOG_DEBUG("Exit");

	return FALSE;
}

void msu_task_queue_start(const msu_task_queue_key_t *queue_id)
{
	msu_task_queue_t *queue;

	MSU_LOG_DEBUG("Enter - Starting queue <%s,%s>", queue_id->source,
		      queue_id->sink);

	queue = g_hash_table_lookup(queue_id->processor->task_queues,
				    queue_id);

	if (!queue->cancellable && !queue->idle_id)
		queue->idle_id = g_idle_add(prv_task_queue_process_task,
					    (gpointer)queue_id);

	MSU_LOG_DEBUG("Exit");
}

void msu_task_queue_add_task(const msu_task_queue_key_t *queue_id,
			     msu_task_atom_t *task)
{
	msu_task_queue_t *queue;

	MSU_LOG_DEBUG("Enter - Task added to queue <%s,%s>", queue_id->source,
		      queue_id->sink);

	queue = g_hash_table_lookup(queue_id->processor->task_queues,
				    queue_id);

	task->queue_id = queue_id;
	g_ptr_array_add(queue->tasks, task);

	if (queue->flags & MSU_TASK_QUEUE_FLAG_AUTO_START) {
		if (!queue->cancellable && !queue->idle_id)
			queue->idle_id = g_idle_add(prv_task_queue_process_task,
						    (gpointer)queue_id);
	}

	MSU_LOG_DEBUG("Exit");
}

void msu_task_queue_task_completed(const msu_task_queue_key_t *queue_id)
{
	msu_task_queue_t *queue;
	msu_task_processor_t *processor = queue_id->processor;

	MSU_LOG_DEBUG("Enter - Task completed for queue <%s,%s>",
		      queue_id->source, queue_id->sink);

	queue = g_hash_table_lookup(processor->task_queues, queue_id);

	if (queue->cancellable) {
		g_object_unref(queue->cancellable);
		queue->cancellable = NULL;
	}

	if (queue->current_task) {
		queue->task_delete_cb(queue->current_task);
		queue->current_task = NULL;
	}

	processor->running_tasks--;

	if (processor->quitting && !processor->running_tasks)
		g_idle_add(processor->on_quit_cb, NULL);
	else if (queue->defer_remove)
		g_hash_table_remove(processor->task_queues, queue_id);
	else if (queue->tasks->len > 0)
		queue->idle_id = g_idle_add(prv_task_queue_process_task, queue);
	else if (queue->flags & MSU_TASK_QUEUE_FLAG_AUTO_REMOVE)
		g_hash_table_remove(processor->task_queues, queue_id);

	MSU_LOG_DEBUG("Exit");
}
