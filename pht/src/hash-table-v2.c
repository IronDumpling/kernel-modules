#include "hash-table-base.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>

struct list_entry {
	const char *key;
	uint32_t value;
	SLIST_ENTRY(list_entry) pointers;
};

SLIST_HEAD(list_head, list_entry);

struct hash_table_entry {
	struct list_head list_head;
	pthread_mutex_t * write_mtx_ptr;
	pthread_mutex_t write_mutex;
};

struct hash_table_v2 {
	struct hash_table_entry entries[HASH_TABLE_CAPACITY];
};

struct hash_table_v2 *hash_table_v2_create()
{
	struct hash_table_v2 *hash_table = calloc(1, sizeof(struct hash_table_v2));
	assert(hash_table != NULL);
	for (size_t i = 0; i < HASH_TABLE_CAPACITY; ++i) {
		struct hash_table_entry *entry = &hash_table->entries[i];
		SLIST_INIT(&entry->list_head);
		// TODO: initialise mutexes
		entry->write_mtx_ptr = &(entry->write_mutex);
		pthread_mutex_init(entry->write_mtx_ptr, NULL);
	}
	return hash_table;
}

void set_start(struct hash_table_entry *entry)
{
    pthread_mutex_lock(entry->write_mtx_ptr);
}

void set_end(struct hash_table_entry *entry)
{
    pthread_mutex_unlock(entry->write_mtx_ptr);
}

static struct hash_table_entry *get_hash_table_entry(struct hash_table_v2 *hash_table,
                                                     const char *key)
{
    assert(key != NULL);
	uint32_t index = bernstein_hash(key) % HASH_TABLE_CAPACITY;
	struct hash_table_entry *entry = &hash_table->entries[index];
	return entry;
}

static struct list_entry *get_list_entry(struct list_head *list_head,
                                         const char *key)
{
    assert(key != NULL);
	struct list_entry *entry = NULL;
	
	SLIST_FOREACH(entry, list_head, pointers) {
	    if (strcmp(entry->key, key) == 0) {
	        return entry;
	    }
	}

	return NULL;
}

bool hash_table_v2_contains(struct hash_table_v2 *hash_table,
                            const char *key)
{
	struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key);
	struct list_head *list_head = &hash_table_entry->list_head;
	struct list_entry *list_entry = get_list_entry(list_head, key);
	return list_entry != NULL;
}

void hash_table_v2_add_entry(struct hash_table_v2 *hash_table,
                             const char *key,
                             uint32_t value)
{
    struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key);

    set_start(hash_table_entry);

    struct list_head *list_head = &hash_table_entry->list_head;
    struct list_entry *list_entry = get_list_entry(list_head, key);

	/* Update the value if it already exists */
	if (list_entry != NULL) {
		list_entry->value = value;
		return;
	}

	list_entry = calloc(1, sizeof(struct list_entry));
	list_entry->key = key;
	list_entry->value = value;

	SLIST_INSERT_HEAD(list_head, list_entry, pointers);

	set_end(hash_table_entry);
}

uint32_t hash_table_v2_get_value(struct hash_table_v2 *hash_table,
                                 const char *key)
{
	struct hash_table_entry *hash_table_entry = get_hash_table_entry(hash_table, key);
	struct list_head *list_head = &hash_table_entry->list_head;
	struct list_entry *list_entry = get_list_entry(list_head, key);
	assert(list_entry != NULL);
	return list_entry->value;
}

void hash_table_v2_destroy(struct hash_table_v2 *hash_table)
{
	for (size_t i = 0; i < HASH_TABLE_CAPACITY; ++i) {
		struct hash_table_entry *entry = &hash_table->entries[i];

		free(entry->write_mtx_ptr);

		struct list_head *list_head = &entry->list_head;
		struct list_entry *list_entry = NULL;
		while (!SLIST_EMPTY(list_head)) {
			list_entry = SLIST_FIRST(list_head);
			SLIST_REMOVE_HEAD(list_head, pointers);
			free(list_entry);
		}
	}
	free(hash_table);
}
