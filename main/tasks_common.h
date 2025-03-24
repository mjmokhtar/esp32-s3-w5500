/*
 * tasks_common.h
 *
 *  Created on: Jul 5, 2024
 *      Author: LattePanda
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

// WiFi application task
#define WIFI_APP_TASK_STACK_SIZE			4096
#define WIFI_APP_TASK_PRIORITY				5
#define WIFI_APP_TASK_CORE_ID				0

// HTTP Server task
#define HTTP_SERVER_TASK_STACK_SIZE			16384
#define HTTP_SERVER_TASK_PRIORITY			4
#define HTTP_SERVER_TASK_CORE_ID			0

// HTTP Server Monitor task
#define HTTP_SERVER_MONITOR_STACK_SIZE		4096
#define HTTP_SERVER_MONITOR_PRIORITY		3
#define HTTP_SERVER_MONITOR_CORE_ID			0

// WiFi Reset Button task
#define WIFI_RESET_BUTTON_TASK_STACK_SIZE	2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY		6
#define WIFI_RESET_BUTTON_TASK_CORE_ID		0

// SNTP Time Sync task
#define SNTP_TIME_SYNC_TASK_STACK_SIZE		4096
#define SNTP_TIME_SYNC_TASK_PRIORITY		4
#define SNTP_TIME_SYNC_TASK_CORE_ID			1


// Ethernet Manager task
#define ETH_APP_TASK_STACK_SIZE             4096
#define ETH_APP_TASK_PRIORITY               5
#define ETH_APP_TASK_CORE_ID                1

#endif /* MAIN_TASKS_COMMON_H_ */
