#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mbslave_util.h"

void lcmbsVectInit(LCMBS_VECT_T *vect, size_t typeSize) {
  memset(vect, 0, sizeof(LCMBS_VECT_T));
  vect->typeSize = typeSize;
}

void *lcmbsVectEnsureSize(LCMBS_VECT_T *vect, size_t count) {
  if (vect->size >= count) {
   return vect->data;
  }

  vect->size = (((count - 1) / LCMBS_VECT_BLKSIZE) + 1) * LCMBS_VECT_BLKSIZE;
  vect->data = realloc(vect->data, vect->typeSize * vect->size);
  return vect->data;
}

void lcmbsVectFree(LCMBS_VECT_T *vect) {
  lcmbsVectClear(vect);
  free(vect->data);
  vect->size = 0;
}

void lcmbsVectClear(LCMBS_VECT_T *vect) {
  vect->count = 0;
  vect->pos = 0;
}

void *lcmbsVectGet(LCMBS_VECT_T *vect, size_t idx) {
  if (idx >= vect->count) {
    return NULL;
  }
  return vect->data + (vect->typeSize * idx);
}

void *lcmbsVectPut(LCMBS_VECT_T *vect) {
  if (vect->count >= vect->size) {
    vect->size += LCMBS_VECT_BLKSIZE;
    vect->data = realloc(vect->data, vect->typeSize * vect->size);
    if (!vect->data) {
      return NULL;
    }
  }

  return lcmbsVectGet(vect, (vect->count++));
}

void *lcmbsVectPull(LCMBS_VECT_T *vect) {
  if (vect->pos >= vect->count) {
    return NULL;
  }

  return lcmbsVectGet(vect, (vect->pos++));
}

void *lcmbsVectPutByte(LCMBS_VECT_T *vect, uint8_t val) {
  if (vect->typeSize != 1 || !lcmbsVectEnsureSize(vect, vect->count + sizeof(uint8_t))) {
    return NULL;
  }

  uint8_t *p = (uint8_t *)(vect->data + vect->count);
  *p = val;
  vect->count += sizeof(uint8_t);
  return p;
}

void *lcmbsVectPutWord(LCMBS_VECT_T *vect, uint16_t val) {
  if (vect->typeSize != 1 || !lcmbsVectEnsureSize(vect, vect->count + sizeof(uint16_t))) {
    return NULL;
  }

  uint16_t *p = (uint16_t *)(vect->data + vect->count);
  *p = val;
  vect->count += sizeof(uint16_t);
  return p;
}

void *lcmbsVectPutDByte(LCMBS_VECT_T *vect, uint32_t val) {
  if (vect->typeSize != 1 || !lcmbsVectEnsureSize(vect, vect->count + sizeof(uint32_t))) {
    return NULL;
  }

  uint32_t *p = (uint32_t *)(vect->data + vect->count);
  *p = val;
  vect->count += sizeof(uint32_t);
  return p;
}

void *lcmbsVectPullByte(LCMBS_VECT_T *vect, uint8_t *val) {
  if (vect->typeSize != 1 || (vect->count - vect->pos) < sizeof(uint8_t)) {
    return NULL;
  }

  uint8_t *p = (uint8_t *)(vect->data + vect->pos);
  *val = *p;
  vect->pos += sizeof(uint8_t);
  return p;
}

void *lcmbsVectPullWord(LCMBS_VECT_T *vect, uint16_t *val) {
  if (vect->typeSize != 1 || (vect->count - vect->pos) < sizeof(uint16_t)) {
    return NULL;
  }

  uint16_t *p = (uint16_t *)(vect->data + vect->pos);
  *val = *p;
  vect->pos += sizeof(uint16_t);
  return p;
}

void *lcmbsVectPullDByte(LCMBS_VECT_T *vect, uint32_t *val) {
  if (vect->typeSize != 1 || (vect->count - vect->pos) < sizeof(uint32_t)) {
    return NULL;
  }

  uint32_t *p = (uint32_t *)(vect->data + vect->pos);
  *val = *p;
  vect->pos += sizeof(uint32_t);
  return p;
}

