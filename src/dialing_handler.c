/*
 * dialing_handler.c
 *
 *  Created on: Nov 21, 2015
 *      Author: pchero
 */

#include "asterisk.h"
#include "asterisk/utils.h"
#include "asterisk/astobj2.h"

#include <stdbool.h>

#include "dialing_handler.h"

static int rb_dialing_cmp_cb(void* obj, void* arg, int flags);
static int rb_dialing_sort_cb(const void* o_left, const void* o_right, int flags);
static void rb_dialing_destructor(void* obj);

static struct ao2_container* g_rb_dialings = NULL;  ///< dialing container

/**
 * Initiate rb_diailing.
 * @return
 */
int init_rb_dialing(void)
{
    g_rb_dialings = ao2_container_alloc_rbtree(AO2_ALLOC_OPT_LOCK_MUTEX, AO2_CONTAINER_ALLOC_OPT_DUPS_REJECT, rb_dialing_sort_cb, rb_dialing_cmp_cb);
    if(g_rb_dialings == NULL) {
        ast_log(LOG_ERROR, "Could not create rbtree.\n");
        return false;
    }
   ast_log(LOG_NOTICE, "Initiated dialing handler.\n");

    return true;
}

/**
 *
 * @param o_left
 * @param o_right
 * @param flags
 * @return
 */
static int rb_dialing_sort_cb(const void* o_left, const void* o_right, int flags)
{
    const rb_dialing* dialing_left;
    const char* key;

    dialing_left = (rb_dialing*)o_left;

    if(flags & OBJ_SEARCH_KEY) {
        key = (char*)o_right;

        return strcmp(dialing_left->uuid, key);
    }
    else if(flags & OBJ_SEARCH_PARTIAL_KEY) {
        key = (char*)o_right;

        return strcmp(ast_json_string_get(ast_json_object_get(dialing_left->j_dl, "uuid_dl")), key);
    }
    else {
        const rb_dialing* dialing_right;

        dialing_right = (rb_dialing*)o_right;
        return strcmp(dialing_left->uuid, dialing_right->uuid);
    }
}

/**
 *
 * @param obj
 * @param arg
 * @param flags
 * @return
 */
static int rb_dialing_cmp_cb(void* obj, void* arg, int flags)
{
    rb_dialing* dialing;
    const char *uuid;

    dialing = (rb_dialing*)obj;

//    ast_log(LOG_DEBUG, "Find. uuid[%s], flag[%d]s\n", uuid, flags);
    if(flags & OBJ_SEARCH_KEY) {
        uuid = (const char*)arg;

        // channel id
        if(dialing->uuid == NULL) {
            return 0;
        }
        if(strcmp(dialing->uuid, uuid) == 0) {
            return CMP_MATCH;
        }
        return 0;
    }
    else if(flags & OBJ_SEARCH_PARTIAL_KEY) {
        uuid = (const char*)arg;

        // uuid_dl
        if(strcmp(ast_json_string_get(ast_json_object_get(dialing->j_dl, "uuid_dl")), uuid) == 0) {
            return CMP_MATCH;
        }
        return 0;
    }
    else {
        // channel id
        rb_dialing* dialing_right;

        dialing_right = (rb_dialing*)arg;
        if(strcmp(dialing->uuid, dialing_right->uuid) == 0) {
            return CMP_MATCH;
        }
        return 0;
    }
}

/**
 * Create dialing obj.
 * @param j_camp
 * @param j_plan
 * @param j_dl
 * @return
 */
rb_dialing* rb_dialing_create(struct ast_json* j_camp, struct ast_json* j_plan, struct ast_json* j_dlma, struct ast_json* j_dl)
{
    rb_dialing* dialing;
    const char* tmp_const;

    if((j_camp == NULL) || (j_plan == NULL) || (j_dl == NULL)) {
        return NULL;
    }

    tmp_const = ast_json_string_get(ast_json_object_get(j_dl, "channelid"));
    if(tmp_const == NULL) {
        return NULL;
    }

    dialing = ao2_alloc(sizeof(rb_dialing), rb_dialing_destructor);

    dialing->uuid = ast_strdup(tmp_const);
    dialing->j_camp = ast_json_deep_copy(j_camp);
    dialing->j_plan = ast_json_deep_copy(j_plan);
    dialing->j_dlma = ast_json_deep_copy(j_dlma);
    dialing->j_dl = ast_json_deep_copy(j_dl);

    dialing->j_chan = ast_json_object_create();
    dialing->j_queues = ast_json_array_create();
    dialing->j_agents = ast_json_array_create();

    if(ao2_link(g_rb_dialings, dialing) == 0) {
        ast_log(LOG_DEBUG, "Could not register the dialing. uuid[%s]\n", dialing->uuid);
        rb_dialing_destructor(dialing);
        return NULL;
    }
    return dialing;
}

void rb_dialing_destory(rb_dialing* dialing)
{
    ast_log(LOG_DEBUG, "Destroying dialing.\n");
    ao2_unlink(g_rb_dialings, dialing);
    ao2_ref(dialing, -1);
}

static void rb_dialing_destructor(void* obj)
{
    rb_dialing* dialing;

    dialing = (rb_dialing*)obj;

    if(dialing->uuid != NULL)       ast_free(dialing->uuid);

    if(dialing->j_camp != NULL)     ast_json_unref(dialing->j_camp);
    if(dialing->j_plan != NULL)     ast_json_unref(dialing->j_plan);
    if(dialing->j_dlma != NULL)     ast_json_unref(dialing->j_dlma);
    if(dialing->j_dl != NULL)       ast_json_unref(dialing->j_dl);
    if(dialing->j_chan != NULL)     ast_json_unref(dialing->j_chan);
    if(dialing->j_queues != NULL)   ast_json_unref(dialing->j_queues);
    if(dialing->j_agents != NULL)   ast_json_unref(dialing->j_agents);

    ast_log(LOG_DEBUG, "Called destroyer.\n");
}

rb_dialing* rb_dialing_find_uuid_dl(const char* chan)
{
    rb_dialing* dialing;
    dialing = ao2_find(g_rb_dialings, chan, OBJ_SEARCH_PARTIAL_KEY);
    if(dialing == NULL) {
        return NULL;
    }
    ao2_ref(dialing, -1);

    return dialing;
}

rb_dialing* rb_dialing_find_uuid_chan(const char* uuid)
{
    rb_dialing* dialing;

    if(uuid == NULL) {
        return NULL;
    }

    ast_log(LOG_DEBUG, "rb_dialing_find_uuid. uuid[%s]\n", uuid);
    dialing = ao2_find(g_rb_dialings, uuid, OBJ_SEARCH_KEY);
    if(dialing == NULL) {
        return NULL;
    }
    ao2_ref(dialing, -1);

    return dialing;
}

struct ao2_iterator rb_dialing_iter_init(void)
{
//    ast_log(LOG_DEBUG, "rd_dialing count. count[%d]\n", ao2_container_count(g_rb_dialings));
    return ao2_iterator_init(g_rb_dialings, 0);
}

struct ast_json* rb_dialing_get_all_for_cli(void)
{
    struct ao2_iterator iter;
    rb_dialing* dialing;
    struct ast_json* j_res;
    struct ast_json* j_tmp;

    j_res = ast_json_array_create();
    iter = rb_dialing_iter_init();
    while((dialing = ao2_iterator_next(&iter))) {
        ao2_ref(dialing, -1);

        j_tmp = ast_json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
                "uuid",         dialing->uuid,
                "channelstate", ast_json_string_get(ast_json_object_get(dialing->j_chan, "channelstate")) ? : "",
                "channel",      ast_json_string_get(ast_json_object_get(dialing->j_chan, "channel")) ? : "",
                "queue",        ast_json_string_get(ast_json_object_get(dialing->j_chan, "queue")) ? : "",
                "membername",   ast_json_string_get(ast_json_object_get(dialing->j_chan, "membername")) ? : "",
                "tm_hangup",    ast_json_string_get(ast_json_object_get(dialing->j_chan, "tm_hangup")) ? : ""
                );

        ast_json_array_append(j_res, j_tmp);
    }
    ao2_iterator_destroy(&iter);

    return j_res;
}
