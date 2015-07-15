#ifndef _LCMBS_UTIL_H
#define _LCMBS_UTIL_H

#include <stdint.h>
#include <stddef.h>

extern const char *compName;

#define LCMBS_VECT_BLKSIZE 32

typedef struct {
  size_t typeSize;
  size_t size;
  size_t count;
  size_t pos;
  void *data;
} LCMBS_VECT_T;

void lcmbsVectInit(LCMBS_VECT_T *vect, size_t typeSize);

void *lcmbsVectEnsureSize(LCMBS_VECT_T *vect, size_t count);

void lcmbsVectFree(LCMBS_VECT_T *vect);
void lcmbsVectClear(LCMBS_VECT_T *vect);

void *lcmbsVectGet(LCMBS_VECT_T *vect, size_t idx);
void *lcmbsVectPut(LCMBS_VECT_T *vect);
void *lcmbsVectPull(LCMBS_VECT_T *vect);

void *lcmbsVectPutByte(LCMBS_VECT_T *vect, uint8_t val);
void *lcmbsVectPutWord(LCMBS_VECT_T *vect, uint16_t val);
void *lcmbsVectPutDByte(LCMBS_VECT_T *vect, uint32_t val);

void *lcmbsVectPullByte(LCMBS_VECT_T *vect, uint8_t *val);
void *lcmbsVectPullWord(LCMBS_VECT_T *vect, uint16_t *val);
void *lcmbsVectPullDByte(LCMBS_VECT_T *vect, uint32_t *val);

#endif

