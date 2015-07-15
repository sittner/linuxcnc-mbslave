#ifndef _LCMBS_PROT_H
#define _LCMBS_PROT_H

#include <stdint.h>
#include <stddef.h>

#include "mbslave_util.h"
#include "mbslave_conf.h"

#define MB_FNK_READ_COIL_STATUS		1
#define MB_FNK_READ_INPUT_STATUS	2
#define MB_FNK_READ_HOLDING_REG		3
#define MB_FNK_READ_INPUT_REG		4
#define MB_FNK_FORCE_SINGLE_COIL	5
#define MB_FNK_PRESET_SINGLE_REG	6
#define MB_FNK_FORCE_MULTI_COIL		15
#define MB_FNK_PRESET_MULTI_REG		16

#define MB_ERR_OK			0
#define MB_ERR_INVALID_FUNCTION		1
#define MB_ERR_ILLEGAL_DATA_ADDRESS	2
#define MB_ERR_ILLEGAL_DATA_VALUE	3
#define MB_ERR_SLAVE_DEVICE_FAILURE	4

int lcmbsProtProc(LCMBS_CONF_SLAVE_T *slave, LCMBS_VECT_T *in, LCMBS_VECT_T *out);

#endif

