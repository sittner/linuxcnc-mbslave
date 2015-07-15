#ifndef _LCMBS_TCP_H
#define _LCMBS_TCP_H

#include <pthread.h>

#include "mbslave_conf.h"

typedef struct {
  LCMBS_CONF_TCP_LSNR_T *listener;
  int sd;
  int client_count;
  pthread_t thread;
  pthread_mutex_t client_count_lock;
  pthread_cond_t client_count_zero;
  int exit_flag;
} LCMBS_TCP_SERVER_DATA_T;

LCMBS_TCP_SERVER_DATA_T *lcmbsTcpStart(LCMBS_CONF_TCP_LSNR_T *listener);
void lcmbsTcpStop(LCMBS_TCP_SERVER_DATA_T *server);

#endif

