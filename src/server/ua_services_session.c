#include "ua_services.h"
#include "ua_server_internal.h"
#include "ua_session_manager.h"
#include "ua_types_generated_encoding_binary.h"

void Service_CreateSession(UA_Server *server, UA_SecureChannel *channel,
                           const UA_CreateSessionRequest *request, UA_CreateSessionResponse *response) {
    if(channel->securityToken.channelId == 0) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADSECURECHANNELIDINVALID;
        return;
    }

    UA_Endpoint* endpoint = NULL;
    UA_String* requestEndpoint = cutoffStringBeforeThirdSlash(&request->endpointUrl);
    for(size_t i=0; i<server->applicationsSize && !endpoint; i++){
        UA_Application* application = &server->applications[i];
        for(size_t j=0; j<application->endpointsSize; j++){
            UA_Endpoint* temp_endpoint = application->endpoints[j];
            UA_String* testUrl = cutoffStringBeforeThirdSlash(&temp_endpoint->description.endpointUrl);
            if(UA_String_equal(requestEndpoint, testUrl)){
                endpoint = temp_endpoint;
            }
            UA_String_delete(testUrl);
            if(endpoint)
                break;
        }
        if(endpoint)
            break;
    }
    UA_String_delete(requestEndpoint);


    if(!endpoint && server->applicationsSize>0 && server->applications[0].endpointsSize>0){
        //no endpoint matched - attach to the first one
        endpoint = server->applications[0].endpoints[0];
    }

    if(!endpoint){
        response->responseHeader.serviceResult = UA_STATUSCODE_BADTCPENDPOINTURLINVALID;
        return;
    }

    UA_Application* application = endpoint->application;

    response->serverEndpoints = UA_malloc(sizeof(UA_EndpointDescription) * application->endpointsSize);

    if(!response->serverEndpoints){
        response->responseHeader.serviceResult = UA_STATUSCODE_BADOUTOFMEMORY;
        return;
    }

    for(size_t i=0;i<application->endpointsSize;i++){
        response->responseHeader.serviceResult =
                UA_copy(&application->endpoints[i]->description, &response->serverEndpoints[i], &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    }

    if(response->responseHeader.serviceResult != UA_STATUSCODE_GOOD)
        return;

    response->serverEndpointsSize = application->endpointsSize;

    UA_Session *newSession;
    response->responseHeader.serviceResult =
        UA_SessionManager_createSession(&server->sessionManager, channel, request, &newSession);
    if(response->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_LOG_DEBUG_CHANNEL(server->config.logger, channel, "Processing CreateSessionRequest failed");
        return;
    }

    //binding session to endpoint
    newSession->endpoint = endpoint;

    newSession->maxResponseMessageSize = request->maxResponseMessageSize;
    newSession->maxRequestMessageSize = channel->connection->localConf.maxMessageSize;
    response->sessionId = newSession->sessionId;
    response->revisedSessionTimeout = (UA_Double)newSession->timeout;
    response->authenticationToken = newSession->authenticationToken;
    response->responseHeader.serviceResult = UA_String_copy(&request->sessionName, &newSession->sessionName);
    response->responseHeader.serviceResult |= UA_ByteString_copy(&endpoint->description.serverCertificate,
                               &response->serverCertificate);
    if(response->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        UA_SessionManager_removeSession(&server->sessionManager, &newSession->authenticationToken);
         return;
    }
    UA_LOG_DEBUG_CHANNEL(server->config.logger, channel, "Session " UA_PRINTF_GUID_FORMAT " created",
                         UA_PRINTF_GUID_DATA(newSession->sessionId));
}

void
Service_ActivateSession(UA_Server *server, UA_SecureChannel *channel, UA_Session *session,
                        const UA_ActivateSessionRequest *request, UA_ActivateSessionResponse *response) {
    if(session->validTill < UA_DateTime_nowMonotonic()) {
        UA_LOG_INFO_SESSION(server->config.logger, session, "ActivateSession: SecureChannel %i wants "
                            "to activate, but the session has timed out", channel->securityToken.channelId);
        response->responseHeader.serviceResult = UA_STATUSCODE_BADSESSIONIDINVALID;
        return;
    }

    if(request->userIdentityToken.encoding < UA_EXTENSIONOBJECT_DECODED ||
       (request->userIdentityToken.content.decoded.type != &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN] &&
        request->userIdentityToken.content.decoded.type != &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN])) {
        UA_LOG_INFO_SESSION(server->config.logger, session, "ActivateSession: SecureChannel %i wants "
                            "to activate, but the UserIdentify token is invalid",
                            channel->securityToken.channelId);
        response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
        return;
    }


    UA_String ap = UA_STRING(ANONYMOUS_POLICY);
    UA_String up = UA_STRING(USERNAME_POLICY);

    /* Compatibility notice: Siemens OPC Scout v10 provides an empty policyId,
       this is not okay For compatibility we will assume that empty policyId == ANONYMOUS_POLICY
       if(token.policyId->data == NULL)
           response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
    */

    if(server->config.enableAnonymousLogin &&
       request->userIdentityToken.content.decoded.type == &UA_TYPES[UA_TYPES_ANONYMOUSIDENTITYTOKEN]) {
        /* anonymous login */
        const UA_AnonymousIdentityToken *token = request->userIdentityToken.content.decoded.data;
        if(token->policyId.data && !UA_String_equal(&token->policyId, &ap)) {
            response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
            return;
        }
    } else if(server->config.enableUsernamePasswordLogin &&
              request->userIdentityToken.content.decoded.type == &UA_TYPES[UA_TYPES_USERNAMEIDENTITYTOKEN]) {
        /* username login */
        const UA_UserNameIdentityToken *token = request->userIdentityToken.content.decoded.data;
        if(!UA_String_equal(&token->policyId, &up)) {
            response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
            return;
        }
        if(token->encryptionAlgorithm.length > 0) {
            /* we don't support encryption */
            response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
            return;
        }

        if(token->userName.length == 0 && token->password.length == 0) {
            /* empty username and password */
            response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
            return;
        }

        /* trying to match pw/username */
        UA_Boolean match = false;
        for(size_t i = 0; i < server->config.usernamePasswordLoginsSize; i++) {
            UA_String *user = &server->config.usernamePasswordLogins[i].username;
            UA_String *pw = &server->config.usernamePasswordLogins[i].password;
            if(UA_String_equal(&token->userName, user) && UA_String_equal(&token->password, pw)) {
                match = true;
                break;
            }
        }
        if(!match) {
            UA_LOG_INFO_SESSION(server->config.logger, session, "ActivateSession: Did not find matching username/password");
            response->responseHeader.serviceResult = UA_STATUSCODE_BADUSERACCESSDENIED;
            return;
        }
    } else {
        /* Unsupported token type */
        response->responseHeader.serviceResult = UA_STATUSCODE_BADIDENTITYTOKENINVALID;
        return;
    }

    /* Detach the old SecureChannel */
    if(session->channel && session->channel != channel) {
        UA_LOG_INFO_SESSION(server->config.logger, session, "ActivateSession: Detach from old channel");
        UA_SecureChannel_detachSession(session->channel, session);
    }

    /* Attach to the SecureChannel and activate */
    UA_SecureChannel_attachSession(channel, session);
    session->activated = true;
    UA_Session_updateLifetime(session);
    UA_LOG_INFO_SESSION(server->config.logger, session, "ActivateSession: Session activated");
}

void
Service_CloseSession(UA_Server *server, UA_Session *session, const UA_CloseSessionRequest *request,
                     UA_CloseSessionResponse *response) {
    UA_LOG_INFO_SESSION(server->config.logger, session, "CloseSession");
    response->responseHeader.serviceResult =
        UA_SessionManager_removeSession(&server->sessionManager, &session->authenticationToken);
}
