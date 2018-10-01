//
//  Copyright (C) 2015 Sascha Ittner <sascha.ittner@modusoft.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <expat.h>
#include <signal.h>

#include "mbslave_conf.h"

#define BUFFSIZE 4096

typedef enum {
  lcmbsConfTypeNone,
  lcmbsConfTypeSlaves,
  lcmbsConfTypeSlave,
  lcmbsConfTypeTcpListener,
  lcmbsConfTypeSerialListener,
  lcmbsConfTypeHoldingRegs,
  lcmbsConfTypeHoldingReg,
  lcmbsConfTypeHoldingBitReg,
  lcmbsConfTypeHoldingBitRegPin,
  lcmbsConfTypeInputRegs,
  lcmbsConfTypeInputReg,
  lcmbsConfTypeInputBitReg,
  lcmbsConfTypeInputBitRegPin,
  lcmbsConfTypeInputs,
  lcmbsConfTypeInput,
  lcmbsConfTypeCoils,
  lcmbsConfTypeCoil
} LCMBS_CONF_TYPE_T;

typedef struct {
  XML_Parser xmlParser;
  LCMBS_CONF_TYPE_T currConfType;
  LCMBS_CONF_SLAVE_T *currSlave;
  LCMBS_VECT_T *currBitpins;
  LCMBS_CONF_T *conf;
} LCMBS_CONF_PARSER_T;

typedef struct {
  const char *nodeName;
  LCMBS_CONF_TYPE_T currState;
  LCMBS_CONF_TYPE_T nextState;
  void (*parser)(LCMBS_CONF_PARSER_T *, const char **);
  void (*validator)(LCMBS_CONF_PARSER_T *);
} LCMBS_CONF_STATE_T;

LCMBS_CONF_T *lcmbsConfParse(const char *filename);
void lcmbsConfFree(LCMBS_CONF_T *conf);

void lcmbsConfInitRegs(LCMBS_CONF_REGS_T *regs);
void lcmbsConfFreeRegs(LCMBS_CONF_REGS_T *regs);
void lcmbsConfInitBits(LCMBS_CONF_BITS_T *bits);
void lcmbsConfFreeBits(LCMBS_CONF_BITS_T *bits);

void lcmbsConfXmlStartHandler(void *data, const char *el, const char **attr);
void lcmbsConfXmlEndHandler(void *data, const char *el);

void lcmbsConfParseSlaveAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseTcpLsnrAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseSerLsnrAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseListAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, int *start, const char *type);
void lcmbsConfParseBitPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_BITS_T *bits, const char *type);
void lcmbsConfParseRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type);
void lcmbsConfParseBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type);
void lcmbsConfParseBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type);
void lcmbsConfParseHoldingRegsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseHoldingRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseHoldingBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseHoldingBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputRegsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseInputAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseCoilsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);
void lcmbsConfParseCoilAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr);

static const LCMBS_CONF_STATE_T lcmbsConfStates[] = {
  { "modbusSlaves",	lcmbsConfTypeNone,		lcmbsConfTypeSlaves,		NULL,					NULL },
  { "modbusSlave",	lcmbsConfTypeSlaves,		lcmbsConfTypeSlave,		lcmbsConfParseSlaveAttrs,		NULL },
  { "tcpListener",	lcmbsConfTypeSlave,		lcmbsConfTypeTcpListener,	lcmbsConfParseTcpLsnrAttrs,		NULL },
  { "serialListener",	lcmbsConfTypeSlave,		lcmbsConfTypeSerialListener,	lcmbsConfParseSerLsnrAttrs,		NULL },
  { "holdingRegisters",	lcmbsConfTypeSlave,		lcmbsConfTypeHoldingRegs,	lcmbsConfParseHoldingRegsAttrs,		NULL },
  { "pin",		lcmbsConfTypeHoldingRegs,	lcmbsConfTypeHoldingReg,	lcmbsConfParseHoldingRegAttrs,		NULL },
  { "bitRegister",	lcmbsConfTypeHoldingRegs,	lcmbsConfTypeHoldingBitReg,	lcmbsConfParseHoldingBitRegAttrs,	NULL },
  { "pin",		lcmbsConfTypeHoldingBitReg,	lcmbsConfTypeHoldingBitRegPin,	lcmbsConfParseHoldingBitRegPinAttrs,	NULL },
  { "inputRegisters",	lcmbsConfTypeSlave,		lcmbsConfTypeInputRegs,		lcmbsConfParseInputRegsAttrs,		NULL },
  { "pin",		lcmbsConfTypeInputRegs,		lcmbsConfTypeInputReg,		lcmbsConfParseInputRegAttrs,		NULL },
  { "bitRegister",	lcmbsConfTypeInputRegs,		lcmbsConfTypeInputBitReg,	lcmbsConfParseInputBitRegAttrs,		NULL },
  { "pin",		lcmbsConfTypeInputBitReg,	lcmbsConfTypeInputBitRegPin,	lcmbsConfParseInputBitRegPinAttrs,	NULL },
  { "inputs",		lcmbsConfTypeSlave,		lcmbsConfTypeInputs,		lcmbsConfParseInputsAttrs,		NULL },
  { "pin",		lcmbsConfTypeInputs,		lcmbsConfTypeInput,		lcmbsConfParseInputAttrs,		NULL },
  { "coils",		lcmbsConfTypeSlave,		lcmbsConfTypeCoils,		lcmbsConfParseCoilsAttrs,		NULL },
  { "pin",		lcmbsConfTypeCoils,		lcmbsConfTypeCoil,		lcmbsConfParseCoilAttrs,		NULL },
  { NULL }
};

LCMBS_CONF_T *lcmbsConfParse(const char *filename) {
  int done;
  char buffer[BUFFSIZE];
  FILE *file;
  LCMBS_CONF_PARSER_T parser;
  LCMBS_CONF_T *ret = NULL;

  // open file
  file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "%s: ERROR: unable to open config file %s\n", compName, filename);
    goto fail0;
  }

  // allocate config mem
  parser.conf = calloc(1, sizeof(LCMBS_CONF_T));
  if (!parser.conf) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for config\n", compName);
    goto fail1;
  }

  // initialize config
  lcmbsVectInit(&parser.conf->slaves, sizeof(LCMBS_CONF_SLAVE_T));

  // create xml parser
  parser.xmlParser = XML_ParserCreate(NULL);
  if (!parser.xmlParser) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for parser\n", compName);
    goto fail2;
  }

  // initialize parser state
  parser.currConfType = lcmbsConfTypeNone;
  XML_SetUserData(parser.xmlParser, &parser);

  // setup handlers
  XML_SetElementHandler(parser.xmlParser, lcmbsConfXmlStartHandler, lcmbsConfXmlEndHandler);

  // parse file
  for (done=0; !done;) {
    // read block
    int len = fread(buffer, 1, BUFFSIZE, file);
    if (ferror(file)) {
      fprintf(stderr, "%s: ERROR: Couldn't read from file %s\n", compName, filename);
      goto fail3;
    }

    // check for EOF
    done = feof(file);

    // parse current block
    if (!XML_Parse(parser.xmlParser, buffer, len, done)) {
      fprintf(stderr, "%s: ERROR: Parse error at line %u: %s\n", compName,
        (unsigned int)XML_GetCurrentLineNumber(parser.xmlParser),
        XML_ErrorString(XML_GetErrorCode(parser.xmlParser)));
      goto fail3;
    }
  }

  // result is ok now
  ret = parser.conf;

fail3:
  XML_ParserFree(parser.xmlParser);
fail2:
  if (!ret) {
    lcmbsConfFree(parser.conf);
  }
fail1:
  fclose(file);
fail0:
  return ret;
}

void lcmbsConfFree(LCMBS_CONF_T *conf) {
  size_t i;

  if (!conf) {
    return;
  }

  // free slaves
  for (i = 0; i < conf->slaves.count; i++) {
    LCMBS_CONF_SLAVE_T *slave = lcmbsVectGet(&conf->slaves, i);
    lcmbsVectFree(&slave->tcpListeners);
    lcmbsConfFreeRegs(&slave->holdingRegs);
    lcmbsConfFreeRegs(&slave->inputRegs);
    lcmbsConfFreeBits(&slave->inputs);
    lcmbsConfFreeBits(&slave->coils);
  }
  lcmbsVectFree(&conf->slaves);

  free(conf);
}

void lcmbsConfInitRegs(LCMBS_CONF_REGS_T *regs) {
  regs->start = -1;
  lcmbsVectInit(&regs->regs, sizeof(LCMBS_CONF_REG_T));
  lcmbsVectInit(&regs->pins, sizeof(LCMBS_CONF_REG_PIN_T));
}

void lcmbsConfFreeRegs(LCMBS_CONF_REGS_T *regs) {
  size_t i;
  for (i = 0; i < regs->regs.count; i++) {
    LCMBS_CONF_REG_T *reg = lcmbsVectGet(&regs->regs, i);
    if (reg->bitpins != NULL) {
      lcmbsVectFree(reg->bitpins);
      free(reg->bitpins);
    }
  }

  lcmbsVectFree(&regs->regs);
  lcmbsVectFree(&regs->pins);
}

void lcmbsConfInitBits(LCMBS_CONF_BITS_T *bits) {
  bits->start = -1;
  lcmbsVectInit(&bits->pins, sizeof(LCMBS_CONF_BIT_PIN_T));
}

void lcmbsConfFreeBits(LCMBS_CONF_BITS_T *bits) {
  lcmbsVectFree(&bits->pins);
}

void lcmbsConfXmlStartHandler(void *data, const char *el, const char **attr) {
  LCMBS_CONF_PARSER_T *parser = (LCMBS_CONF_PARSER_T *) data;
  static const LCMBS_CONF_STATE_T *state;

  for (state = lcmbsConfStates; state->nodeName; state++) {
    // check current state
    if (parser->currConfType != state->currState) {
      continue;
    }

    // check node name
    if (strcmp(state->nodeName, el)) {
      continue;
    }

    // set state
    parser->currConfType = state->nextState;

    // call parser if applicable
    if (state->parser != NULL) {
      state->parser(parser, attr);
    }

    return;
  }

  fprintf(stderr, "%s: ERROR: unexpected node %s found\n", compName, el);
  XML_StopParser(parser->xmlParser, 0);
}

void lcmbsConfXmlEndHandler(void *data, const char *el) {
  LCMBS_CONF_PARSER_T *parser = (LCMBS_CONF_PARSER_T *) data;
  static const LCMBS_CONF_STATE_T *state;

  for (state = lcmbsConfStates; state->nodeName; state++) {
    // check current state
    if (parser->currConfType != state->nextState) {
      continue;
    }

    // check node name
    if (strcmp(state->nodeName, el)) {
      continue;
    }

    // set state
    parser->currConfType = state->currState;

    // call validator if applicable
    if (state->validator != NULL) {
      state->validator(parser);
    }

    return;
  }

  fprintf(stderr, "%s: ERROR: unexpected close tag %s found\n", compName, el);
  XML_StopParser(parser->xmlParser, 0);
}

void lcmbsConfParseSlaveAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  // create new slave
  LCMBS_CONF_SLAVE_T *slave = lcmbsVectPut(&parser->conf->slaves);
  if (!slave) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for slave\n", compName);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // initialize attributes
  lcmbsVectInit(&slave->tcpListeners, sizeof(LCMBS_CONF_TCP_LSNR_T));
  lcmbsConfInitRegs(&slave->holdingRegs);
  lcmbsConfInitRegs(&slave->inputRegs);
  lcmbsConfInitBits(&slave->inputs);
  lcmbsConfInitBits(&slave->coils);

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(slave->name, val, HAL_NAME_LEN);
      slave->name[HAL_NAME_LEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid slave attribute %s\n", compName, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check for name
  if (slave->name[0] == 0) {
    fprintf(stderr, "%s: ERROR: No slave name given\n", compName);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // set current slave
  parser->currSlave = slave;
}

void lcmbsConfParseTcpLsnrAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  // create new tcpListener
  LCMBS_CONF_SLAVE_T *slave = parser->currSlave;
  LCMBS_CONF_TCP_LSNR_T *listener = lcmbsVectPut(&slave->tcpListeners);
  if (!listener) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for tcpListener\n", compName);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // initialize attributes
  listener->slave = slave;
  listener->port = -1;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse port number
    if (strcmp(name, "port") == 0) {
      listener->port = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid tcpListener attribute %s\n", compName, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check port number
  if (listener->port < 0 || listener->port > 65535) {
    fprintf(stderr, "%s: ERROR: Invalid port number %d\n", compName, listener->port);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }
}

void lcmbsConfParseSerLsnrAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  fprintf(stderr, "%s: ERROR: serialListener is currently not supported\n", compName);
  XML_StopParser(parser->xmlParser, 0);
}

void lcmbsConfParseListAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, int *start, const char *type) {
  // check for unique node
  if (*start >= 0) {
    fprintf(stderr, "%s: ERROR: %s node must be unique per slave\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
  }

  // initialize attributes
  *start = -1;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse port number
    if (strcmp(name, "start") == 0) {
      *start = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid %s attribute %s\n", compName, type, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check start number
  if (*start < 0 || *start > 65535) {
    fprintf(stderr, "%s: ERROR: Invalid %s start number %d\n", compName, type, *start);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }
}

void lcmbsConfParseBitPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_BITS_T *bits, const char *type) {
  // create new pin
  LCMBS_CONF_SLAVE_T *slave = parser->currSlave;
  LCMBS_CONF_BIT_PIN_T *pin = lcmbsVectPut(&bits->pins);
  if (!pin) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s pin\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(pin->name, val, HAL_NAME_LEN);
      pin->name[HAL_NAME_LEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid %s attribute %s\n", compName, type, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check for name
  if (pin->name[0] == 0) {
    fprintf(stderr, "%s: ERROR: No %s name given\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // set attributes
  slave->halSize += sizeof(hal_bit_t *);
}

void lcmbsConfParseRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type) {
  int i;
  size_t halSize;

  // create new pin
  LCMBS_CONF_SLAVE_T *slave = parser->currSlave;
  LCMBS_CONF_REG_PIN_T *pin = lcmbsVectPut(&regs->pins);
  if (!pin) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s pin\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // initialize attributes
  pin->type = LCMBS_PINTYPE_INVAL;
  pin->halType = HAL_TYPE_UNSPECIFIED;
  pin->regCount = 0;
  halSize = 0;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(pin->name, val, HAL_NAME_LEN);
      pin->name[HAL_NAME_LEN - 1] = 0;
      continue;
    }

    // parse type
    if (strcmp(name, "type") == 0) {
      if (strcmp(val, "u16") == 0) {
        pin->type = LCMBS_PINTYPE_U16;
        pin->halType = HAL_U32;
        pin->regCount = 1;
        halSize = sizeof(hal_u32_t *);
        continue;
      }
      if (strcmp(val, "s16") == 0) {
        pin->type = LCMBS_PINTYPE_S16;
        pin->halType = HAL_S32;
        pin->regCount = 1;
        halSize = sizeof(hal_s32_t *);
        continue;
      }
      if (strcmp(val, "u32") == 0) {
        pin->type = LCMBS_PINTYPE_U32;
        pin->halType = HAL_U32;
        pin->regCount = 2;
        halSize = sizeof(hal_u32_t *);
        continue;
      }
      if (strcmp(val, "s32") == 0) {
        pin->type = LCMBS_PINTYPE_S32;
        pin->halType = HAL_S32;
        pin->regCount = 2;
        halSize = sizeof(hal_s32_t *);
        continue;
      }
      if (strcmp(val, "float") == 0) {
        pin->type = LCMBS_PINTYPE_FLOAT;
        pin->halType = HAL_FLOAT;
        pin->regCount = 2;
        halSize = sizeof(hal_float_t *);
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid %s data type %s\n", compName, type, val);
      XML_StopParser(parser->xmlParser, 0);
      return;
    }

    // parse byteswap
    if (strcmp(name, "byteswap") == 0) {
      if (strcmp(val, "true") == 0) {
        pin->flags |= LCMBS_PINFLAG_BYTESWAP;
      } else {
        pin->flags &= ~LCMBS_PINFLAG_BYTESWAP;
      }
      continue;
    }

    // parse wordswap
    if (strcmp(name, "wordswap") == 0) {
      if (strcmp(val, "true") == 0) {
        pin->flags |= LCMBS_PINFLAG_WORDSWAP;
      } else {
        pin->flags &= ~LCMBS_PINFLAG_WORDSWAP;
      }
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid %s attribute %s\n", compName, type, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check for name
  if (pin->name[0] == 0) {
    fprintf(stderr, "%s: ERROR: No %s name given\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check for type
  if (pin->type == LCMBS_PINTYPE_INVAL) {
    fprintf(stderr, "%s: ERROR: No data type given\n", compName);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // set attributes
  slave->halSize += halSize;

  // create register mappings
  for (i=0; i<pin->regCount; i++) {
    LCMBS_CONF_REG_T *reg = lcmbsVectPut(&regs->regs);

    if (!reg) {
      fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s\n", compName, type);
      XML_StopParser(parser->xmlParser, 0);
      return;
    }

    // set attributes
    reg->index = i;
    reg->pin = pin;
    reg->bitpins = NULL;
  }
}

void lcmbsConfParseBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type) {
  LCMBS_CONF_REG_T *reg = lcmbsVectPut(&regs->regs);

  if (!reg) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // set attributes
  reg->pin = NULL;
  reg->index = 0;
  reg->bitpins = malloc(sizeof(LCMBS_VECT_T));
  if (!reg->bitpins) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s\n bitpin vector", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }
  lcmbsVectInit(reg->bitpins, sizeof(LCMBS_CONF_REG_BIT_PIN_T));

  parser->currBitpins = reg->bitpins;
}

void lcmbsConfParseBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr, LCMBS_CONF_REGS_T *regs, const char *type) {
  // create new pin
  LCMBS_CONF_SLAVE_T *slave = parser->currSlave;
  LCMBS_CONF_REG_BIT_PIN_T *pin = lcmbsVectPut(parser->currBitpins);
  if (!pin) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for %s pin\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse bit number
    if (strcmp(name, "bit") == 0) {
      pin->bit = atoi(val);
      if (pin->bit < 0 || pin->bit > 15) {
        fprintf(stderr, "%s: ERROR: Invalid %s bit number %d\n", compName, type, pin->bit);
        XML_StopParser(parser->xmlParser, 0);
        return;
      }
      continue;
    }

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(pin->name, val, HAL_NAME_LEN);
      pin->name[HAL_NAME_LEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid %s attribute %s\n", compName, type, name);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // check for name
  if (pin->name[0] == 0) {
    fprintf(stderr, "%s: ERROR: No %s name given\n", compName, type);
    XML_StopParser(parser->xmlParser, 0);
    return;
  }

  // set attributes
  slave->halSize += sizeof(hal_bit_t *);
}

void lcmbsConfParseHoldingRegsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseListAttrs(parser, attr, &parser->currSlave->holdingRegs.start, "holdingRegisters");
}

void lcmbsConfParseHoldingRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseRegPinAttrs(parser, attr, &parser->currSlave->holdingRegs, "holdingRegister");
}

void lcmbsConfParseHoldingBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitRegAttrs(parser, attr, &parser->currSlave->holdingRegs, "holdingRegister");
}

void lcmbsConfParseHoldingBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitRegPinAttrs(parser, attr, &parser->currSlave->holdingRegs, "holdingRegister");
}

void lcmbsConfParseInputRegsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseListAttrs(parser, attr, &parser->currSlave->inputRegs.start, "inputRegisters");
}

void lcmbsConfParseInputRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseRegPinAttrs(parser, attr, &parser->currSlave->inputRegs, "inputRegister");
}

void lcmbsConfParseInputBitRegAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitRegAttrs(parser, attr, &parser->currSlave->inputRegs, "inputRegister");
}

void lcmbsConfParseInputBitRegPinAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitRegPinAttrs(parser, attr, &parser->currSlave->inputRegs, "inputRegister");
}

void lcmbsConfParseInputsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseListAttrs(parser, attr, &parser->currSlave->inputs.start, "inputs");
}

void lcmbsConfParseInputAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitPinAttrs(parser, attr, &parser->currSlave->inputs, "input");
}

void lcmbsConfParseCoilsAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseListAttrs(parser, attr, &parser->currSlave->coils.start, "coils");
}

void lcmbsConfParseCoilAttrs(LCMBS_CONF_PARSER_T *parser, const char **attr) {
  lcmbsConfParseBitPinAttrs(parser, attr, &parser->currSlave->coils, "coil");
}

