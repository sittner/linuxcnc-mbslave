#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/eventfd.h>

#include "mbslave_util.h"
#include "mbslave_conf.h"
#include "mbslave_tcp.h"

const char *compName = "mbslave";

static int compId;
static int exitEvent;

static void sigtermHandler(int sig) {
  uint64_t u = 1;
  if (write(exitEvent, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error writing exit event\n", compName);
  }
}

int exportRegPins(LCMBS_CONF_SLAVE_T *slave, void **halData, LCMBS_CONF_REGS_T *regs, hal_pin_dir_t dir, const char *type) {
  int i;

  for (i = 0; i < regs->pins.count; i++) {
    LCMBS_CONF_REG_PIN_T *pin = lcmbsVectGet(&regs->pins, i);
    int ret;
    switch (pin->halType) {
      case HAL_U32:
        pin->pin.u = (hal_u32_t **) *halData;
        *halData += sizeof(hal_u32_t *);
        ret = hal_pin_u32_newf(dir, pin->pin.u, compId, "%s.%s.%s", compName, slave->name, pin->name);
        break;
      case HAL_S32:
        pin->pin.s = (hal_s32_t **) *halData;
        *halData += sizeof(hal_s32_t *);
        ret = hal_pin_s32_newf(dir, pin->pin.s, compId, "%s.%s.%s", compName, slave->name, pin->name);
        break;
      case HAL_FLOAT:
        pin->pin.f = (hal_float_t **) *halData;
        *halData += sizeof(hal_float_t *);
        ret = hal_pin_float_newf(dir, pin->pin.f, compId, "%s.%s.%s", compName, slave->name, pin->name);
        break;
      default:
        ret = 0;
    }
    if (ret) {
      fprintf(stderr, "%s: ERROR: Unable to export %s pin %s.%s.\n", compName, type, slave->name, pin->name);
      return -1;
    }
  }

  return 0;
}

int exportBitPins(LCMBS_CONF_SLAVE_T *slave, void **halData, LCMBS_CONF_BITS_T *bits, hal_pin_dir_t dir, const char *type) {
  int i;

  for (i = 0; i < bits->pins.count; i++) {
    LCMBS_CONF_BIT_PIN_T *pin = lcmbsVectGet(&bits->pins, i);
    pin->pin = (hal_bit_t **) *halData;
    *halData += sizeof(hal_bit_t *);
    if (hal_pin_bit_newf(dir, pin->pin, compId, "%s.%s.%s", compName, slave->name, pin->name)) {
      fprintf(stderr, "%s: ERROR: Unable to export %s pin %s.%s.\n", compName, type, slave->name, pin->name);
      return -1;
    }
  }

  return 0;
}

int startTcpListeners(LCMBS_CONF_SLAVE_T *slave) {
  int i;

  for (i = 0; i < slave->tcpListeners.count; i++) {
    LCMBS_CONF_TCP_LSNR_T *listener = lcmbsVectGet(&slave->tcpListeners, i);
    LCMBS_TCP_SERVER_DATA_T *server = lcmbsTcpStart(listener);
    if (!server) {
      fprintf(stderr, "%s: ERROR: Unable to start tcp listener on port %d.\n", compName, listener->port);
      return -1;
    }

    listener->server = server;
  }

  return 0;
}

int startSlaves(LCMBS_CONF_T *conf) {
  size_t i, j;

  for (i = 0; i < conf->slaves.count; i++) {
    LCMBS_CONF_SLAVE_T *slave = lcmbsVectGet(&conf->slaves, i);

    void *halData = hal_malloc(slave->halSize);
    if (!halData) {
      fprintf(stderr, "%s: ERROR: Unable alloc hal data for slave %s.\n", compName, slave->name);
      return -1;
    }
    slave->halData = halData;

    // export holding register pins
    if (exportRegPins(slave, &halData, &slave->holdingRegs, HAL_IO, "holdingRegister")) {
      return -1;
    }

    // export input register pins
    if (exportRegPins(slave, &halData, &slave->inputRegs, HAL_IN, "inputRegister")) {
      return -1;
    }

    // export input pins
    if (exportBitPins(slave, &halData, &slave->inputs, HAL_IN, "input")) {
      return -1;
    }

    // export coil pins
    if (exportBitPins(slave, &halData, &slave->coils, HAL_IO, "coil")) {
      return -1;
    }

    // start TCP listeners
    if (startTcpListeners(slave)) {
      return -1;
    }
  }

  return 0;
}

void stopSlaves(LCMBS_CONF_T *conf) {
  size_t i, j;

  for (i = 0; i < conf->slaves.count; i++) {
    LCMBS_CONF_SLAVE_T *slave = lcmbsVectGet(&conf->slaves, i);

    // stop TCP listeners
    for (j = 0; j < slave->tcpListeners.count; j++) {
      LCMBS_CONF_TCP_LSNR_T *listener = lcmbsVectGet(&slave->tcpListeners, j);
      LCMBS_TCP_SERVER_DATA_T *server = (LCMBS_TCP_SERVER_DATA_T *) listener->server;

      if (server != NULL) {
        lcmbsTcpStop(server);
      }

      listener->server = server;
    }
  }
}

int main(int argc, char **argv) {
  int ret = 1;
  char *filename;
  LCMBS_CONF_T *conf;
  uint64_t u;

  // get config file name
  if (argc != 2) {
    fprintf(stderr, "%s: ERROR: invalid arguments\n", compName);
    goto fail0;
  }
  filename = argv[1];

  // initialize hal
  compId = hal_init(compName);
  if (compId < 1) {
    fprintf(stderr, "%s: ERROR: hal_init failed\n", compName);
    goto fail0;
  }

  // parse config file
  conf = lcmbsConfParse(filename);
  if (!conf) {
    goto fail1;
  }

  // create exit event
  exitEvent = eventfd(0, 0);
  if (exitEvent < 0) {
    fprintf(stderr, "%s: ERROR: unable to create exit event\n", compName);
    goto fail2;
  }


  // install signal handler
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = &sigtermHandler;
  if (sigaction(SIGTERM, &act, NULL) < 0)
  {
    fprintf(stderr, "%s: ERROR: Unable to register SIGTERM handler.", compName);
    goto fail3;
  }

  // start slaves
  if (startSlaves(conf)) {
    goto fail4;
  }

  // everything is fine
  ret = 0;
  hal_ready(compId);

  // wait for SIGTERM
  read(exitEvent, &u, sizeof(uint64_t));

fail4:
  stopSlaves(conf);
fail3:
  close(exitEvent);
fail2:
  lcmbsConfFree(conf);
fail1:
  hal_exit(compId);
fail0:
  return ret;
}

