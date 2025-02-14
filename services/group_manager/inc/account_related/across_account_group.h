/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ACROSS_ACCOUNT_GROUP_H
#define ACROSS_ACCOUNT_GROUP_H

#include "base_group.h"

typedef struct {
    BaseGroup base;
    int32_t (*syncGroup)(const char *appId, int64_t userId, const char *deviceId, const CJson *sharedUserIdList);
    int32_t (*generateGroupId)(int64_t userId, int64_t sharedUserId, char **returnGroupId);
} AcrossAccountGroup;

bool IsAcrossAccountGroupSupported();
BaseGroup *GetAcrossAccountGroupInstance();

#endif
