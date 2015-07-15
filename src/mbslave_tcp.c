#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/eventfd.h>

#include "mbslave_tcp.h"
#include "mbslave_util.h"
#include "mbslave_prot.h"

#define LISTEN_MAXPENDING 10
#define SELECT_TIMEOUT    500
#define HEADER_LEN        6
#define BLOCK_SIZE        4096

#define SET_TIMEVAL_MS(tv, val) { tv.tv_sec = val / 1000; tv.tv_usec = (val % 1000) * 1000; }


typedef struct {
  LCMBS_TCP_SERVER_DATA_T *server;
  int sd;
  char addr[INET_ADDRSTRLEN];
  int port;

} LCMBS_TCP_CLIENT_DATA_T;


void *lcmbsTcpServerThread(void *arg);
int lcmbsTcpNewConnection(LCMBS_TCP_SERVER_DATA_T *server);
void *lcmbsTcpClientThread(void *arg);


LCMBS_TCP_SERVER_DATA_T *lcmbsTcpStart(LCMBS_CONF_TCP_LSNR_T *listener) {
  LCMBS_TCP_SERVER_DATA_T *server;
  int optval;
  struct sockaddr_in addr;

  // alloc memory
  server = calloc(1, sizeof(LCMBS_TCP_SERVER_DATA_T));
  if (!server) {
    goto fail0;
  }

  // initialize fields
  server->listener = listener;
  server->client_count_lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  server->client_count_zero = (pthread_cond_t) PTHREAD_COND_INITIALIZER; 

  // create exit flag event
  if ((server->exit_flag = eventfd(0, 0)) < 0) {
    goto fail1;
  }

  // create socket
  if((server->sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    goto fail2;
  }

  // set option SO_REUSEADDR to avoid "wait for FIN" hangs on restart
  optval = 1;
  setsockopt(server->sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  // bind to tcp port
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(listener->port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(server->sd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    goto fail3;
  }

  // listen on port
  if (listen(server->sd, LISTEN_MAXPENDING)) {
    goto fail3;
  }

  // start server thread
  if (pthread_create(&server->thread, 0, lcmbsTcpServerThread, server)) {
    goto fail3;
  }

  return server;

fail3:
  close(server->sd);
fail2:
  close(server->exit_flag);
fail1:
  free(server);
fail0:
  return NULL;
}

void lcmbsTcpStop(LCMBS_TCP_SERVER_DATA_T *server) {
  // set exit flag
  uint64_t u = 1;
  write(server->exit_flag, &u, sizeof(uint64_t));

  // wait for server thread
  pthread_join(server->thread, NULL); 

  // wait for clients
  pthread_mutex_lock(&server->client_count_lock);
  while (server->client_count > 0) {
    pthread_cond_wait(&server->client_count_zero, &server->client_count_lock);
  }
  pthread_mutex_unlock(&server->client_count_lock);

  // close server socket
  close(server->sd);
}

void *lcmbsTcpServerThread(void *arg) {
  LCMBS_TCP_SERVER_DATA_T *server = (LCMBS_TCP_SERVER_DATA_T *) arg;

  fd_set set;
  int max_fd;

  while(1) {
    // check for new client connect
    FD_ZERO(&set);
    max_fd = -1;
    FD_SET(server->exit_flag, &set);
    if (max_fd < server->exit_flag) max_fd = server->exit_flag;
    FD_SET(server->sd, &set);
    if (max_fd < server->sd) max_fd = server->sd;
    if (select(max_fd + 1, &set, 0, 0, NULL) < 0) {
      break;
    }
 
    // check for exit event
    if (FD_ISSET(server->exit_flag, &set)) {
      break;
    }

    // check for new connection
    if (!FD_ISSET(server->sd, &set)) {
      continue;
    }

    // accept new connection
    lcmbsTcpNewConnection(server);
  }

  return NULL;
}

int lcmbsTcpNewConnection(LCMBS_TCP_SERVER_DATA_T *server) {
  struct sockaddr_in client_addr;
  socklen_t client_addr_len;
  int client_sd;
  pthread_t client_thread;
  LCMBS_TCP_CLIENT_DATA_T *client;

  // accept connection
  client_addr_len = sizeof(client_addr);
  if ((client_sd = accept(server->sd, (struct sockaddr *) &client_addr, &client_addr_len)) < 0) {
    goto fail0;
  }

  // initialize thread data
  client = (LCMBS_TCP_CLIENT_DATA_T *)calloc(1, sizeof(LCMBS_TCP_CLIENT_DATA_T));
  if (!client) {
    goto fail1;
  }
  client->server = server;
  client->sd = client_sd;
  strncpy(client->addr, inet_ntoa(client_addr.sin_addr), INET_ADDRSTRLEN);
  client->port = client_addr.sin_port;

  // start client thread
  if (pthread_create(&client_thread, 0, lcmbsTcpClientThread, client)) {
    goto fail2;
  }

  // increment clinet count 
  pthread_mutex_lock(&server->client_count_lock);
  server->client_count++;
  pthread_mutex_unlock(&server->client_count_lock);

  pthread_detach(client_thread);

  return 0;

fail2:
  free(client);
fail1:
  close(client_sd);
fail0:
  return -1;
}

void *lcmbsTcpClientThread(void *arg) {
  LCMBS_TCP_CLIENT_DATA_T *client = (LCMBS_TCP_CLIENT_DATA_T *) arg;
  LCMBS_TCP_SERVER_DATA_T *server = client->server;
  LCMBS_CONF_TCP_LSNR_T *listener = server->listener;
  LCMBS_CONF_SLAVE_T *slave = listener->slave;

  fd_set set;
  int max_fd, count;
  struct timeval timeout;
  uint8_t header[HEADER_LEN];
  ssize_t header_pos;
  uint16_t tid, prot, len;
  LCMBS_VECT_T rcvbuf, sndbuf;
  ssize_t rcvd;

  // loop to receive data
  header_pos = 0;
  lcmbsVectInit(&rcvbuf, 1);
  lcmbsVectInit(&sndbuf, 1);
  while (1) {
    // check for new data
    FD_ZERO(&set);
    max_fd = -1;
    FD_SET(server->exit_flag, &set);
    if (max_fd < server->exit_flag) max_fd = server->exit_flag;
    FD_SET(client->sd, &set);
    if (max_fd < client->sd) max_fd = client->sd;
    SET_TIMEVAL_MS(timeout, SELECT_TIMEOUT);
    if ((count = select(max_fd + 1, &set, 0, 0, &timeout)) < 0) {
      break;
    }

    // check for timeout
    if (count == 0) {
      header_pos = 0;
      lcmbsVectClear(&rcvbuf);
      continue;
    }

    // check for exit event
    if (FD_ISSET(server->exit_flag, &set)) {
      break;
    }

    // check for data
    if (!FD_ISSET(client->sd, &set)) {
      continue;
    }

    // receive header
    if (header_pos < HEADER_LEN) {
      if ((rcvd = read(client->sd, &header[header_pos], HEADER_LEN - header_pos)) <= 0) {
        break;
      }

      // check for full packet
      header_pos += rcvd;
      if (header_pos < HEADER_LEN) {
        continue;
      }

      // read header data
      tid = ntohs(*((uint16_t *) &header[0]));
      prot = ntohs(*((uint16_t *) &header[2]));
      len = ntohs(*((uint16_t *) &header[4]));

      // check protocol number
      if (prot != 0) {
        header_pos = 0;
        continue;
      }

      // check buffer size and allocate buffer
      if (!lcmbsVectEnsureSize(&rcvbuf, len)) {
        break;
      }

      // reset data pointer
      lcmbsVectClear(&rcvbuf);
      continue;
    }

    // receive data
    if ((rcvd = read(client->sd,  rcvbuf.data + rcvbuf.count, len - rcvbuf.count)) <= 0) {
      break;
    }

    // check for full packet
    rcvbuf.count += rcvd;
    if (rcvbuf.count < len) {
      continue;
    }

    // process data
    len = lcmbsProtProc(slave, &rcvbuf, &sndbuf);

    // send response
    if (len > 0) {
      // send header
      *((uint16_t *) &header[0]) = htons(tid);
      *((uint16_t *) &header[2]) = 0;
      *((uint16_t *) &header[4]) = htons(len);
      if (send(client->sd, header, HEADER_LEN, MSG_MORE) != HEADER_LEN) {
        break;
      }

      // send payload
      if (send(client->sd, sndbuf.data, sndbuf.count, 0) != len) {
        break;
      }
    }

    // reset receive buffers
    header_pos = 0;
    lcmbsVectClear(&rcvbuf);
  }

  // close client socket
  close(client->sd);

  // free thread data
  lcmbsVectFree(&rcvbuf);
  lcmbsVectFree(&sndbuf);
  free(client);

  // decrement clinet count 
  pthread_mutex_lock(&server->client_count_lock);
  server->client_count--;
  if (server->client_count == 0) {
    pthread_cond_signal(&server->client_count_zero);
  }
  pthread_mutex_unlock(&server->client_count_lock);

  return NULL;
}


