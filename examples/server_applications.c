/*
 * This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 */

// This file contains source-code that is discussed in a tutorial located here:
// http://open62541.org/doc/sphinx/tutorial_firstStepsServer.html
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#ifdef UA_NO_AMALGAMATION
# include "ua_types.h"
# include "ua_server.h"
# include "ua_config_standard.h"
# include "ua_network_tcp.h"
# include "ua_log_stdout.h"
#else
# include "open62541.h"
#endif

UA_Boolean running = true;

static UA_NodeId componentFolderNodeId;
static UA_UInt32 currentId = 1000;
static void stopHandler(int sign) {
    running = false;
}
typedef struct readNodeIdReturn {
    UA_Server *server;
    UA_NodeId nodeId;
    UA_QualifiedName browseName;
    UA_Boolean found;
} readNodeIdReturn;

//Callback to look up the addressed component
static UA_StatusCode readNodeId(UA_NodeId childId, UA_Boolean isInverse,
        UA_NodeId referenceTypeId, void *handle) {
    if (isInverse)
        return UA_STATUSCODE_GOOD;
    readNodeIdReturn* rNIR = (readNodeIdReturn*) handle;
    UA_QualifiedName browseName;
    UA_Server_readBrowseName(rNIR->server, childId, &browseName);
    if (UA_String_equal(&browseName.name,
            &rNIR->browseName.name) && rNIR->found==false) {
        rNIR->found = true;
        rNIR->nodeId = childId;
    }
    return UA_STATUSCODE_GOOD;
}
static UA_NodeId getNewNodeId(UA_UInt16 ns) {
    return UA_NODEID_NUMERIC(ns, currentId++);

}
static void createMessageObject(UA_Server *server, UA_NodeId parentId,
        UA_String *Sender, UA_String *Receiver, UA_String *Message) {
    UA_QUALIFIEDNAME(0, "message");
    UA_ObjectAttributes objAtr;
    UA_ObjectAttributes_init(&objAtr);

    objAtr.description = UA_LOCALIZEDTEXT("en", "message");
    objAtr.displayName = UA_LOCALIZEDTEXT("en", "message");

    UA_VariableAttributes varAtr1;
    UA_VariableAttributes varAtr2;
    UA_VariableAttributes varAtr3;

    UA_VariableAttributes_init(&varAtr1);
    UA_VariableAttributes_init(&varAtr2);
    UA_VariableAttributes_init(&varAtr3);

    varAtr1.description = UA_LOCALIZEDTEXT("en_US", "Sender");
    varAtr1.displayName = UA_LOCALIZEDTEXT("en_US", "Sender");

    varAtr2.description = UA_LOCALIZEDTEXT("en_US", "Receiver");
    varAtr2.displayName = UA_LOCALIZEDTEXT("en_US", "Receiver");

    varAtr3.description = UA_LOCALIZEDTEXT("en_US", "Message");
    varAtr3.displayName = UA_LOCALIZEDTEXT("en_US", "Message");

    UA_Variant senderValue;
    UA_Variant_init(&senderValue);
    UA_Variant_setScalarCopy(&senderValue, Sender, &UA_TYPES[UA_TYPES_STRING]);

    UA_Variant receiverValue;
    UA_Variant_init(&receiverValue);
    UA_Variant_setScalarCopy(&receiverValue, Receiver,
            &UA_TYPES[UA_TYPES_STRING]);

    UA_Variant messageValue;
    UA_Variant_init(&messageValue);
    UA_Variant_setScalarCopy(&messageValue, Message,
            &UA_TYPES[UA_TYPES_STRING]);

    //create message object and initialize its values
    UA_NodeId newNodeId;
    UA_Server_addObjectNode(server, getNewNodeId(1), parentId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            UA_QUALIFIEDNAME(0, "message"), UA_NODEID_NUMERIC(0, 58), objAtr,
            NULL, &newNodeId);
    UA_NodeId parNodeId = newNodeId;

    UA_Server_addVariableNode(server, getNewNodeId(1), parNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(0, "Sender"), UA_NODEID_NULL, varAtr1, NULL,
            &newNodeId);
    UA_Server_writeValue(server, newNodeId, senderValue);

    UA_Server_addVariableNode(server, getNewNodeId(1), parNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(0, "Receiver"), UA_NODEID_NULL, varAtr2, NULL,
            &newNodeId);
    UA_Server_writeValue(server, newNodeId, receiverValue);

    UA_Server_addVariableNode(server, getNewNodeId(1), parNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
            UA_QUALIFIEDNAME(0, "Message"), UA_NODEID_NULL, varAtr3, NULL,
            &newNodeId);
    UA_Server_writeValue(server, newNodeId, messageValue);
}
//Callback to receive a message
static UA_StatusCode dropMessageFunction(void *handle, const UA_NodeId objectId,
        size_t inputSize, const UA_Variant *input, size_t outputSize,
        UA_Variant *output) {
    UA_Server *server = (UA_Server*) handle;
    UA_String *receiver = (UA_String *) input[0].data;
    UA_String *sender = (UA_String *) input[1].data;
    UA_String *message = (UA_String *) input[2].data;

    //translate global ids to local ids
    readNodeIdReturn rNIR;
    char* componentName = malloc(receiver->length + 1);
    memcpy(componentName, receiver->data, receiver->length);
    componentName[receiver->length] = 0;

    rNIR.browseName = UA_QUALIFIEDNAME(0, componentName);
    rNIR.found = false;
    rNIR.server = server;

    UA_Server_forEachChildNodeCall(server, componentFolderNodeId, &readNodeId,
            &rNIR);
    if (rNIR.found) {
        //get inbox nodeId
        readNodeIdReturn rNIR1;
        rNIR1.browseName = UA_QUALIFIEDNAME(0, "Inbox");
        rNIR1.found = false;
        rNIR1.server = server;
        UA_Server_forEachChildNodeCall(server, rNIR.nodeId, &readNodeId,
                &rNIR1);
        if (rNIR1.found) {
            createMessageObject(server, rNIR1.nodeId, sender, receiver,
                    message);
        }

    }
    free(componentName);
    return UA_STATUSCODE_GOOD;
}

static void createComponent(UA_ObjectAttributes* objAtrC1, UA_NodeId newNodeId,
        char* componentName, UA_Server* server, UA_NodeId parentNodeId) {
    UA_QualifiedName browseNameC1 = UA_QUALIFIEDNAME(0, componentName);
    objAtrC1->description = UA_LOCALIZEDTEXT("en", componentName);
    objAtrC1->displayName = UA_LOCALIZEDTEXT("en", componentName);
    UA_NodeId nodeId = newNodeId;
    UA_Server_addObjectNode(server, nodeId, parentNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), browseNameC1,
            UA_NODEID_NUMERIC(0, 58), *objAtrC1, NULL, &nodeId);
    parentNodeId = nodeId;
    UA_ObjectAttributes objAtrInbox;
    UA_QualifiedName browseNameInbox = UA_QUALIFIEDNAME(0, "Inbox");
    objAtrInbox.description = UA_LOCALIZEDTEXT("en", "Inbox");
    objAtrInbox.displayName = UA_LOCALIZEDTEXT("en", "Inbox");
    UA_NodeId inboxNodeId = UA_NODEID_NUMERIC(nodeId.namespaceIndex,
            nodeId.identifier.numeric + 1);
    UA_Server_addObjectNode(server, getNewNodeId(nodeId.namespaceIndex),
            parentNodeId, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
            browseNameInbox, UA_NODEID_NUMERIC(0, 61), objAtrInbox, NULL,
            &inboxNodeId);
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(
            UA_ConnectionConfig_standard, 16664);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;

    UA_Server *server = UA_Server_new(config);

    UA_ApplicationDescription app0;
    UA_ApplicationDescription_copy(&config.applicationDescription, &app0);
    UA_String_deleteMembers(&app0.applicationName.text);
    app0.applicationName.text = UA_STRING_ALLOC("Engineering");
    app0.discoveryUrlsSize = 1;
    app0.discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
    app0.discoveryUrls[0] = UA_STRING_ALLOC("/app0");
    UA_UInt16 ns[4];
    ns[0] = 0;
    ns[1] = 1;
    ns[2] = 2;
    ns[3] = 3;
    UA_Server_addApplication(server, &app0, ns, 4);
    UA_ApplicationDescription_deleteMembers(&app0);

    UA_ApplicationDescription app1;
    UA_ApplicationDescription_copy(&config.applicationDescription, &app1);
    UA_String_deleteMembers(&app1.applicationName.text);
    app1.applicationName.text = UA_STRING_ALLOC("Message");
    app1.discoveryUrlsSize = 1;
    app1.discoveryUrls = UA_Array_new(1, &UA_TYPES[UA_TYPES_STRING]);
    app1.discoveryUrls[0] = UA_STRING_ALLOC("/app1");
    ns[0] = 0;
    ns[1] = 1;
    UA_Server_addApplication(server, &app1, ns, 2);
    UA_ApplicationDescription_deleteMembers(&app1);
    /* initialize the server */

    UA_NodeId newNodeId = UA_NODEID_NUMERIC(1, 101);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, 85);

    /*Create PeerManager */
    UA_ObjectAttributes objAtrPeerManager;
    UA_ObjectAttributes_init(&objAtrPeerManager);
    UA_NodeId peerManagerNodeId = UA_NODEID_NUMERIC(1, 1000000);
    char* peerManager = "LMSR";
    createComponent(&objAtrPeerManager, peerManagerNodeId, peerManager, server,
            parentNodeId);
    /*create method node for property creation */
    /*Demo */
    /*create folder for components */
    /*        componentFolderNodeId= UA_NODEID_NUMERIC(1, 501);
     UA_ObjectAttributes objAtrComponentFolder;
     UA_QualifiedName browseNameComponentFolder = UA_QUALIFIEDNAME(0, "Components");
     objAtrComponentFolder.description = UA_LOCALIZEDTEXT("en", "Components");
     objAtrComponentFolder.displayName = UA_LOCALIZEDTEXT("en", "Components");
     UA_Server_addObjectNode(server, componentFolderNodeId, peerManagerNodeId,
     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), browseNameComponentFolder,
     UA_NODEID_NUMERIC(0, 61), objAtrComponentFolder, NULL, &componentFolderNodeId);
     */
    /*Create Components 1-3 */
    UA_ObjectAttributes objAtrC1;
    UA_ObjectAttributes_init(&objAtrC1);
    char* componentName1 = "Component1";
    /*        createComponent(&objAtrC1, getNewNodeId(2), componentName1, server,
     parentNodeId);
     */
    UA_ObjectAttributes objAtrC2;
    UA_ObjectAttributes_init(&objAtrC2);
    char* componentName2 = "Component2";
    /*        createComponent(&objAtrC2, getNewNodeId(3), componentName2, server,
     parentNodeId);
     */
    UA_ObjectAttributes objAtrC3;
    UA_ObjectAttributes_init(&objAtrC3);
    char* componentName3 = "Component3";
    /*         createComponent(&objAtrC3, getNewNodeId(4), componentName3, server,
     parentNodeId);
     */

    /*Demo */
    UA_MethodAttributes dropMessageAtr;
    UA_MethodAttributes_init(&dropMessageAtr);
    dropMessageAtr.description = UA_LOCALIZEDTEXT("en",
            "drops a message to the opc ua peer");
    dropMessageAtr.displayName = UA_LOCALIZEDTEXT("en", "dropMessage");
    dropMessageAtr.executable = true;
    dropMessageAtr.userExecutable = true;

    size_t inputArgSize = 3;
    UA_Argument* inArgs = UA_Array_new(inputArgSize,
            &UA_TYPES[UA_TYPES_ARGUMENT]);
    for (size_t i = 0; i < inputArgSize; i++) {
        UA_Argument_init(&inArgs[i]);
    }

    inArgs[0].arrayDimensionsSize = 0;
    inArgs[0].arrayDimensions = NULL;
    inArgs[0].dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    inArgs[0].description = UA_LOCALIZEDTEXT_ALLOC("en_US",
            "Unique receiver address");
    inArgs[0].name = UA_STRING_ALLOC("Receiver Address");
    inArgs[0].valueRank = -1;

    inArgs[1].arrayDimensionsSize = 0;
    inArgs[1].arrayDimensions = NULL;
    inArgs[1].dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    inArgs[1].description = UA_LOCALIZEDTEXT_ALLOC("en", "Unique sender address");
    inArgs[1].name = UA_STRING_ALLOC("Sender Address");
    inArgs[1].valueRank = -1;

    inArgs[2].arrayDimensionsSize = 0;
    inArgs[2].arrayDimensions = NULL;
    inArgs[2].dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    inArgs[2].description = UA_LOCALIZEDTEXT_ALLOC("en", "Message");
    inArgs[2].name = UA_STRING_ALLOC("Message");
    inArgs[2].valueRank = -1;

    //add global "dropMessage" node which is works as the peer's mailbox
    UA_QualifiedName dropMsgBrName;

    dropMsgBrName = UA_QUALIFIEDNAME(0, "dropMessage");

    UA_Server_addMethodNode(server, newNodeId, peerManagerNodeId,
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT), dropMsgBrName,
            dropMessageAtr, &dropMessageFunction, (void*) server, inputArgSize,
            inArgs, 0, NULL, &newNodeId);

    UA_Array_delete(inArgs, inputArgSize, &UA_TYPES[UA_TYPES_ARGUMENT]);

    /*create folder for components */
    //componentFolderNodeId= UA_NODEID_NUMERIC(1, 502);
    //UA_ObjectAttributes objAtrComponentFolder;
    //UA_QualifiedName browseNameComponentFolder = UA_QUALIFIEDNAME(0, "Components");

    /*Create Components 1-3 */

    UA_ObjectAttributes_init(&objAtrC1);
    UA_NodeId newNodeId1 = getNewNodeId(2);

    createComponent(&objAtrC1, newNodeId1, componentName1, server,
            parentNodeId);

    UA_ObjectAttributes_init(&objAtrC2);
    UA_NodeId newNodeId2 = getNewNodeId(3);

    createComponent(&objAtrC2, newNodeId2, componentName2, server,
            parentNodeId);

    UA_ObjectAttributes_init(&objAtrC3);
    UA_NodeId newNodeId3 = getNewNodeId(4);

    createComponent(&objAtrC3, newNodeId3, componentName3, server,
            parentNodeId);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    nl.deleteMembers(&nl);

    return (int) retval;
}
