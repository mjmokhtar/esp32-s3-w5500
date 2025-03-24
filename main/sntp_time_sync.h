/*
 * sntp_time_sync.h
 *
 *  Created on: Jul 18, 2024
 *      Author: LattePanda
 */

#ifndef MAIN_SNTP_TIME_SYNC_H_
#define MAIN_SNTP_TIME_SYNC_H_

/**
 * Starts the NTP server synchronization task.
 */
void sntp_time_sync_task_start(void);

/**
 * Return local time if set.
 * @return local time buffer.
 */
char* sntp_time_sync_get_time(void);

#endif /* MAIN_SNTP_TIME_SYNC_H_ */
