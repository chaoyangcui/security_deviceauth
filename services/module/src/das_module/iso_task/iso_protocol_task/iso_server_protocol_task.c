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

#include "iso_server_protocol_task.h"
#include "common_defs.h"
#include "das_common.h"
#include "hc_log.h"
#include "hc_types.h"
#include "iso_protocol_common.h"
#include "iso_task_common.h"
#include "protocol_common.h"

enum {
    TASK_STATUS_BEGIN = 0,
    TASK_STATUS_CMD_RES_TOKEN,
    TASK_STATUS_GEN_SESSION_KEY,
    TASK_STATUS_FINAL,
};

static CurTaskType GetTaskType()
{
    return TASK_TYPE_ISO_PROTOCOL;
}

static void DestroyProtocolServerTask(struct SymBaseCurTaskT *task)
{
    IsoProtocolServerTask *realTask = (IsoProtocolServerTask *)task;
    if (realTask == NULL) {
        return;
    }
    HcFree(realTask);
}

static int PackageServerStartMessage(const IsoParams *params, const Uint8Buff *selfTokenBuf, CJson *out)
{
    int res;
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
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_ISO_SALT, params->baseParams.randSelf.val,
        params->baseParams.randSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_TOKEN, selfTokenBuf->val, selfTokenBuf->length), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_PEER_AUTH_ID, params->baseParams.authIdSelf.val,
        params->baseParams.authIdSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_PEER_USER_TYPE, params->selfUserType), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_OPERATION_CODE, params->opCode), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(sendToPeer, FIELD_AUTH_FORM, AUTH_FORM_ACCOUNT_UNRELATED), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(sendToPeer, FIELD_PAYLOAD, payload), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(out, FIELD_SEND_TO_PEER, sendToPeer), res);
err:
    FreeJson(payload);
    FreeJson(sendToPeer);
    return res;
}

static int IsoServerStart(SymBaseCurTask *task, IsoParams *params, const CJson *in, CJson *out, int *status)
{
    if (task->taskStatus != TASK_STATUS_BEGIN) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }
    int res;
    uint8_t *selfToken = (uint8_t *)HcMalloc(ISO_TOKEN_LEN, 0);
    if (selfToken == NULL) {
        return HC_ERR_ALLOC_MEMORY;
    }
    Uint8Buff selfTokenBuf = { selfToken, ISO_TOKEN_LEN };
    if (params->opCode == OP_BIND) {
        GOTO_ERR_AND_SET_RET(GetAuthIdPeerFromPayload(in, &(params->baseParams.authIdSelf),
            &(params->baseParams.authIdPeer)), res);
    } else {
        GOTO_ERR_AND_SET_RET(GetAndCheckAuthIdPeer(in, &(params->baseParams.authIdSelf),
            &(params->baseParams.authIdPeer)), res);
        GOTO_ERR_AND_SET_RET(GetAndCheckKeyLenOnServer(in, &(params->keyLen)), res);
    }
    GOTO_ERR_AND_SET_RET(GetByteFromJson(in, FIELD_ISO_SALT, params->baseParams.randPeer.val,
        params->baseParams.randPeer.length), res);
    res = GeneratePsk(in, params);
    if (res != 0) {
        LOGE("Generate psk failed, res:%d", res);
        goto err;
    }
    res = IsoServerGenRandomAndToken(&params->baseParams, &selfTokenBuf);
    if (res != 0) {
        LOGE("IsoServerGenRandomAndToken failed, res:%d", res);
        goto err;
    }

    res = PackageServerStartMessage(params, &selfTokenBuf, out);
    if (res != HC_SUCCESS) {
        LOGE("PackageServerStartMessage failed.");
        goto err;
    }

    task->taskStatus = TASK_STATUS_CMD_RES_TOKEN;
    *status = CONTINUE;
err:
    HcFree(selfToken);
    return res;
}

static int PackDataForCalTokenServer(const IsoParams *params, const Uint8Buff *tokenToPeer, CJson *out)
{
    int res = 0;
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
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_PEER_AUTH_ID, params->baseParams.authIdSelf.val,
        params->baseParams.authIdSelf.length), res);
    GOTO_ERR_AND_SET_RET(AddByteToJson(payload, FIELD_RETURN_CODE_MAC, tokenToPeer->val, tokenToPeer->length), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(payload, FIELD_OPERATION_CODE, params->opCode), res);
    GOTO_ERR_AND_SET_RET(AddIntToJson(sendToPeer, FIELD_AUTH_FORM, AUTH_FORM_ACCOUNT_UNRELATED), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(sendToPeer, FIELD_PAYLOAD, payload), res);
    GOTO_ERR_AND_SET_RET(AddObjToJson(out, FIELD_SEND_TO_PEER, sendToPeer), res);
err:
    FreeJson(payload);
    FreeJson(sendToPeer);
    return res;
}

static int CalTokenAndGenSessionKey(SymBaseCurTask *task, IsoParams *params, const CJson *in, CJson *out, int *status)
{
    int res;
    uint8_t *peerToken = NULL;
    uint8_t *tokenSelf = NULL;

    if (task->taskStatus < TASK_STATUS_CMD_RES_TOKEN) {
        LOGE("Invalid taskStatus:%d", task->taskStatus);
        return HC_ERR_BAD_MESSAGE;
    }

    if (task->taskStatus > TASK_STATUS_CMD_RES_TOKEN) {
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
    GOTO_ERR_AND_SET_RET(GetByteFromJson(in, FIELD_TOKEN, peerToken, ISO_TOKEN_LEN), res);
    Uint8Buff tokenFromPeer = { peerToken, ISO_TOKEN_LEN };

    tokenSelf = (uint8_t *)HcMalloc(ISO_TOKEN_LEN, 0);
    if (tokenSelf == NULL) {
        res = HC_ERR_ALLOC_MEMORY;
        goto err;
    }
    Uint8Buff tokenToPeer = { tokenSelf, ISO_TOKEN_LEN };

    // execute
    res = IsoServerGenSessionKeyAndCalToken(&params->baseParams, &tokenFromPeer, &tokenToPeer);
    if (res != 0) {
        LOGE("IsoServerGenSessionKeyAndCalToken failed, res:%d", res);
        goto err;
    }

    // package message
    res = PackDataForCalTokenServer(params, &tokenToPeer, out);
    if (res != 0) {
        LOGE("PackDataForCalTokenServer failed, res:%d", res);
        goto err;
    }
    task->taskStatus = TASK_STATUS_GEN_SESSION_KEY;
    *status = FINISH;
err:
    HcFree(peerToken);
    HcFree(tokenSelf);
    return res;
}

static int Process(struct SymBaseCurTaskT *task, IsoParams *params, const CJson *in, CJson *out, int *status)
{
    int res;
    if ((task == NULL) || (in == NULL) || (out == NULL) || (status == NULL) || (params == NULL)) {
        res = HC_ERR_INVALID_PARAMS;
        goto out;
    }
    uint32_t step = ProtocolMessageIn(in);
    if (step == INVALID_MESSAGE) {
        res = HC_ERR_BAD_MESSAGE;
        goto out;
    }
    switch (step) {
        case STEP_ONE:
            res = IsoServerStart(task, params, in, out, status);
            break;
        case STEP_TWO:
            res = CalTokenAndGenSessionKey(task, params, in, out, status);
            break;
        default:
            res = HC_ERR_BAD_MESSAGE;
            break;
    }
out:
    if (res == 0) {
        res = ServerProtocolMessageOut(out, params->opCode, step);
    }

    return res;
}

SymBaseCurTask *CreateProtocolServerTask()
{
    IsoProtocolServerTask *task = (IsoProtocolServerTask *)HcMalloc(sizeof(IsoProtocolServerTask), 0);
    if (task == NULL) {
        return NULL;
    }
    task->taskBase.destroyTask = DestroyProtocolServerTask;
    task->taskBase.process = Process;
    task->taskBase.getCurTaskType = GetTaskType;
    return (SymBaseCurTask *)task;
}