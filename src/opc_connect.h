#ifndef OPC_CONNECT_H
#define OPC_CONNECT_H

#include "open62541.h"
#include "ts_module_const.h"

/**
 * Secure mode
 */
typedef enum
{
	OSM_NONE,
	OSM_SIGN,
	OSM_SIGN_ENCRYPT
} OpcSecurityMode;

/**
 * Client info
 */
typedef struct
{
	int   nameSpace;              // Data name space
	bool  secure;			      // Enable secure
	const char *node;   	      // Name of node
	const char *serverName;       // Name of server
	DataType dataType;  	      // Type of data
	OpcSecurityMode securityMode; // Secure mode
} OpcClient;

bool opcConnect(const char *pServerName,
		        bool secureConnection,
		        OpcSecurityMode securityMode,
		        const char *pAppURI,
		        const char *pCertFullName,
		        const char *pKeyFullName);

bool opcReceiveData(OpcClient *pClient, ClientData *pData);
void opcCloseConnection();

#endif //OPC_CONNECT_H
