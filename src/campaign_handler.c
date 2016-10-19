/*
 * campaign_handler.c
 *
 *  Created on: Nov 26, 2015
 *	  Author: pchero
 */

#include "asterisk.h"
#include "asterisk/json.h"
#include "asterisk/logger.h"
#include "asterisk/utils.h"
#include "asterisk/uuid.h"

#include <stdbool.h>

#include "db_handler.h"
#include "campaign_handler.h"
#include "cli_handler.h"
#include "event_handler.h"
#include "utils.h"

/**
 * Create campaign.
 * @param j_camp
 * @return
 */
bool create_campaign(const struct ast_json* j_camp)
{
	int ret;
	char* uuid;
	char* tmp;
	struct ast_json* j_tmp;

	if(j_camp == NULL) {
		return false;
	}
	j_tmp = ast_json_deep_copy(j_camp);

	uuid = gen_uuid();
	ast_json_object_set(j_tmp, "uuid", ast_json_string_create(uuid));

	tmp = get_utc_timestamp();
	ast_json_object_set(j_tmp, "tm_create", ast_json_string_create(tmp));
	ast_free(tmp);

	ast_log(LOG_NOTICE, "Create campaign. uuid[%s], name[%s]\n",
			ast_json_string_get(ast_json_object_get(j_tmp, "uuid")),
			ast_json_string_get(ast_json_object_get(j_tmp, "name"))
			);
	ret = db_insert("campaign", j_tmp);
	ast_json_unref(j_tmp);
	if(ret == false) {
		ast_free(uuid);
		return false;
	}

	// send ami event
	j_tmp = get_campaign(uuid);
	ast_free(uuid);
	send_manager_evt_out_campaign_create(j_tmp);
	ast_json_unref(j_tmp);

	return true;
}

/**
 * Delete campaign.
 * @param uuid
 * @return
 */
bool delete_campaign(const char* uuid)
{
	struct ast_json* j_camp;
	char* tmp;
	char* sql;
	int ret;

	if(uuid == NULL) {
		// invalid parameter.
		return false;
	}

	j_camp = ast_json_object_create();
	tmp = get_utc_timestamp();
	ast_json_object_set(j_camp, "status", ast_json_integer_create(E_CAMP_STOP));
	ast_json_object_set(j_camp, "tm_delete", ast_json_string_create(tmp));
	ast_json_object_set(j_camp, "in_use", ast_json_integer_create(0));
	ast_free(tmp);

	tmp = db_get_update_str(j_camp);
	ast_json_unref(j_camp);
	ast_asprintf(&sql, "update campaign set %s where uuid=\"%s\";", tmp, uuid);
	ast_free(tmp);

	ret = db_exec(sql);
	ast_free(sql);
	if(ret == false) {
		ast_log(LOG_WARNING, "Could not delete campaign. uuid[%s]\n", uuid);
		return false;
	}

	// send notification
	send_manager_evt_out_campaign_delete(uuid);

	return true;
}

/**
 * Get specified campaign
 * @return
 */
struct ast_json* get_campaign(const char* uuid)
{
	struct ast_json* j_res;
	db_res_t* db_res;
	char* sql;

	if(uuid == NULL) {
		return NULL;
	}
	ast_log(LOG_DEBUG, "Get campaign info. uuid[%s]\n", uuid);

	// get specified campaign
	ast_asprintf(&sql, "select * from campaign where uuid=\"%s\" and in_use=1;", uuid);

	db_res = db_query(sql);
	ast_free(sql);
	if(db_res == NULL) {
		ast_log(LOG_WARNING, "Could not get campaign info.\n");
		return NULL;
	}

	j_res = db_get_record(db_res);
	db_free(db_res);

	return j_res;
}

/**
 * Get all campaigns
 * @return
 */
struct ast_json* get_campaigns_all(void)
{
	struct ast_json* j_res;
	struct ast_json* j_tmp;
	db_res_t* db_res;
	char* sql;

	// get all campaigns
	ast_asprintf(&sql, "%s", "select * from campaign where in_use=1");

	db_res = db_query(sql);
	ast_free(sql);
	if(db_res == NULL) {
		ast_log(LOG_WARNING, "Could not get campaign info.\n");
		return NULL;
	}

	j_res = ast_json_array_create();
	while(1) {
		j_tmp = db_get_record(db_res);
		if(j_tmp == NULL) {
			break;
		}

		ast_json_array_append(j_res, j_tmp);
	}
	db_free(db_res);

	return j_res;
}

/**
 * Update campaign
 * @param j_camp
 * @return
 */
bool update_campaign(const struct ast_json* j_camp)
{
	char* tmp;
	const char* tmp_const;
	char* sql;
	struct ast_json* j_tmp;
	char* uuid;

	if(j_camp == NULL) {
		return false;
	}

	j_tmp = ast_json_deep_copy(j_camp);
	if(j_tmp == NULL) {
		return false;
	}

	tmp_const = ast_json_string_get(ast_json_object_get(j_tmp, "uuid"));
	if(tmp_const == NULL) {
		ast_log(LOG_WARNING, "Could not get uuid info.\n");
		ast_json_unref(j_tmp);
		return false;
	}
	uuid = ast_strdup(tmp_const);

	tmp = get_utc_timestamp();
	ast_json_object_set(j_tmp, "tm_update", ast_json_string_create(tmp));
	ast_free(tmp);

	tmp = db_get_update_str(j_tmp);
	if(tmp == NULL) {
		ast_log(LOG_WARNING, "Could not get update str.\n");
		ast_json_unref(j_tmp);
		ast_free(uuid);
		return false;
	}
	ast_json_unref(j_tmp);

	ast_asprintf(&sql, "update campaign set %s where in_use=1 and uuid=\"%s\";", tmp, uuid);
	ast_free(tmp);

	db_exec(sql);
	ast_free(sql);

	j_tmp = get_campaign(uuid);
	ast_free(uuid);
	if(j_tmp == NULL) {
		ast_log(LOG_WARNING, "Could not get updated campaign info.\n");
		return false;
	}
	send_manager_evt_out_campaign_update(j_tmp);
	ast_json_unref(j_tmp);

	return true;
}

/**
 * Update campaign status info.
 * @param uuid
 * @param status
 * @return
 */
bool update_campaign_status(const char* uuid, E_CAMP_STATUS_T status)
{
	int ret;
	char* tmp_status;
	struct ast_json* j_tmp;

	if(uuid == NULL) {
		ast_log(LOG_WARNING, "Invalid input parameters.\n");
		return false;
	}

	if(status == E_CAMP_START)		  tmp_status = "run";
	else if(status == E_CAMP_STOP)	  tmp_status = "stop";
	else if(status == E_CAMP_PAUSE)	 tmp_status = "pause";
	else if(status == E_CAMP_STARTING)  tmp_status = "running";
	else if(status == E_CAMP_STOPPING)  tmp_status = "stopping";
	else if(status == E_CAMP_PAUSING)   tmp_status = "pausing";
	else {
		ast_log(LOG_WARNING, "Invalid input parameters.\n");
		return false;
	}
	ast_log(LOG_NOTICE, "Update campaign status. uuid[%s], status[%d], status_string[%s]\n", uuid, status, tmp_status);

	// get campaign
	j_tmp = get_campaign(uuid);
	if(j_tmp == NULL) {
		ast_log(LOG_WARNING, "Could not find campaign. camp_uuid[%s]\n", uuid);
		return false;
	}

	// set status
	ast_json_object_set(j_tmp, "status", ast_json_integer_create(status));

	// update
	ret = update_campaign(j_tmp);
	ast_json_unref(j_tmp);
	if(ret == false) {
		ast_log(LOG_ERROR, "Could not update campaign status. camp_uuid[%s], status[%d]", uuid, status);
		return false;
	}

	return true;
}

/**
 * Get campaign for dialing.
 * @return
 */
struct ast_json* get_campaigns_by_status(E_CAMP_STATUS_T status)
{
	struct ast_json* j_res;
	struct ast_json* j_tmp;
	db_res_t* db_res;
	char* sql;

	// get "start" status campaign only.
	ast_asprintf(&sql, "select * from campaign where status = %d and in_use=1;",
			status
			);

	db_res = db_query(sql);
	ast_free(sql);
	if(db_res == NULL) {
		ast_log(LOG_WARNING, "Could not get campaign info.\n");
		return NULL;
	}

	j_res = ast_json_array_create();
	while(1) {
		j_tmp = db_get_record(db_res);
		if(j_tmp == NULL) {
			break;
		}
		ast_json_array_append(j_res, j_tmp);
	}
	db_free(db_res);

	return j_res;
}

/**
 * Get campaign for dialing.
 * @return
 */
struct ast_json* get_campaign_for_dialing(void)
{
	struct ast_json* j_res;
	db_res_t* db_res;
	char* sql;

	// get "start" status campaign only.
	ast_asprintf(&sql, "select * from campaign where status = %d and in_use = 1 order by %s limit 1;",
			E_CAMP_START,
			db_translate_function(E_FUNC_RANDOM)
			);

	db_res = db_query(sql);
	ast_free(sql);
	if(db_res == NULL) {
		ast_log(LOG_WARNING, "Could not get campaign info.\n");
		return NULL;
	}

	j_res = db_get_record(db_res);
	db_free(db_res);

	return j_res;
}

/**
 * return the possibility of status change to start
 */
bool is_startable_campgain(struct ast_json* j_camp)
{
	if(j_camp == NULL) {
		ast_log(LOG_WARNING, "Wrong input parameter.\n");
		return false;
	}

	return true;
}

/**
 * return the possibility of status change to stop
 */
bool is_stoppable_campgain(struct ast_json* j_camp)
{
	struct ao2_iterator iter;
	rb_dialing* dialing;
	const char* tmp_const;
	bool flg_dialing;
	int ret;

	if(j_camp == NULL) {
		ast_log(LOG_WARNING, "Wrong input parameter.\n");
		return false;
	}
	ast_log(LOG_VERBOSE, "is_stoppable_campgain.\n");

	flg_dialing = false;
	iter = rb_dialing_iter_init();
	while(1) {
		dialing = rb_dialing_iter_next(&iter);
		if(dialing == NULL) {
			break;
		}

		tmp_const = ast_json_string_get(ast_json_object_get(dialing->j_dialing, "camp_uuid"));
		if(tmp_const == NULL) {
			continue;
		}

		ret = strcmp(tmp_const, ast_json_string_get(ast_json_object_get(j_camp, "uuid")));
		if(ret == 0) {
			ast_log(LOG_NOTICE, "Found active call. dialing-uuid[%s], dialing-name[%s]\n",
					dialing->uuid, dialing->name
					);
			flg_dialing = true;
			break;
		}
	}
	rb_dialing_iter_destroy(&iter);

	if(flg_dialing == true) {
		return false;
	}

	return true;
}


