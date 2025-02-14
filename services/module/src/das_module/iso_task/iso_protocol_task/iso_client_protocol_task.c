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

#include "iso_client_protocol_task.h"
#include "common_defs.h"
#include "das_common.h"
#include "hc_log.h"
#include "hc_types.h"
#include "iso_protocol_common.h"
#include "iso_task_common.h"

enum {
    TASK_STATUS_BEGIN = 0,
    TASK_STATUS_SERVER_RES_TOKEN,
    TASK_STATUS_GEN_SESSION_KEY,
    TASK_STATUS_FINAL,
};

static CurTaskType GetTaskType()
{
    return TASK_TYPE_ISO_PROTOCOL;
}

static void DestroyProtocolClientTask(struct SymBaseCurTaskT *task)
{
    IsoProtocolClientTask *realTask = (IsoProtocolClientTask *)task;
    if (realTask == NULL) {
        return;
    }
    HcFree(realTask);
}

static int IsoClientStartPackData(CJson *out, const IsoParams *params)
{
    int res = HC_SUCCESS;
    CJson *payload = NULL;
    CJson *sendToPeer = NULL;
    payload = CreateJson();
    if (payload == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    sendToPeer = CreateJson();
    if (sendToPeer == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    GOTO_ERR_AND_SET_RET(AddIntToJson(sendToPeer, FIELD_AUTH_FORM, AUTH_FORM_ACCOUNT_UNRELATED), res);

    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_ISO_SALT, params->baseParams.randSelf.val,
        params->baseParams.randSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_PEER_AUTH_ID, params->baseParams.authIdSelf.val,
        params->baseParams.authIdSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_OPERATION_CODE, params->opCode), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_SEED, params->seed.val, params->seed.length), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_PEER_USER_TYPE, params->selfUserType), res);
    if (params->opCode == AUTHENTICATE || params->opCode == OP_UNBIND) {
        GOTO_ERR_AND_SET_RET(AddStringToJson(payload, FIELD_PKG_NAME, params->packageName), res);
        GOTO_ERR_AND_SET_RET(AddStringToJson(payload, FIELD_SERVICE_TYPE, params->serviceType), res);
        GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_KEY_LENGTH, params->keyLen), res);
    }
    GOTO_ERR_AND_SET_RET(AddObjToJson(sendToPeer, FIELD_PAYLOAD, payload), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(out, FIELD_SEND_TO_PEER, sendToPeer), res);
err:
    FreeJson(payload);
    FreeJson(sendToPeer);
    return res;
}

static int IsoClientStart(SymBaseCurTask *task, IsoParams *params, const CJson *in, CJson *out, int *status)
{
    if (task->taskStatus != TASK_STATUS_BEGIN) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }
    int res;
    res = IsoClientGenRandom(&params->baseParams);
    if (res != 0) {
        return res;
    }
    res = GenerateSeed(params);
    if (res != 0) {
        return res;
    }
    res = IsoClientStartPackData(out, params);
    if (res != 0) {
        LOGE("IsoClientStartPackData failed, res:%d", res);
        return res;
    }
    task->taskStatus = TASK_STATUS_SERVER_RES_TOKEN;
    *status = CONTINUE;
    return res;
}

static int PackDataForCalToken(const IsoParams *params, const Uint8Buff *selfTokenBuf, CJson *out)
{
    int res = HC_SUCCESS;
    CJson *payload = NULL;
    CJson *sendToPeer = NULL;

    sendToPeer = CreateJson();
    if (sendToPeer == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    payload = CreateJson();
    if (payload == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }

    GOTO_ERR_AND_SET_RET(AddIntToJson(sendToPeer, FIELD_AUTH_FORM, AUTH_FORM_ACCOUNT_UNRELATED), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_PEER_AUTH_ID, params->baseParams.authIdSelf.val,
        params->baseParams.authIdSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_TOKEN, selfTokenBuf->val, selfTokenBuf->length), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(sendToPeer, FIELD_PAYLOAD, payload), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(out, FIELD_SEND_TO_PEER, sendToPeer), res);
err:
    FreeJson(payload);
    FreeJson(sendToPeer);
    return res;
}

static int ParseServerStartMessage(IsoParams *params, const CJson *in, uint8_t *peerToken)
{
    if (params->opCode == OP_BIND) {
        RETURN_IF_ERR(GetAuthIdPeerFromPayload(in, &(params->baseParams.authIdSelf), &(params->baseParams.authIdPeer)));
    } else {
        RETURN_IF_ERR(GetAndCheckAuthIdPeer(in, &(params->baseParams.authIdSelf), &(params->baseParams.authIdPeer)));
    }
    RETURN_IF_ERR(GetByteFromJson(in, FIELD_ISO_SALT, params->baseParams.randPeer.val,
        params->baseParams.randPeer.length));
    RETURN_IF_ERR(GetByteFromJson(in, FIELD_TOKEN, peerToken, ISO_TOKEN_LEN));
    RETURN_IF_ERR(GetIntFromJson(in, FIELD_PEER_USER_TYPE, &(params->peerUserType)));
    return HC_SUCCESS;
}

static int CalculateTokenClient(SymBaseCurTask *task, IsoParams *params, const CJson *in, CJson *out, int *status)
{
    int res;
    uint8_t *peerToken = NULL;
    uint8_t *selfToken = NULL;
    if (task->taskStatus < TASK_STATUS_SERVER_RES_TOKEN) {
        LOGE("Invalid taskStatus:%d", task->taskStatus);
        return HC_ERR_BAD_MESSAGE;
    }

    if (task->taskStatus > TASK_STATUS_SERVER_RES_TOKEN) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }

    // parse message
    peerToken = (uint8_t *)HcMalloc(ISO_TOKEN_LEN, 0);
    if (peerToken == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    Uint8Buff peerTokenBuf = { peerToken, ISO_TOKEN_LEN };

    selfToken = (uint8_t *)HcMalloc(ISO_TOKEN_LEN, 0);
    if (selfToken == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    Uint8Buff selfTokenBuf = { selfToken, ISO_TOKEN_LEN };

    res = ParseServerStartMessage(params, in, peerToken);
    if (res != 0) {
        LOGE("ParseServerStartMessage failed, res:%d", res);
        goto err;
    }
    res = GeneratePsk(in, params);
    if (res != 0) {
        LOGE("GeneratePsk failed, res:%d", res);
        goto err;
    }

    // execute
    res = IsoClientCheckAndGenToken(&params->baseParams, &peerTokenBuf, &selfTokenBuf);
    if (res != HC_SUCCESS) {
        LOGE("IsoClientCheckAndGenToken failed, res:%d", res);
        goto err;
    }

    // package message
    res = PackDataForCalToken(params, &selfTokenBuf, out);
    if (res != 0) {
        LOGE("PackDataForCalToken failed, res: %d", res);
        goto err;
    }
    task->taskStatus = TASK_STATUS_GEN_SESSION_KEY;
    *status = CONTINUE;
err:
    HcFree(peerToken);
    HcFree(selfToken);
    return res;
}

static int GenerateSessionKey(SymBaseCurTask *task, IsoParams *params, const CJson *in, int *status)
{
    if (task->taskStatus < TASK_STATUS_GEN_SESSION_KEY) {
        LOGE("Invalid taskStatus:%d", task->taskStatus);
        return HC_ERR_BAD_MESSAGE;
    }

    if (task->taskStatus > TASK_STATUS_GEN_SESSION_KEY) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }

    uint8_t *hmac = (uint8_t *)HcMalloc(HMAC_LEN, 0);
    if (hmac == NULL) {
        return HC_ERR_ALLOC_MEMORY;
    }
    int res = GetByteFromJson(in, FIELD_RETURN_CODE_MAC, hmac, HMAC_LEN);
    if (res != 0) {
        LOGE("get mac failed, res:%d", res);
        goto err;
    }

    // execute
    res = IsoClientGenSessionKey(&params->baseParams, 0, hmac, HMAC_LEN);
    if (res != 0) {
        LOGE("IsoClientGenSessionKey failed, res:%d", res);
        goto err;
    }

    task->taskStatus = TASK_STATUS_FINAL;
    *status = FINISH;
err:
    HcFree(hmac);
    return res;
}

static int Process(struct SymBaseCurTaskT *task, IsoParams *params, const CJson *in, CJson *out,
    int *status)
{
    int res;
    uint32_t step = ProtocolMessageIn(in);
    if (step == INVALID_MESSAGE) {
        res = IsoClientStart(task, params, in, out, status);
        step = STEP_ONE;
        goto out_func;
    }

    step = step + 1; /* when receive peer message code, need to do next step */
    switch (step) {
        case STEP_TWO:
            res = CalculateTokenClient(task, params, in, out, status);
            break;
        case STEP_THREE:
            res = GenerateSessionKey(task, params, in, status);
            break;
        default:
            res = HC_ERR_BAD_MESSAGE;
            break;
    }
out_func:
    if (res != HC_SUCCESS) {
        return res;
    }
    if (step != STEP_THREE) {
        res = ClientProtocolMessageOut(out, params->opCode, step);
        if (res != HC_SUCCESS) {
            LOGE("ClientProtocolMessageOut failed, res:%d", res);
        }
    }

    return res;
}

SymBaseCurTask *CreateProtocolClientTask()
{
    IsoProtocolClientTask *task = (IsoProtocolClientTask *)HcMalloc(sizeof(IsoProtocolClientTask), 0);
    if (task == NULL) {
        return NULL;
    }
    task->taskBase.destroyTask = DestroyProtocolClientTask;
    task->taskBase.process = Process;
    task->taskBase.getCurTaskType = GetTaskType;
    return (SymBaseCurTask *)task;
}