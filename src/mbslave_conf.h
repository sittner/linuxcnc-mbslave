#ifndef _LCMBS_CONF_H
#define _LCMBS_CONF_H

#include <hal.h>

#include "mbslave_util.h"

#define LCMBS_PINTYPE_INVAL 0
#define LCMBS_PINTYPE_U16   (1 << 0)
#define LCMBS_PINTYPE_S16   (1 << 1)
#define LCMBS_PINTYPE_U32   (1 << 2)
#define LCMBS_PINTYPE_S32   (1 << 3)
#define LCMBS_PINTYPE_FLOAT (1 << 4)

#define LCMBS_PINFLAG_BYTESWAP (1 << 0)
#define LCMBS_PINFLAG_WORDSWAP (1 << 1)

typedef struct {
  char name[HAL_NAME_LEN];
  hal_bit_t **pin;
} LCMBS_CONF_BIT_PIN_T;

typedef struct {
  int start;
  LCMBS_VECT_T regs;
  LCMBS_VECT_T pins;
} LCMBS_CONF_REGS_T;

typedef struct {
  int start;
  LCMBS_VECT_T pins;
} LCMBS_CONF_BITS_T;

typedef struct {
  char name[HAL_NAME_LEN];
  int type;
  hal_type_t halType;
  int regCount;
  union {
    hal_u32_t **u;
    hal_s32_t **s;
    hal_float_t **f;
  } pin;
  int flags;
} LCMBS_CONF_REG_PIN_T;

typedef struct {
  LCMBS_CONF_REG_PIN_T *pin;
  int index;
} LCMBS_CONF_REG_T;

typedef struct {
  void *halData;
  size_t halSize;
  char name[HAL_NAME_LEN];
  LCMBS_VECT_T tcpListeners;
  LCMBS_CONF_REGS_T holdingRegs;
  LCMBS_CONF_REGS_T inputRegs;
  LCMBS_CONF_BITS_T inputs;
  LCMBS_CONF_BITS_T coils;
} LCMBS_CONF_SLAVE_T;

typedef struct {
  LCMBS_CONF_SLAVE_T *slave;
  int port;
  void *server;
} LCMBS_CONF_TCP_LSNR_T;

typedef struct {
  LCMBS_VECT_T slaves;
} LCMBS_CONF_T;

LCMBS_CONF_T *lcmbsConfParse(const char *filename);
void lcmbsConfFree(LCMBS_CONF_T *conf);

#endif

