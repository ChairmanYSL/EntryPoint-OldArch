#ifndef PAYPASS_API_H
#define PAYPASS_API_H

#include "emv_type.h"

int32_t paypass_init();
int32_t paypass_preprocess(bool bIsOnline);
int32_t paypass_process();

#endif
