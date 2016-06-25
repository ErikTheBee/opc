#include "ua_server_internal.h"
#include "ua_services.h"
#include "ua_util.h"

void Service_FindServers(UA_Server *server, UA_Session *session,
                         const UA_FindServersRequest *request, UA_FindServersResponse *response) {
    UA_LOG_DEBUG_SESSION(server->config.logger, session, "Processing FindServersRequest");
    /* copy ApplicationDescription from the config */
    UA_ApplicationDescription *descr = UA_malloc(sizeof(UA_ApplicationDescription)*server->applicationsSize);
    if(!descr) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADOUTOFMEMORY;
        return;
    }

    for(size_t i=0;i<server->applicationsSize;i++){
        UA_ApplicationDescription_init(&descr[i]);
        response->responseHeader.serviceResult =
                UA_ApplicationDescription_copy(&server->applications[i].description, &descr[i]);
        if(response->responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
            UA_free(descr);
            return;
        }
    }

    response->servers = descr;
    response->serversSize = server->applicationsSize;
}

void Service_GetEndpoints(UA_Server *server, UA_Session *session, const UA_GetEndpointsRequest *request,
                          UA_GetEndpointsResponse *response) {
    UA_LOG_DEBUG_SESSION(server->config.logger, session, "Processing GetEndpointsRequest");
    /* Test if one of the networklayers exposes the discoveryUrl of the requested endpoint */
    /* Disabled, servers in a virtualbox don't know their external hostname */
    /* UA_Boolean foundUri = false; */
    /* for(size_t i = 0; i < server->config.networkLayersSize; i++) { */
    /*     if(UA_String_equal(&request->endpointUrl, &server->config.networkLayers[i].discoveryUrl)) { */
    /*         foundUri = true; */
    /*         break; */
    /*     } */
    /* } */
    /* if(!foundUri) { */
    /*     response->endpointsSize = 0; */
    /*     return; */
    /* } */
    
    /* locate relevant application */
    UA_Application* application = NULL;
    for(size_t i=0;i<server->applicationsSize;i++){
        UA_Application* temp_application = &server->applications[i];
        for(size_t j=0;j<temp_application->description.discoveryUrlsSize;j++){
            if(UA_String_equal(&request->endpointUrl, &temp_application->description.discoveryUrls[j])){
                application = temp_application;
                break;
            }
        }
        if(application)
            break;
    }

    if(!application){
        response->endpointsSize = 0;
        return;
    }

    /* test if the supported binary profile shall be returned */
#ifdef NO_ALLOCA
    UA_Boolean relevant_endpoints[application->endpointsSize];
#else
    UA_Boolean *relevant_endpoints = UA_alloca(sizeof(UA_Byte) * application->endpointsSize);
#endif
    size_t relevant_count = 0;
    for(size_t j = 0; j < application->endpointsSize; j++) {
        relevant_endpoints[j] = false;
        if(request->profileUrisSize == 0) {
            relevant_endpoints[j] = true;
            relevant_count++;
            continue;
        }
        for(size_t i = 0; i < request->profileUrisSize; i++) {
            if(UA_String_equal(&request->profileUris[i], &application->endpoints[j]->description.transportProfileUri)) {
                relevant_endpoints[j] = true;
                relevant_count++;
                break;
            }
        }
    }

    if(relevant_count == 0) {
        response->endpointsSize = 0;
        return;
    }

    response->endpoints = UA_malloc(sizeof(UA_EndpointDescription) * relevant_count);
    if(!response->endpoints) {
        response->responseHeader.serviceResult = UA_STATUSCODE_BADOUTOFMEMORY;
        return;
    }

    size_t k = 0;
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    for(size_t j = 0; j < server->endpointsSize && retval == UA_STATUSCODE_GOOD; j++) {
        if(!relevant_endpoints[j])
            continue;
        retval = UA_EndpointDescription_copy(&application->endpoints[j]->description, &response->endpoints[k]);
        if(retval != UA_STATUSCODE_GOOD)
            break;
        /* replace endpoint's URL to the requested one if provided */
        //if(request->endpointUrl.length > 0){
        //    UA_String_deleteMembers(&response->endpoints[k].endpointUrl);
        //    retval = UA_String_copy(&request->endpointUrl, &response->endpoints[k].endpointUrl);
        //}
        k++;
    }

    if(retval != UA_STATUSCODE_GOOD) {
        response->responseHeader.serviceResult = retval;
        UA_Array_delete(response->endpoints, --k, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
        return;
    }
    response->endpointsSize = relevant_count;
}
