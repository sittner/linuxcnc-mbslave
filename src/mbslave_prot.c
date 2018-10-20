#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include <limits.h>

#include "mbslave_prot.h"

typedef union {
  uint32_t u;
  int32_t s;
  float f;
  uint16_t w[2];
  uint8_t b[4];
} MODBUS_VAL_T;

static void writeRegBitpins(LCMBS_VECT_T *bitpins, uint16_t val) {
  int i;
  for (i = 0; i < bitpins->count; i++) {
    LCMBS_CONF_REG_BIT_PIN_T *pin = lcmbsVectGet(bitpins, i);
    **pin->pin = (val & (1 << pin->bit)) ? 1 : 0;
  }
}

static uint16_t readRegBitpins(LCMBS_VECT_T *bitpins) {
  int i;
  uint16_t val = 0;
  for (i = 0; i < bitpins->count; i++) {
    LCMBS_CONF_REG_BIT_PIN_T *pin = lcmbsVectGet(bitpins, i);
    if (**pin->pin) {
      val |= (1 << pin->bit);
    }
  }
  return val;
}

int lcmbsProtReadBits(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_BITS_T *bits) {
  uint16_t start, count;

  // get parameters
  if (!lcmbsVectPullWord(in, &start) || !lcmbsVectPullWord(in, &count)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  start = ntohs(start);
  count = ntohs(count);

  // check valid register range
  if (start < bits->start || (start + count) > (bits->start + bits->pins.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // calculate number of bytes
  int bytes = count >> 3;
  if (count & 7) {
    bytes++;
  }
  if (bytes > 255) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // prepare header
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutByte(out, bytes)) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  // iterate single bits
  int i = 0;
  uint8_t val = 0;
  start -= bits->start;
  while(i < count) {
    LCMBS_CONF_BIT_PIN_T *pin = lcmbsVectGet(&bits->pins, start + i);
    if (**pin->pin) {
      val |= 1 << (i & 7);
    }
    i++;
    if ((i & 7) == 0 || i == count) {
      if (!lcmbsVectPutByte(out, val)) {
        return MB_ERR_SLAVE_DEVICE_FAILURE;
      }   
      val = 0;
    }
  }

  return MB_ERR_OK;
}

int lcmbsProtForceBit(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_BITS_T *bits) {
  uint16_t addr, val;

  // get parameters
  if (!lcmbsVectPullWord(in, &addr) || !lcmbsVectPullWord(in, &val)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  addr = ntohs(addr);
  val = ntohs(val);

  // check valid register range
  if (addr < bits->start || addr >= (bits->start + bits->pins.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // check valid data
  if (val != 0x0000 && val != 0xff00) {
    return MB_ERR_ILLEGAL_DATA_VALUE;
  }
  
  // set bit
  LCMBS_CONF_BIT_PIN_T *pin = lcmbsVectGet(&bits->pins, addr - bits->start);
  **pin->pin = val ? 1 : 0;

  // setup response
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutWord(out, htons(addr)) ||
    !lcmbsVectPutWord(out, htons(val))) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  return MB_ERR_OK;
}

int lcmbsProtForceBits(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_BITS_T *bits) {
  uint16_t start, count;
  uint8_t bc;

  // get parameters
  if (!lcmbsVectPullWord(in, &start) || !lcmbsVectPullWord(in, &count) || !lcmbsVectPullByte(in, &bc)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  start = ntohs(start);
  count = ntohs(count);

  // check valid register range
  if (start < bits->start || (start + count) > (bits->start + bits->pins.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // check number of bytes
  int bytes = count >> 3;
  if (count & 7) {
    bytes++;
  }
  if (bytes != bc) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }
  if (bytes != (in->count - in->pos)) {
    return MB_ERR_ILLEGAL_DATA_VALUE;
  }

  // iterate single bits
  int i = 0;
  uint8_t val = 0;
  start -= bits->start;
  while(i < count) {
    LCMBS_CONF_BIT_PIN_T *pin = lcmbsVectGet(&bits->pins, start + i);
    if ((i & 7) == 0) {
      if (!lcmbsVectPullByte(in, &val)) {
        return MB_ERR_ILLEGAL_DATA_VALUE;
      }
    }
    **pin->pin = (val & (1 << (i & 7))) ? 1 : 0;
    i++;
  }

  // setup response
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutWord(out, htons(start)) ||
    !lcmbsVectPutWord(out, htons(count))) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  return MB_ERR_OK;
}

int lcmbsProtReadRegs(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_REGS_T *regs) {
  uint16_t start, count;
  LCMBS_CONF_REG_T *reg;

  // get parameters
  if (!lcmbsVectPullWord(in, &start) || !lcmbsVectPullWord(in, &count)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  start = ntohs(start);
  count = ntohs(count);

  // check valid register range
  if (start < regs->start || (start + count) > (regs->start + regs->regs.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // calculate number of bytes
  int bytes = count << 1;
  if (bytes > 255) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // prepare header
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutByte(out, bytes)) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  // check for zero size request
  if (count == 0) {
    return MB_ERR_OK;
  }

  // check aligned data boundaries
  start -= regs->start;
  reg = lcmbsVectGet(&regs->regs, start);
  if (reg->index > 0) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }
  reg = lcmbsVectGet(&regs->regs, start + count - 1);
  if (reg->pin != NULL && reg->index < (reg->pin->regCount - 1)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  int i, pad;
  MODBUS_VAL_T pinval;
  pinval.u = 0;
  for (i=0; i<count; i++) {
    // get register and pin
    reg = lcmbsVectGet(&regs->regs, start + i);

    // normal register pins
    LCMBS_CONF_REG_PIN_T *pin = reg->pin;
    if (pin != NULL) {
      pad = 2 - pin->regCount;

      // read pin (triggerd by first register access)
      if (reg->index == 0) {
        // read value
        switch (pin->halType) {
          case HAL_U32:
            pinval.u = **pin->pin.u;
            break;
          case HAL_S32:
            pinval.s = **pin->pin.s;
            break;
          case HAL_FLOAT:
            pinval.f = **pin->pin.f;
            break;
          default:
            pinval.u = 0;
        }

        // limit single word values
        switch(pin->type) {
          case LCMBS_PINTYPE_U16:
            // limit range
            if (pinval.u > USHRT_MAX) pinval.u = USHRT_MAX;
            break;
          case LCMBS_PINTYPE_S16:
            // limit range
            if (pinval.s < SHRT_MIN) pinval.s = SHRT_MIN;
            if (pinval.s > SHRT_MAX) pinval.s = SHRT_MAX;
            break;
        }

        // convert to network byte order
        pinval.u = htonl(pinval.u);

        // reorder words on request
        if (pin->flags & LCMBS_PINFLAG_WORDSWAP) {
          uint16_t tmp = pinval.w[0];
          pinval.w[0] = pinval.w[1];
          pinval.w[1] = tmp;
        }
      }

      // get register value;
      uint16_t val = pinval.w[pad + reg->index];

      // reorder bytes on request
      if (pin->flags & LCMBS_PINFLAG_BYTESWAP) {
        val = bswap_16(val);
      }

      if (!lcmbsVectPutWord(out, val)) {
        return MB_ERR_SLAVE_DEVICE_FAILURE;
      }

      continue;
    }

    // handle bit mapped register pins
    LCMBS_VECT_T *bitpins = reg->bitpins;
    if (bitpins != NULL) {
      if (!lcmbsVectPutWord(out, htons(readRegBitpins(bitpins)))) {
        return MB_ERR_SLAVE_DEVICE_FAILURE;
      }

      continue;
    }
  }

  return MB_ERR_OK;
}

int lcmbsProtPresetReg(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_REGS_T *regs) {
  uint16_t addr, val, pinval;

  // get parameters
  if (!lcmbsVectPullWord(in, &addr) || !lcmbsVectPullWord(in, &val)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  addr = ntohs(addr);
  val = ntohs(addr);

  // check valid register range
  if (addr < regs->start || addr >= (regs->start + regs->regs.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // get register
  LCMBS_CONF_REG_T *reg = lcmbsVectGet(&regs->regs, addr - regs->start);

  // handle normal register pins
  LCMBS_CONF_REG_PIN_T *pin = reg->pin;
  if (pin != NULL) {
    // reorder bytes on request
    if (pin->flags & LCMBS_PINFLAG_BYTESWAP) {
      pinval = bswap_16(val);
    } else {
      pinval = val;
    }

    // set register
    switch(pin->type) {
      case LCMBS_PINTYPE_U16:
        **pin->pin.u = (hal_u32_t) pinval;
        break;
      case LCMBS_PINTYPE_S16:
        **pin->pin.s = (hal_s32_t) ((int16_t) pinval);
        break;
      default:
        // only single word pins are allowd here
        return MB_ERR_ILLEGAL_DATA_ADDRESS;
    }
  }

  // handle bit mapped register pins
  LCMBS_VECT_T *bitpins = reg->bitpins;
  if (bitpins != NULL) {
    writeRegBitpins(bitpins, val);
  }

  // setup response
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutWord(out, htons(addr)) ||
    !lcmbsVectPutWord(out, htons(val))) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  return MB_ERR_OK;
}

int lcmbsProtPresetRegs(uint8_t sid, uint8_t fnk, LCMBS_VECT_T *in, LCMBS_VECT_T *out, LCMBS_CONF_REGS_T *regs) {
  uint16_t start, count;
  uint8_t bc;
  LCMBS_CONF_REG_T *reg;

  // get parameters
  if (!lcmbsVectPullWord(in, &start) || !lcmbsVectPullWord(in, &count) || !lcmbsVectPullByte(in, &bc)) {
    return MB_ERR_INVALID_FUNCTION;
  }

  // adjust byte order
  start = ntohs(start);
  count = ntohs(count);

  // check valid register range
  if (start < regs->start || (start + count) > (regs->start + regs->regs.count)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  // check number of bytes
  int bytes = count << 1;
  if (bytes != bc) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }
  if (bytes != (in->count - in->pos)) {
    return MB_ERR_ILLEGAL_DATA_VALUE;
  }

  // setup response
  if (
    !lcmbsVectPutByte(out, sid) ||
    !lcmbsVectPutByte(out, fnk) ||
    !lcmbsVectPutWord(out, htons(start)) ||
    !lcmbsVectPutWord(out, htons(count))) {
    return MB_ERR_SLAVE_DEVICE_FAILURE;
  }

  // check for zero size request
  if (count == 0) {
    return MB_ERR_OK;
  }

  // check aligned data boundaries
  start -= regs->start;
  reg = lcmbsVectGet(&regs->regs, start);
  if (reg->index > 0) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }
  reg = lcmbsVectGet(&regs->regs, start + count - 1);
  if (reg->pin != NULL && reg->index < (reg->pin->regCount - 1)) {
    return MB_ERR_ILLEGAL_DATA_ADDRESS;
  }

  int i, pad;
  MODBUS_VAL_T pinval;
  pinval.u = 0;
  for (i=0; i<count; i++) {
    // get register
    reg = lcmbsVectGet(&regs->regs, start + i);

    // handle normal register pins
    LCMBS_CONF_REG_PIN_T *pin = reg->pin;
    if (pin != NULL) {
      pad = 2 - pin->regCount;

      // initialize value (triggerd by first register access)
      if (reg->index == 0) {
        pinval.u = 0;
      }

      // read register value
      uint16_t val;
      if (!lcmbsVectPullWord(in, &val)) {
        return MB_ERR_SLAVE_DEVICE_FAILURE;
      }

      // reorder bytes on request
      if (pin->flags & LCMBS_PINFLAG_BYTESWAP) {
        val = bswap_16(val);
      }

      // set register value;
      pinval.w[pad + reg->index] = val;

      // write pin (triggerd by last register access)
      if (reg->index == (pin->regCount - 1)) {
        // reorder words on request
        if (pin->flags & LCMBS_PINFLAG_WORDSWAP) {
          uint16_t tmp = pinval.w[0];
          pinval.w[0] = pinval.w[1];
          pinval.w[1] = tmp;
        }

        // convert to host byte order
        pinval.u = ntohl(pinval.u);

        // handle single word values
        switch(pin->type) {
          case LCMBS_PINTYPE_U16:
            pinval.u = (uint16_t) pinval.u;
            break;
          case LCMBS_PINTYPE_S16:
            pinval.s = (int16_t) pinval.s;
            break;
        }

        // read value
        switch (pin->halType) {
          case HAL_U32:
            **pin->pin.u = pinval.u;
            break;
          case HAL_S32:
            **pin->pin.s = pinval.s;
            break;
          case HAL_FLOAT:
            **pin->pin.f = pinval.f;
            break;
          default:
            break;
        }
      }

      continue;
    }

    // handle bit mapped register pins
    LCMBS_VECT_T *bitpins = reg->bitpins;
    if (bitpins != NULL) {
      // read register value
      uint16_t val;
      if (!lcmbsVectPullWord(in, &val)) {
        return MB_ERR_SLAVE_DEVICE_FAILURE;
      }

      writeRegBitpins(bitpins, val);
      continue;
    }
  }

  return MB_ERR_OK;
}

int lcmbsProtProc(LCMBS_CONF_SLAVE_T *slave, LCMBS_VECT_T *in, LCMBS_VECT_T *out) {
  uint8_t sid, fnk;

  // get slave and function
  if (!lcmbsVectPullByte(in, &sid) || !lcmbsVectPullByte(in, &fnk)) {
    return 0;
  }

  // reset output buffer
  uint8_t err = MB_ERR_INVALID_FUNCTION;
  lcmbsVectClear(out);

  // process function
  switch (fnk) {
    case MB_FNK_READ_COIL_STATUS:
      err = lcmbsProtReadBits(sid, fnk, in, out, &slave->coils);
      break;

    case MB_FNK_READ_INPUT_STATUS:
      err = lcmbsProtReadBits(sid, fnk, in, out, &slave->inputs);
      break;

    case MB_FNK_FORCE_SINGLE_COIL:
      err = lcmbsProtForceBit(sid, fnk, in, out, &slave->coils);
      break;

    case MB_FNK_FORCE_MULTI_COIL:
      err = lcmbsProtForceBits(sid, fnk, in, out, &slave->coils);
      break;

    case MB_FNK_READ_HOLDING_REG:
      err = lcmbsProtReadRegs(sid, fnk, in, out, &slave->holdingRegs);
      break;

    case MB_FNK_READ_INPUT_REG:
      err = lcmbsProtReadRegs(sid, fnk, in, out, &slave->inputRegs);
      break;

    case MB_FNK_PRESET_SINGLE_REG:
      err = lcmbsProtPresetReg(sid, fnk, in, out, &slave->holdingRegs);
      break;

    case MB_FNK_PRESET_MULTI_REG:
      err = lcmbsProtPresetRegs(sid, fnk, in, out, &slave->holdingRegs);
      break;

    default:
      err = MB_ERR_INVALID_FUNCTION;
  }

  // handle error
  if (err != MB_ERR_OK) {
    lcmbsVectClear(out);
    if (
      !lcmbsVectPutByte(out, sid) ||
      !lcmbsVectPutByte(out, fnk | 0x80) ||
      !lcmbsVectPutByte(out, err)) {
      return 0;
    }
  }

  return out->count;
}

