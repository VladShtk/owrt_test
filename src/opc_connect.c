#include "opc_connect.h"
#include "logger.h"

UA_Client *_handler;

void listTreeRecursive(UA_Client *client, UA_NodeId nodeId);
static UA_INLINE UA_ByteString loadFile(const char *const path);

/**
 * Connect to OPC server
 *
 * @param  pServerName - name of server
 * @param  secureConnection - use secure
 * @param  pAppURI - application Uri
 * @param  pCertFullName - client certificate full name
 * @param  pKeyFullName  - client path full name
 * @return true - ok, false - error
 */
bool opcConnect(const char *pServerName,
				bool secureConnection,
				OpcSecurityMode securityMode,
				const char *pAppURI,
				const char *pCertFullName,
				const char *pKeyFullName)
{
	UA_StatusCode retval;

	_handler = UA_Client_new();

	if(secureConnection)
	{
		// Load certificate and private key
		UA_ByteString certificate = loadFile(pCertFullName);
		UA_ByteString privateKey  = loadFile(pKeyFullName);

		if(certificate.length == 0)
		{
			LOG_E("Can't load sertificate: %s", pCertFullName);
			return false;
		}

		if(privateKey.length == 0)
		{
			LOG_E("Can't load privateKey: %s", pKeyFullName);
			return false;
		}

		// Load the trustList. Load revocationList is not supported now
		UA_ByteString trustList[2];
		size_t trustListSize = 0;

		UA_ByteString *revocationList = NULL;
		size_t revocationListSize = 0;

		UA_ClientConfig *cc = UA_Client_getConfig(_handler);

		if(securityMode == OSM_NONE)
			cc->securityMode = UA_MESSAGESECURITYMODE_NONE;
		else if(securityMode == OSM_SIGN)
			cc->securityMode = UA_MESSAGESECURITYMODE_SIGN;
		else if(securityMode == OSM_SIGN_ENCRYPT)
			cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
		else
		{
			LOG_E("Wrong security mode type");
			return false;
		}

		UA_ClientConfig_setDefaultEncryption(cc, certificate, privateKey,
											  trustList, trustListSize,
											  revocationList, revocationListSize);

		UA_ByteString_clear(&certificate);
		UA_ByteString_clear(&privateKey);
		for(size_t deleteCount = 0; deleteCount < trustListSize; deleteCount++) {
			 UA_ByteString_clear(&trustList[deleteCount]);
		}

		cc->clientDescription.applicationUri = UA_STRING_ALLOC(pAppURI);
	}
	else
	{
	    UA_ClientConfig_setDefault(UA_Client_getConfig(_handler));
	}

    // Connect to a server, example server name "opc.tcp://Host:53530/OPCUA/SimulationServer"
    retval = UA_Client_connect(_handler, pServerName);

    if(retval != UA_STATUSCODE_GOOD)
    {
        UA_Client_delete(_handler);
        _handler = NULL;
        LOG_E("Can't connect to server: %s, secure: %s, retval: %u",
        		pServerName,
        		(secureConnection) ? "yes" : "no",
        		retval);

        return false;
    }

    // Uncomment to browse selected node
    //listTreeRecursive(_handler, UA_NODEID_STRING(3, "DataBlocksGlobal")/* UA_NODEID_NUMERIC(2, 1005)*/);

	return true;
}

/**
 * Receive data from server
 *
 * @param  pClient - client handle
 * @param  pData   - data
 * @return true - ok, false - error
 */
bool opcReceiveData(OpcClient *pClient, ClientData *pData)
{
    int nodeNameLen = strlen(pClient->node);
	UA_Variant *val = UA_Variant_new();
    UA_StatusCode retval;

    char nodeName[MAX_OPC_NODE_LEN + 1];

    if(nodeNameLen > MAX_OPC_NODE_LEN)
    {
    	LOG_E("Too big node name: %s, max %d chars", pClient->node, MAX_OPC_NODE_LEN);
    	return false;
    }

    if(_handler == NULL)
    {
    	LOG_E("OPC Handler == NULL, need reconnect");
    	return false;
    }

    memcpy(nodeName, pClient->node, nodeNameLen + 1);

    retval = UA_Client_readValueAttribute(_handler, UA_NODEID_STRING(pClient->nameSpace, nodeName), val);

    if(retval == UA_STATUSCODE_GOOD)
    {
    	pData->dataType = pClient->dataType;

    	switch(pData->dataType)
    	{
    		case MDT_BOOL:
    			if(strcmp(val->type->typeName, "Boolean") == 0)
    				pData->data_bool = *(UA_Boolean*)val->data;
    			else
    			{
    				LOG_E("Wrong data type, expect: %s, got: %s, node: %s", "Boolean", val->type->typeName, pClient->node);
    				pData->data_bool = false;
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}
    			break;

    		case MDT_INT:
    			if(strcmp(val->type->typeName, "Int16") == 0)
    				pData->data_int = (int)*(UA_Int16*)val->data;
    			else if(strcmp(val->type->typeName, "UInt16") == 0)
    				pData->data_int = (unsigned int)*(UA_UInt16*)val->data;
    			else if(strcmp(val->type->typeName, "Int32") == 0)
    				pData->data_int = (int)*(UA_Int32*)val->data;
    			else if(strcmp(val->type->typeName, "UInt32") == 0)
    				pData->data_int = (unsigned int)*(UA_UInt32*)val->data;
    			else
    			{
    				LOG_E("Wrong data type, expect: %s, got: %s, node: %s", "Int16, UInt16, Int32, UInt32", val->type->typeName, pClient->node);
    				pData->data_int = 0;
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}

    			LOG_D("OPC int data: %d", pData->data_int);

    		    break;


       		case MDT_DWORD:
    			if(strcmp(val->type->typeName, "UInt32") == 0)
    			{
    				pData->data_dword = (DWORD)*(UA_Int32*)val->data;
    				LOG_D("OPC long data: %ld", pData->data_dword);
    			}
    			else
    			{
    				LOG_E("Wrong data type, expect dword as: %s, got: %s, node: %s", "UInt32", val->type->typeName, pClient->node);
    				pData->data_dword = 0;
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}
    			break;

       		case MDT_DOUBLE:
    			if(strcmp(val->type->typeName, "Double") == 0)
    			{
    				pData->data_double = (double)*(UA_Double*)val->data;
    				LOG_D("OPC double data: %f", pData->data_double);
    			}
    			else
    			{
    				LOG_E("Wrong data type, expect: %s, got: %s, node: %s", "Double", val->type->typeName, pClient->node);
    				pData->data_double = 0;
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}

        		break;

       		case MDT_ENUM:
    			if(strcmp(val->type->typeName, "EnumValueType") == 0)
    			{
    				UA_EnumValueType enumVal = *(UA_EnumValueType*)val->data;
    				pData->data_enum = (unsigned int)enumVal.value;
    				LOG_D("OPC enum data(uint): %u", pData->data_enum);
    			}
    			else
    			{
    				LOG_E("Wrong data type, expect: %s, got: %s, node: %s", "EnumValueType", val->type->typeName, pClient->node);
    				pData->data_enum = 0;
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}
       		    break;

       		case MDT_CHAR:
    			if(strcmp(val->type->typeName, "String") == 0)
    			{
    				strncpy(pData->data_char, (char*)val->data, MAX_OPC_STR_LEN); // svv test it
    				LOG_D("OPC char data: %s", (char*)val->data);
    			}
    			else
    			{
    				LOG_E("Wrong data type, expect: %s, got: %s, node: %s", "String", val->type->typeName, pClient->node);
    				pData->data_char[0] = '\0';
    				retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    			}

        		break;

       		default:
       			LOG_E("Unsupported data type %d, node: %s", (int)pData->dataType, pClient->node);
       			retval = UA_STATUSCODE_BADDATATYPEIDUNKNOWN;
    	}
    }
    else
    {
    	retval = UA_STATUSCODE_BADCOMMUNICATIONERROR;
    	LOG_E("Fail to read attribute %d; %s\n", pClient->nameSpace, pClient->node);
    }

    UA_Variant_delete(val);

    if(retval == UA_STATUSCODE_BADDATATYPEIDUNKNOWN)
    	return true;

    return (retval == UA_STATUSCODE_GOOD);
}

/**
 * Close connection
 */
void opcCloseConnection()
{
	if(_handler != NULL)
	{
		UA_Client_delete(_handler);
		_handler = NULL;
	}
}

/**
 * load File to UA_ByteString
 *
 * @param  path - full file path
 * @return OK - UA_ByteString, ERROR - UA_STRING_NULL
 */
static UA_INLINE UA_ByteString loadFile(const char *const path)
{
    UA_ByteString fileContents = UA_STRING_NULL;

    // Open the file
    FILE *fp = fopen(path, "rb");
    if(!fp) {
        errno = 0; // We read errno also from the tcp layer...
        return fileContents;
    }

    /* Get the file length, allocate the data and read */
    fseek(fp, 0, SEEK_END);
    fileContents.length = (size_t)ftell(fp);
    fileContents.data = (UA_Byte *)UA_malloc(fileContents.length * sizeof(UA_Byte));
    if(fileContents.data) {
        fseek(fp, 0, SEEK_SET);
        size_t read = fread(fileContents.data, sizeof(UA_Byte), fileContents.length, fp);
        if(read != fileContents.length)
            UA_ByteString_clear(&fileContents);
    } else {
        fileContents.length = 0;
    }
    fclose(fp);

    return fileContents;
}

/**
 * Browse node
 *
 * @param  client - client handle
 * @param  nodeId - node
 * @return OK - UA_ByteString, ERROR - UA_STRING_NULL
 */
void listTreeRecursive(UA_Client *client, UA_NodeId nodeId)
{
	// Browse some objects
    printf("Browsing nodes in objects folder:\n");
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = nodeId; // browse objects folder
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; // return everything
    UA_BrowseResponse bResp = UA_Client_Service_browse(_handler, bReq);
    printf("%-9s %-16s %-16s %-16s\n", "NAMESPACE", "NODEID", "BROWSE NAME", "DISPLAY NAME");

    for(size_t i = 0; i < bResp.resultsSize; ++i)
    {
        for(size_t j = 0; j < bResp.results[i].referencesSize; ++j)
        {
            UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
            if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC)
            {
                printf("%-9d %-16d %-16.*s %-16.*s\n", ref->nodeId.nodeId.namespaceIndex,
                       ref->nodeId.nodeId.identifier.numeric, (int)ref->browseName.name.length,
                       ref->browseName.name.data, (int)ref->displayName.text.length,
                       ref->displayName.text.data);
            }
            else if(ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING)
            {
                printf("%-9d %-16.*s %-16.*s %-16.*s\n", ref->nodeId.nodeId.namespaceIndex,
                       (int)ref->nodeId.nodeId.identifier.string.length,
                       ref->nodeId.nodeId.identifier.string.data,
                       (int)ref->browseName.name.length, ref->browseName.name.data,
                       (int)ref->displayName.text.length, ref->displayName.text.data);
            }
            // TODO: distinguish further types
        }
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

