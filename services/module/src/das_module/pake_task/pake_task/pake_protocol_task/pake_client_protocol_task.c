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

#include "pake_client_protocol_task.h"
#include "das_common.h"
#include "hc_log.h"
#include "hc_types.h"
#include "pake_message_util.h"
#include "pake_protocol_common.h"
#include "pake_task_common.h"

enum {
    TASK_STATUS_CLIENT_PAKE_BEGIN = 0,
    TASK_STATUS_CLIENT_PAKE_REQUEST,
    TASK_STATUS_CLIENT_PAKE_CONFIRM,
    TASK_STATUS_CLIENT_PAKE_VERIFY_CONFIRM,
};

static CurTaskType GetTaskType()
{
    return TASK_TYPE_PAKE_PROTOCOL;
}

static int PakeRequest(AsyBaseCurTask *task, PakeParams *params, const CJson *in, CJson *out, int *status)
{
    int res;
    if (task->taskStatus != TASK_STATUS_CLIENT_PAKE_BEGIN) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }

    res = ParseStartJsonParams(params, in);
    if (res != HC_SUCCESS) {
        LOGE("ParseStartJsonParams failed, res: %d.", res);
        return res;
    }

    // execute
    res = ClientRequestPakeProtocol(&params->baseParams);
    if (res != HC_SUCCESS) {
        LOGE("ClientRequestPakeProtocol failed, res: %d.", res);
        return res;
    }

    res = ConstructOutJson(params, out);
    if (res != HC_SUCCESS) {
        LOGE("ConstructOutJson failed, res: %d.", res);
        return res;
    }
    CJson *payload = GetObjFromJson(out, FIELD_PAYLOAD);
    if (payload == NULL) {
        LOGE("Get payload from json failed.");
        return HC_ERR_JSON_GET;
    }
    res = PackagePakeRequestData(params, payload);
    if (res != HC_SUCCESS) {
        LOGE("PackagePakeRequestData failed, res: %d.", res);
        return res;
    }
    // package differentiated data
    if (params->opCode == AUTHENTICATE || params->opCode == OP_UNBIND) {
        res = AddByteToJson(payload, FIELD_PEER_AUTH_ID, params->baseParams.idSelf.val,
            params->baseParams.idSelf.length);
        if (res != HC_SUCCESS) {
            LOGE("Add idSelf failed, res: %d.", res);
            return res;
        }
        res = AddIntToJson(payload, FIELD_KEY_LENGTH, params->returnKey.length);
        if (res != HC_SUCCESS) {
            LOGE("Add keyLength failed, res: %d.", res);
            return res;
        }
    }

    task->taskStatus = TASK_STATUS_CLIENT_PAKE_REQUEST;
    *status = CONTINUE;
    return res;
}

static int ParseMsgForClientConfirm(PakeParams *params, const CJson *in)
{
    int res = ParsePakeResponseMessage(params, in);
    if (res != HC_SUCCESS) {
        LOGE("ParsePakeResponseMessage failed, res: %d.", res);
        return res;
    }
    // parse differentiated data
    res = GetByteFromJson(in, FIELD_CHALLENGE, params->baseParams.challengePeer.val,
        params->baseParams.challengePeer.length);
    if (res != HC_SUCCESS) {
        LOGE("Get challengePeer failed, res: %d.", res);
        return res;
    }
    if (params->opCode == AUTHENTICATE || params->opCode == OP_UNBIND) {
        res = GetAndCheckAuthIdPeer(in, &(params->baseParams.idSelf), &(params->baseParams.idPeer));
        if (res != HC_SUCCESS) {
            LOGE("GetAndCheckAuthIdPeer failed, res: %d.", res);
            return res;
        }
    }
    return res;
}

static int PackageMsgForClientConfirm(PakeParams *params, CJson *out)
{
    int res = ConstructOutJson(params, out);
    if (res != HC_SUCCESS) {
        LOGE("ConstructOutJson failed, res: %d.", res);
        return res;
    }
    CJson *payload = GetObjFromJson(out, FIELD_PAYLOAD);
    if (payload == NULL) {
        LOGE("Get payload from json failed.");
        return HC_ERR_JSON_GET;
    }
    res = PackagePakeClientConfirmData(params, payload);
    if (res != HC_SUCCESS) {
        LOGE("PackagePakeClientConfirmData failed, res: %d.", res);
        return res;
    }
    res = AddByteToJson(payload, FIELD_CHALLENGE, params->baseParams.challengeSelf.val,
        params->baseParams.challengeSelf.length);
    if (res != HC_SUCCESS) {
        LOGE("Add challengeSelf failed, res: %d.", res);
        return res;
    }
    return res;
}

static int PakeClientConfirm(AsyBaseCurTask *task, PakeParams *params, const CJson *in, CJson *out, int *status)
{
    int res;
    if (task->taskStatus < TASK_STATUS_CLIENT_PAKE_REQUEST) {
        return HC_ERR_BAD_MESSAGE;
    }
    if (task->taskStatus > TASK_STATUS_CLIENT_PAKE_REQUEST) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }

    res = ParseMsgForClientConfirm(params, in);
    if (res != HC_SUCCESS) {
        LOGE("ParseMsgForClientConfirm failed, res: %d.", res);
        return res;
    }
    if (((uint32_t)params->baseParams.supportedPakeAlg & PSK_SPEKE) &&
        (params->opCode == AUTHENTICATE || params->opCode == OP_UNBIND)) {
        res = FillPskWithDerivedKey(params);
        if (res != HC_SUCCESS) {
            LOGE("FillPskWithDerivedKey failed.");
            return res;
        }
    }

    // execute
    res = ClientConfirmPakeProtocol(&(params->baseParams));
    if (res != HC_SUCCESS) {
        LOGE("ClientConfirmPakeProtocol failed, res:%d", res);
        return res;
    }

    res = PackageMsgForClientConfirm(params, out);
    if (res != HC_SUCCESS) {
        LOGE("PackageMsgForClientConfirm failed, res: %d.", res);
        return res;
    }

    task->taskStatus = TASK_STATUS_CLIENT_PAKE_CONFIRM;
    *status = CONTINUE;
    return res;
}

static int PakeClientVerifyConfirm(AsyBaseCurTask *task, PakeParams *params, const CJson *in, CJson *out, int *status)
{
    int res;

    if (task->taskStatus < TASK_STATUS_CLIENT_PAKE_CONFIRM) {
        return HC_ERR_BAD_MESSAGE;
    }
    if (task->taskStatus > TASK_STATUS_CLIENT_PAKE_CONFIRM) {
        LOGI("The message is repeated, ignore it, status :%d", task->taskStatus);
        *status = IGNORE_MSG;
        return HC_SUCCESS;
    }

    // parse message
    res = ParsePakeServerConfirmMessage(params, in);
    if (res != HC_SUCCESS) {
        LOGE("ParsePakeServerConfirmMessage failed, res: %d.", res);
        return res;
    }

    // execute
    res = ClientVerifyConfirmPakeProtocol(&params->baseParams);
    if (res != HC_SUCCESS) {
        LOGE("ClientVerifyConfirmPakeProtocol failed, res: %d.", res);
        return res;
    }

    task->taskStatus = TASK_STATUS_CLIENT_PAKE_VERIFY_CONFIRM;
    *status = FINISH;
    return res;
}

static int Process(struct AsyBaseCurTaskT *task, PakeParams *params, const CJson *in, CJson *out, int *status)
{
    int res = HC_SUCCESS;
    uint32_t step = ProtocolMessageIn(in);
    if (step == INVALID_MESSAGE) {
        res = PakeRequest(task, params, in, out, status);
        step = 1;
        goto out;
    }

    step = step + 1; /* when receive peer message code, need to do next step */
    switch (step) {
        case STEP_TWO:
            res = PakeClientConfirm(task, params, in, out, status);
            break;
        case STEP_THREE:
            res = PakeClientVerifyConfirm(task, params, in, out, status);
            break;
        default:
            res = HC_ERR_BAD_MESSAGE;
            break;
    }
out:
    if (res != HC_SUCCESS) {
        SendErrorToOut(out, params->opCode, res);
    } else {
        if (step != STEP_THREE) {
            res = ClientProtocolMessageOut(out, params->opCode, step);
        }
    }
    return res;
}

static void DestroyPakeProtocolClientTask(struct AsyBaseCurTaskT *task)
{
    PakeProtocolClientTask *innerTask = (PakeProtocolClientTask *)task;
    if (innerTask == NULL) {
        return;
    }

    HcFree(innerTask);
}

AsyBaseCurTask *CreatePakeProtocolClientTask()
{
    PakeProtocolClientTask *task = (PakeProtocolClientTask *)HcMalloc(sizeof(PakeProtocolClientTask), 0);
    if (task == NULL) {
        return NULL;
    }
    task->taskBase.destroyTask = DestroyPakeProtocolClientTask;
    task->taskBase.process = Process;
    task->taskBase.taskStatus = TASK_STATUS_CLIENT_PAKE_BEGIN;
    task->taskBase.getCurTaskType = GetTaskType;
    return (AsyBaseCurTask *)task;
}