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

#ifndef DEVICE_AUTH_H
#define DEVICE_AUTH_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__LINUX__) || defined(_UNIX)
#define DEVICE_AUTH_API_PUBLIC __attribute__ ((visibility("default")))
#else
#define DEVICE_AUTH_API_PUBLIC
#endif

#define FIELD_GROUP_ID "groupId"
#define FIELD_GROUP_TYPE "groupType"
#define FIELD_GROUP_NAME "groupName"
#define FIELD_PEER_DEVICE_ID "peerDeviceId"
#define FIELD_IS_ADMIN "isAdmin"
#define FIELD_CREDENTIAL_TYPE "credentialType"
#define FIELD_IS_FORCE_DELETE "isForceDelete"
#define FIELD_IS_IGNORE_CHANNEL "isIgnoreChannel"
#define FIELD_CONNECT_PARAMS "connectParams"
#define FIELD_ADD_ID "addId"
#define FIELD_DELETE_ID "deleteId"
#define FIELD_APP_ID "appId"
#define FIELD_SERVICE_TYPE "serviceType"
#define FIELD_PEER_AUTH_STATE "peerAuthState"
#define FIELD_IS_DEVICE_LEVEL "isDeviceLevel"
#define FIELD_ALTERNATIVE "alternative"
#define FIELD_PEER_UDID "peerUdid"
#define FIELD_PEER_CONN_DEVICE_ID "peerConnDeviceId"
#define FIELD_KEY_LENGTH "keyLength"
#define FIELD_IS_CLIENT "isClient"
#define FIELD_SESSION_KEY "sessionKey"
#define FIELD_AUTH_FORM "authForm"
#define FIELD_CONFIRMATION "confirmation"
#define FIELD_GROUP_OWNER "groupOwner"
#define FIELD_PEER_AUTH_ID "peerAuthId"
#define FIELD_PEER_USER_TYPE "peerUserType"
#define FIELD_PEER_USER_ID "peerUserId"
#define FIELD_SERVICE_PKG_NAME "servicePkgName"
#define FIELD_USER_TYPE "userType"
#define FIELD_USER_ID "userId"
#define FIELD_DEVICE_ID "deviceId"
#define FIELD_UID_HASH "uidHash"
#define FIELD_PIN_CODE "pinCode"
#define FIELD_AUTH_ID "authId"
#define FIELD_GROUP_VISIBILITY "groupVisibility"
#define FIELD_EXPIRE_TIME "expireTime"
#define FIELD_IS_DELETE_ALL "isDeleteAll"

typedef enum {
    GROUP_CREATE = 0,
    GROUP_DISBAND = 1,
    MEMBER_INVITE = 2,
    MEMBER_JOIN = 3,
    MEMBER_DELETE = 4,
    ACCOUNT_BIND = 5
} GroupOperationCode;

typedef enum {
    CREDENTIAL_SAVE = 0,
    CREDENTIAL_CLEAR = 1,
    CREDENTIAL_UPDATE = 2,
    CREDENTIAL_QUERY = 3,
} CredentialCode;

typedef enum {
    DEVICE_TYPE_ACCESSORY = 0,
    DEVICE_TYPE_CONTROLLER = 1,
    DEVICE_TYPE_PROXY = 2
} UserType;

#define REQUEST_REJECTED 0x80000005
#define REQUEST_ACCEPTED 0x80000006
#define REQUEST_WAITING 0x80000007

typedef struct {
    void (*onGroupCreated)(const char *groupInfo);
    void (*onGroupDeleted)(const char *groupInfo);
    void (*onDeviceBound)(const char *peerUdid, const char *groupInfo);
    void (*onDeviceUnBound)(const char *peerUdid, const char *groupInfo);
    void (*onDeviceNotTrusted)(const char *peerUdid);
    void (*onLastGroupDeleted)(const char *peerUdid, int groupType);
    void (*onTrustedDeviceNumChanged)(int curTrustedDeviceNum);
} DataChangeListener;

typedef struct {
    bool (*onTransmit)(int64_t requestId, const uint8_t *data, uint32_t dataLen);
    void (*onSessionKeyReturned)(int64_t requestId, const uint8_t *sessionKey, uint32_t sessionKeyLen);
    void (*onFinish)(int64_t requestId, int operationCode, const char *returnData);
    void (*onError)(int64_t requestId, int operationCode, int errorCode, const char *errorReturn);
    char *(*onRequest)(int64_t requestId, int operationCode, const char *reqParams);
} DeviceAuthCallback;

typedef struct {
    int32_t (*processData)(int64_t authReqId, const uint8_t *data, uint32_t dataLen,
        const DeviceAuthCallback *gaCallback);
    int32_t (*queryTrustedDeviceNum)(void);
    bool (*isTrustedDevice)(const char *udid);
    int32_t (*getAuthState)(int64_t authReqId, const char *groupId, const char *peerUdid,
        uint8_t *out, uint32_t *outLen);
    int32_t (*authDevice)(int64_t authReqId, const char *authParams, const DeviceAuthCallback *gaCallback);
    void (*informDeviceDisconnection)(const char *udid);
} GroupAuthManager;

typedef struct {
    int32_t (*regCallback)(const char *appId, const DeviceAuthCallback *callback);
    int32_t (*unRegCallback)(const char *appId);
    int32_t (*regDataChangeListener)(const char *appId, const DataChangeListener *listener);
    int32_t (*unRegDataChangeListener)(const char *appId);
    int32_t (*createGroup)(int64_t requestId, const char *appId, const char *createParams);
    int32_t (*deleteGroup)(int64_t requestId, const char *appId, const char *disbandParams);
    int32_t (*addMemberToGroup)(int64_t requestId, const char *appId, const char *addParams);
    int32_t (*deleteMemberFromGroup)(int64_t requestId, const char *appId, const char *deleteParams);
    int32_t (*processData)(int64_t requestId, const uint8_t *data, uint32_t dataLen);
    int32_t (*confirmRequest)(int64_t requestId, const char *appId, const char *confirmParams);
    int32_t (*bindPeer)(int64_t requestId, const char *appId, const char *bindParams);
    int32_t (*unbindPeer)(int64_t requestId, const char *appId, const char *unBindParams);
    int32_t (*processLiteData)(int64_t requestId, const char *appId, const uint8_t *data, uint32_t dataLen);
    int32_t (*authKeyAgree)(int64_t requestId, const char *appId, const char *buildParams);
    int32_t (*processKeyAgreeData)(int64_t requestId, const char *appId, const uint8_t *data, uint32_t dataLen);
    int32_t (*processCredential)(int operationCode, const char *reqJsonStr, char **returnJsonStr);
    int32_t (*getRegisterInfo)(char **returnRegisterInfo);
    int32_t (*getLocalConnectInfo)(char *returnInfo, int32_t bufLen);
    int32_t (*addGroupManager)(const char *appId, const char *groupId, const char *managerAppId);
    int32_t (*addGroupFriend)(const char *appId, const char *groupId, const char *friendAppId);
    int32_t (*deleteGroupManager)(const char *appId, const char *groupId, const char *managerAppId);
    int32_t (*deleteGroupFriend)(const char *appId, const char *groupId, const char *friendAppId);
    int32_t (*getGroupManagers)(const char *appId, const char *groupId, char **returnManagers, uint32_t *returnSize);
    int32_t (*getGroupFriends)(const char *appId, const char *groupId, char **returnFriends, uint32_t *returnSize);
    int32_t (*getGroupInfoById)(const char *appId, const char *groupId, char **returnGroupInfo);
    int32_t (*getGroupInfo)(const char *appId, const char *queryParams, char **returnGroupVec, uint32_t *groupNum);
    int32_t (*getJoinedGroups)(const char *appId, int groupType, char **returnGroupVec, uint32_t *groupNum);
    int32_t (*getRelatedGroups)(const char *appId, const char *peerDeviceId, char **returnGroupVec, uint32_t *groupNum);
    int32_t (*getDeviceInfoById)(const char *appId, const char *deviceId, const char *groupId, char **returnDeviceInfo);
    int32_t (*getTrustedDevices)(const char *appId, const char *groupId, char **returnDevInfoVec, uint32_t *deviceNum);
    int32_t (*checkAccessToGroup)(const char *appId, const char *groupId);
    bool (*isDeviceInGroup)(const char *appId, const char *groupId, const char *deviceId);
    void (*destroyInfo)(char **returnInfo);
} DeviceGroupManager;

#ifdef __cplusplus
extern "C" {
#endif
DEVICE_AUTH_API_PUBLIC int InitDeviceAuthService();
DEVICE_AUTH_API_PUBLIC void DestroyDeviceAuthService();
DEVICE_AUTH_API_PUBLIC const GroupAuthManager *GetGaInstance();
DEVICE_AUTH_API_PUBLIC const DeviceGroupManager *GetGmInstance();
#ifdef __cplusplus
}
#endif
#endif
