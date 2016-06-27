/*
 * This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 */

// This file contains source-code that is discussed in a tutorial located here:
// http://open62541.org/doc/sphinx/tutorial_firstStepsServer.html

#include <stdio.h>
#include <signal.h>

#ifdef UA_NO_AMALGAMATION
# include "ua_types.h"
# include "ua_server.h"
# include "ua_config_standard.h"
# include "networklayer_tcp.h"
#else
# include "open62541.h"
#endif

UA_Boolean running = true;
static void stopHandler(int sig) {
    running = false;
}

int main(void) {
    signal(SIGINT,  stopHandler);
    signal(SIGTERM, stopHandler);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    UA_Server *server = UA_Server_new(config);

    UA_ApplicationDescription app2;
    UA_ApplicationDescription_copy(&config.applicationDescription, &app2);
    UA_String_deleteMembers(&app2.applicationName.text);
    app2.applicationName.text = UA_STRING_ALLOC("Dummy Application");
    app2.discoveryUrlsSize = 1;
    app2.discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
    app2.discoveryUrls[0] = UA_STRING_ALLOC("/app2");
    UA_UInt16 ns[1];
    ns[0] = 0;
    UA_Server_addApplication(server,&app2,ns,1);


    /* add a variable node to the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en_US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer");
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NULL, attr, NULL, NULL);


    /* add a variable node to the address space */

    UA_VariableAttributes_init(&attr);
    myInteger = 43;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en_US","the answer1");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer1");
    myIntegerNodeId = UA_NODEID_STRING(0, "the.answer1");
    myIntegerName = UA_QUALIFIEDNAME(1, "the answer1");
    parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NULL, attr, NULL, NULL);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    nl.deleteMembers(&nl);

    UA_ApplicationDescription_deleteMembers(&app2);

    return (int)retval;
}
