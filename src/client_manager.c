#include "client_manager.h"
#include "logger.h"

#if __BYTE_ORDER == __BIG_ENDIAN
	#define IS_BIG_ENDIAN 1
#elif __BYTE_ORDER == __LITTLE_ENDIAN
	#define IS_BIG_ENDIAN 0
#else
     #error "Wrong endian type! file: modbus_connect.c"
#endif

#define SIZEOF_UINT16_T 2

int _clientsCnt;	    // Total num of clients
int _sendInterval = 0;  // Interval to send data in minute
ClientSettings _clientSettings[MAX_CLIENT_NUM+1];

/**
 * Set connect status
 *
 * @param  idx    - Inner idx of client
 * @param  status - status (connect, disconnect)
 */
void setConnectStatus(InnerIdx innerIdx, bool status)
{
	if(_clientSettings[innerIdx].protocol == CP_MODBUS)
	{
		_clientSettings[innerIdx].connected = status;
	}
	else if(_clientSettings[innerIdx].protocol == CP_OPC)
	{
		// OPC protocol, set status to all OPC client
		for(int i = 0; i < _clientsCnt; i++)
		{
			if(_clientSettings[i].protocol == CP_OPC)
				_clientSettings[i].connected = status;
		}
	}
	else
	{
		LOG_E("Wrong protocol");
		_clientSettings[innerIdx].connected = status;
	}
}

/**
 * Fill type specific fields (data_int, data_dword ...) in ModbusClientData
 *
 * @param  pData pointer no client data
 * @return true - ok, false - wrong type
 */
bool modbusToClientData(ModbusData *pData, DataType dataType, ClientData *pClientData)
{
	int numToCopy, servicePos, dataPos;
	uint16_t serviceData[MAX_RCV_DATA_LEN];


	pClientData->dataType = dataType;

	// Fill data according to type
	switch(dataType)
	{
		case MDT_BOOL:
			pClientData->data_bool = pData->data[0] != 0;
			break;

		case MDT_INT:

			numToCopy = sizeof(int) / SIZEOF_UINT16_T;

			if(IS_BIG_ENDIAN)
				for(servicePos = 0, dataPos = numToCopy - 1; servicePos < numToCopy; servicePos++, dataPos--)
					serviceData[servicePos] = pData->data[dataPos];
			else
				memcpy(&serviceData, &(pData->data), numToCopy * SIZEOF_UINT16_T);

			memcpy(&(pClientData->data_int), &serviceData, sizeof(int));
			break;

		case MDT_DWORD:

			numToCopy = sizeof(DWORD) / SIZEOF_UINT16_T;

			if(IS_BIG_ENDIAN)
				for(servicePos = 0, dataPos = numToCopy - 1; servicePos < numToCopy; servicePos++, dataPos--)
					serviceData[servicePos] = pData->data[dataPos];
			else
				memcpy(&serviceData, &(pData->data), numToCopy * SIZEOF_UINT16_T);

			memcpy(&(pClientData->data_dword), &serviceData, sizeof(DWORD));
			break;

		case MDT_DOUBLE:

			numToCopy = sizeof(double) / SIZEOF_UINT16_T;

			if(IS_BIG_ENDIAN)
				for(servicePos = 0, dataPos = numToCopy - 1; servicePos < numToCopy; servicePos++, dataPos--)
					serviceData[servicePos] = pData->data[dataPos];
			else
				memcpy(&serviceData, &(pData->data), numToCopy * SIZEOF_UINT16_T);

			memcpy(&(pClientData->data_dword), &serviceData, sizeof(double));
			break;

		case MDT_TIME:

			numToCopy = sizeof(time_t) / SIZEOF_UINT16_T;

			if(IS_BIG_ENDIAN)
				for(servicePos = 0, dataPos = numToCopy - 1; servicePos < numToCopy; servicePos++, dataPos--)
					serviceData[servicePos] = pData->data[dataPos];
			else
				memcpy(&serviceData, &(pData->data), numToCopy * SIZEOF_UINT16_T);

			memcpy(&(pClientData->data_time), &serviceData, sizeof(time_t));

			break;

		case MDT_ENUM:

			numToCopy = sizeof(int) / SIZEOF_UINT16_T;

			if(IS_BIG_ENDIAN)
				for(servicePos = 0, dataPos = numToCopy - 1; servicePos < numToCopy; servicePos++, dataPos--)
					serviceData[servicePos] = pData->data[dataPos];
			else
				memcpy(&serviceData, &(pData->data), numToCopy * SIZEOF_UINT16_T);

			memcpy(&(pClientData->data_enum), &serviceData, sizeof(int));

			break;

		default:
			LOG_E("Wrong data type");
			return false;
	}

	return true;
}

/**
 * Init data from config file
 *
 * @param  config_t config file
 * @return true - ok, false - error
 */
bool manager_init_config(config_t cfg)
{
	int i;
	const char       *pDataType;
	const char       *pModbusType;
	const char       *pRefreshRate;
	const char       *pProtocol;
	const char       *pCounterType;
	const char       *pComputationType;
	const char       *pOpcSecure;
	const char       *pOpcSecurityMode;
	const char       *pOpcServerName;
	config_setting_t *opcConf;
	config_setting_t *opcServerConf;
	config_setting_t *clientsConf;
	config_setting_t *currClientConf;
	config_setting_t *commonConf;
	config_setting_t *commonDataConf;
	bool opcSettings = false;
	bool opcIsSecure = false;
	OpcSecurityMode opcSecurityMode;

	// Load common settings
	commonConf = config_lookup(&cfg, "common");
	if(commonConf != NULL)
	{
		commonDataConf = config_setting_get_elem(commonConf, 0);

		// Modbus settings
		if(config_setting_lookup_int(commonDataConf, "send_interval_min", &_sendInterval) != CONFIG_TRUE){
			LOG_E("Wrong parameter send_interval_min, section: common");
			return false;
		}
	}
	else
	{
		LOG_E("No section: common");
		return false;
	}

	// Load OPC UA Server Settings
	opcConf = config_lookup(&cfg, "opc_ua");
	if(opcConf != NULL)
	{
		opcServerConf = config_setting_get_elem(opcConf, 0);

		if(config_setting_lookup_string(opcServerConf, "server", &pOpcServerName) != CONFIG_TRUE){
			LOG_E("Wrong parameter: server, section: opc_ua");
			return false;
		}

		if(config_setting_lookup_string(opcServerConf, "secure", &pOpcSecure) != CONFIG_TRUE){
			LOG_E("Wrong parameter: secure, section: opc_ua");
			return false;
		}

		if(config_setting_lookup_string(opcServerConf, "security_mode", &pOpcSecurityMode) != CONFIG_TRUE){
			LOG_E("Wrong parameter: security_mode, section: opc_ua");
			return false;
		}

		// Parse secure
		opcIsSecure = (strcmp(pOpcSecure, "Y") == 0);

		// Parse security mode
		if(strcmp(pOpcSecurityMode, "none") == 0)
			opcSecurityMode = OSM_NONE;
		else if(strcmp(pOpcSecurityMode, "sign") == 0)
			opcSecurityMode = OSM_SIGN;
		else if(strcmp(pOpcSecurityMode, "sign_encrypt") == 0)
			opcSecurityMode = OSM_SIGN_ENCRYPT;
		else
		{
			LOG_E("Wrong security mode: %s, section: opc_ua",
					pOpcSecurityMode);
			return false;
		}

		opcSettings = true;
	}

	// Load Clients settings
	clientsConf = config_lookup(&cfg, "clients");

	if(clientsConf == NULL)
	{
		LOG_E("No client settings");
		return false;
	}

	_clientsCnt = config_setting_length(clientsConf);

	if(_clientsCnt == 0)
	{
		LOG_E("No clients in settings file");
		return false;
	}

	if(_clientsCnt > MAX_CLIENT_NUM)
	{
		LOG_E("Too many clients, got %d, max %d", _clientsCnt, MAX_CLIENT_NUM);

		return false;
	}

	// Init all clients
	for(i = 0; i < _clientsCnt; i++)
	{
		currClientConf = config_setting_get_elem(clientsConf, i);

		// Common settings
		_clientSettings[i].innerIdx = i;
		if(config_setting_lookup_string(currClientConf, "name",  &_clientSettings[i].name) != CONFIG_TRUE){
			LOG_E("Wrong parameter name, client counter %d", i);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "protocol", &pProtocol) != CONFIG_TRUE){
			LOG_E("Wrong parameter protocol, client: %s", _clientSettings[i].name);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "refresh_rate", &pRefreshRate) != CONFIG_TRUE){
			LOG_E("Wrong parameter refresh_rate, client: %s", _clientSettings[i].name);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "data_type", &pDataType) != CONFIG_TRUE){
			LOG_E("Wrong parameter data_type, client: %s", _clientSettings[i].name);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "unit", &_clientSettings[i].unit) != CONFIG_TRUE){
			LOG_E("Wrong parameter unit, client: %s", _clientSettings[i].name);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "counter", &pCounterType) != CONFIG_TRUE){
			LOG_E("Wrong parameter counter, client: %s", _clientSettings[i].name);
			return false;
		}

		if(config_setting_lookup_string(currClientConf, "computation", &pComputationType) != CONFIG_TRUE){
			LOG_E("Wrong parameter computation, client: %s", _clientSettings[i].name);
			return false;
		}

		if(strlen(_clientSettings[i].name) > MAX_CLIENT_NAME_LEN)
		{
			LOG_E("Client name too big, max %d", MAX_CLIENT_NAME_LEN);
				return false;
		}

		// Parse refresh rate
		if(strcmp(pRefreshRate, "100ms") == 0)
			_clientSettings[i].refreshRate = RR_100ms;
		else if(strcmp(pRefreshRate, "1s") == 0)
			_clientSettings[i].refreshRate = RR_1s;
		else if(strcmp(pRefreshRate, "1m") == 0)
			_clientSettings[i].refreshRate = RR_1m;
		else
		{
			LOG_E("Wrong refresh rate: %s, client: %s",
					pRefreshRate, _clientSettings[i].name);
			return false;
		}

		// Parse data type
		if (strcmp(pDataType, "int") == 0){
			_clientSettings[i].dataType = MDT_INT;
		}
		else if (strcmp(pDataType, "dword") == 0) {
			_clientSettings[i].dataType = MDT_DWORD;
		}
		else if (strcmp(pDataType, "double") == 0) {
			_clientSettings[i].dataType = MDT_DOUBLE;
		}
		else if (strcmp(pDataType, "bool") == 0) {
			_clientSettings[i].dataType = MDT_BOOL;
		}
		else if (strcmp(pDataType, "time") == 0) {
			_clientSettings[i].dataType = MDT_TIME;
		}
		else if (strcmp(pDataType, "enum") == 0) {
			_clientSettings[i].dataType = MDT_ENUM;
		}
		else if (strcmp(pDataType, "char") == 0) {
			_clientSettings[i].dataType = MDT_CHAR;
		}
		else
		{
			LOG_E("Wrong data type: %s, client: %s",
				  pDataType, _clientSettings[i].name);
			return false;
		}

		// Parse protocol
		if(strcmp(pProtocol, "modbus") == 0)
		{
			_clientSettings[i].protocol = CP_MODBUS;
		}
		else if(strcmp(pProtocol, "opc") == 0)
		{
			_clientSettings[i].protocol = CP_OPC;
		}
		else
		{
			LOG_E("Wrong protocol type: %s, client: %s",
					pProtocol, _clientSettings[i].name);
			return false;
		}

		// Parse counter type
		if (strcmp(pCounterType, "none") == 0){
			_clientSettings[i].counter = CT_NONE;
		}
		else if (strcmp(pCounterType, "difference") == 0) {
			_clientSettings[i].counter = CT_DIFFERENCE;
		}
		else if (strcmp(pCounterType, "cumulated_not_100") == 0) {
			_clientSettings[i].counter = CT_CUMUL_NOT_100;
		}
		else if (strcmp(pCounterType, "cumulated") == 0) {
			_clientSettings[i].counter = CT_CUMUL;
		}
		else
		{
			LOG_E("Wrong counter type: %s, client: %s",
					pCounterType, _clientSettings[i].name);
			return false;
		}

		// Parse computation type
		if (strcmp(pComputationType, "none") == 0){
			_clientSettings[i].computation = LCT_NONE;
		}
		else if (strcmp(pComputationType, "mul_60") == 0) {
			_clientSettings[i].computation = LCT_MUL_60;
		}
		else if (strcmp(pComputationType, "mul_60000000") == 0) {
			_clientSettings[i].computation = LCT_MUL_60000000;
		}
		else if (strcmp(pComputationType, "mul_1000") == 0) {
			_clientSettings[i].computation = LCT_MUL_1000;
		}
		else
		{
			LOG_E("Wrong computation type: %s, client: %s",
					pCounterType, _clientSettings[i].name);
			return false;
		}

		// Protocol specific
		if(_clientSettings[i].protocol == CP_MODBUS)
		{
			// Modbus settings
			if(config_setting_lookup_int(currClientConf,    "mb_id",      &_clientSettings[i].modbus.id) != CONFIG_TRUE){
				LOG_E("Wrong parameter mb_id, client: %s", _clientSettings[i].name);
				return false;
			}

			if(config_setting_lookup_int(currClientConf,    "mb_offset",  &_clientSettings[i].modbus.offset) != CONFIG_TRUE){
				LOG_E("Wrong parameter mb_offset, client: %s", _clientSettings[i].name);
				return false;
			}

			if(config_setting_lookup_string(currClientConf, "mb_type",    &pModbusType) != CONFIG_TRUE){
				LOG_E("Wrong parameter mb_type, client: %s", _clientSettings[i].name);
				return false;
			}

			// Parse modbus type
			if(strcmp(pModbusType, "TCP") == 0)
			{
				_clientSettings[i].modbus.protocolType = MPT_TCP;
				if(config_setting_lookup_int(currClientConf,    "mb_port", &_clientSettings[i].modbus.port) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_port, client: %s", _clientSettings[i].name);
					return false;
				}

				if(config_setting_lookup_string(currClientConf, "mb_ip",   &_clientSettings[i].modbus.ipAdress) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_ip, client: %s", _clientSettings[i].name);
					return false;
				}
			}
			else if(strcmp(pModbusType, "RTU") == 0)
			{
				_clientSettings[i].modbus.protocolType = MPT_RTU;
				if(config_setting_lookup_string(currClientConf, "mb_device",    &_clientSettings[i].modbus.device) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_device, client: %s", _clientSettings[i].name);
					return false;
				}

				if(config_setting_lookup_string(currClientConf, "mb_parity",    &_clientSettings[i].modbus.parity) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_parity, client: %s", _clientSettings[i].name);
					return false;
				}

				if(config_setting_lookup_int(currClientConf,    "mb_baud_rate", &_clientSettings[i].modbus.baudRate) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_baud_rate, client: %s", _clientSettings[i].name);
					return false;
				}

				if(config_setting_lookup_int(currClientConf,    "mb_data_bit",  &_clientSettings[i].modbus.dataBit) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_data_bit, client: %s", _clientSettings[i].name);
					return false;
				}

				if(config_setting_lookup_int(currClientConf,    "mb_stop_bit",  &_clientSettings[i].modbus.stopBit) != CONFIG_TRUE){
					LOG_E("Wrong parameter mb_stop_bit, client: %s", _clientSettings[i].name);
					return false;
				}
			}

			// Parse data type
			if (_clientSettings[i].dataType == MDT_INT){
				_clientSettings[i].modbus.registersToRead = 2;
			}
			else if (_clientSettings[i].dataType == MDT_DWORD) {
				_clientSettings[i].modbus.registersToRead = 4;
			}
			else if (_clientSettings[i].dataType == MDT_DOUBLE) {
				_clientSettings[i].modbus.registersToRead = 4;
			}
			else if (_clientSettings[i].dataType == MDT_BOOL) {
				_clientSettings[i].modbus.registersToRead = 1;
			}
			else if (_clientSettings[i].dataType == MDT_TIME) {
				_clientSettings[i].modbus.registersToRead = 4;
			}
			else if (_clientSettings[i].dataType == MDT_ENUM) {
				_clientSettings[i].modbus.registersToRead = 2;
			}
			else
			{
				LOG_E("Wrong data type for modbus, client: %s",
					  pDataType, _clientSettings[i].name);
				return false;
			}

			// Offset
			//_clientSettings[i].modbus.offset = 0; // Default for all clients
		}
		else if(_clientSettings[i].protocol == CP_OPC)
		{
			if(!opcSettings)
			{
				LOG_E("client: %s protocol = opc, but no opc settings in file", _clientSettings[i].name);
				return false;
			}

			if(config_setting_lookup_int(currClientConf,    "opc_ns",     &_clientSettings[i].opc.nameSpace) != CONFIG_TRUE){
				LOG_E("Wrong parameter opc_ns, client: %s", _clientSettings[i].name);
				return false;
			}

			if(config_setting_lookup_string(currClientConf, "opc_node",   &_clientSettings[i].opc.node) != CONFIG_TRUE){
				LOG_E("Wrong parameter opc_node, client: %s", _clientSettings[i].name);
				return false;
			}

			_clientSettings[i].opc.serverName   = pOpcServerName;
			_clientSettings[i].opc.secure       = opcIsSecure;
			_clientSettings[i].opc.securityMode = opcSecurityMode;
			_clientSettings[i].opc.dataType     = _clientSettings[i].dataType;
		}
	}

	return true;
}

/**
 * Connect to all clients
 *
 * @return true - ok, false - error
 */
bool manager_init_connections()
{
	int i;
	bool allClientsConnected = true;
	bool opcFirstConnection  = true;
	bool opcConnected = false;

	// For all clients
	for(i = 0; i < _clientsCnt; i++)
	{
		if(_clientSettings[i].protocol == CP_MODBUS)
		{
			// Connect
			if(modbusConnect(&(_clientSettings[i].modbus)))
			{
				_clientSettings[i].connected = true;
				LOG("Client: %s, new modbus connection",_clientSettings[i].name);
			}
			else
			{
				_clientSettings[i].connected = false;
				allClientsConnected = false;

				if(_clientSettings[i].modbus.protocolType == MPT_TCP)
				{
					LOG_E("Client: %s, connection FAIL, for ip: %s, port %d",
							_clientSettings[i].name, _clientSettings[i].modbus.ipAdress, _clientSettings[i].modbus.port);
				}
				else
				{
					LOG_E("Client: %s, connection FAIL, device: %s, baud: %d",
							_clientSettings[i].name, _clientSettings[i].modbus.device, _clientSettings[i].modbus.baudRate);
				}

				continue;
			}
		}
		else if(_clientSettings[i].protocol == CP_OPC)
		{
			if(opcFirstConnection)
			{
				opcFirstConnection = false;

				if(opcConnect(_clientSettings[i].opc.serverName,
						      _clientSettings[i].opc.secure,
						      _clientSettings[i].opc.securityMode,
						      OPC_APP_URI,
						      OPC_CERT_PATH,
						      OPC_KEY_PATH))
				{
					opcConnected = true;
					LOG("Client: %s, connected OK",_clientSettings[i].name);
				}
				else
				{
					opcConnected = false;
					allClientsConnected = false;
					LOG_E("Client: %s, connection FAIL", _clientSettings[i].name);
				}
			}

			_clientSettings[i].connected = opcConnected;
		}
		else
		{
			LOG_E("Wrong protocol, client: %s", _clientSettings[i].name);
				return false;
		}
	}

	if(!allClientsConnected)
		return false;

	return true;
}

/**
 * Get client ptr
 *
 * @param InnerIdx idx of client
 * @return ptr - ok, NULL - error
 */
const ClientSettings* manager_get_client(InnerIdx innerIdx)
{
	if((innerIdx < 0) || (innerIdx > _clientsCnt-1))
		return NULL;

	return &(_clientSettings[innerIdx]);
}

/**
 * Receive data from client
 *
 * @param InnerIdx idx of client
 * @param ClientData receiving data
 * @return true - ok, false - error
 */
bool manager_receive_data_simple(InnerIdx innerIdx, ClientData *pClientData)
{
	if((innerIdx < 0) || (innerIdx > _clientsCnt-1))
	{
		LOG_E("Wrong innerIdx: %d, min 0, max %d, client: %s",
				innerIdx, _clientsCnt-1, _clientSettings[innerIdx].name);
			return false;
	}

	if(!_clientSettings[innerIdx].connected)
	{
		LOG_E("Client: %s not connected, need reconnect", _clientSettings[innerIdx].name);
		return false;
	}

	if(_clientSettings[innerIdx].protocol == CP_MODBUS)
	{
		ModbusData data;

		// Receive data
		if(modbusReceiveData(&(_clientSettings[innerIdx].modbus), &data))
		{
			// Convert data
			if(!modbusToClientData(&data, _clientSettings[innerIdx].dataType, pClientData))
			{
				LOG_E("Can't convert data, client: %s", _clientSettings[innerIdx].name);
				return false;
			}
		}
		else
		{
			LOG_E("Can't receive data, client: %s", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, false);
			return false;
		}
	}
	else if(_clientSettings[innerIdx].protocol == CP_OPC)
	{
		if(!opcReceiveData(&(_clientSettings[innerIdx].opc), pClientData))
		{
			LOG_E("Can't receive data, client: %s", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, false);
			return false;
		}
	}
	else
	{
		LOG_E("Wrong protocol");
		return false;
	}

	return true;
}

/**
 * Reconnect to client
 *
 * @param InnerIdx idx of client
 * @return true - ok, false - error
 */
bool manager_reconnect(InnerIdx innerIdx)
{
	if((innerIdx < 0) || (innerIdx > _clientsCnt-1))
	{
		LOG_E("Wrong innerIdx: %d, min 0, max %d, client: %s",
				innerIdx, _clientsCnt-1, _clientSettings[innerIdx].name);
			return false;
	}

	// Reconnect
	if(_clientSettings[innerIdx].protocol == CP_MODBUS)
	{
		if( modbusReconnect(&(_clientSettings[innerIdx].modbus)) )
		{
			LOG("client: %s, modbus reconnected", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, true);
		}
		else
		{
			LOG_E("Can't reconnect, client: %s", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, false);
			return false;
		}

	}
	else if(_clientSettings[innerIdx].protocol == CP_OPC)
	{
		opcCloseConnection();
		if(opcConnect(_clientSettings[innerIdx].opc.serverName,
				      _clientSettings[innerIdx].opc.secure,
				      _clientSettings[innerIdx].opc.securityMode,
				      OPC_APP_URI,
				      OPC_CERT_PATH,
				      OPC_KEY_PATH))
		{
			LOG("client: %s, reconnected OK", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, true);
		}
		else
		{
			LOG_E("Can't reconnect, client: %s", _clientSettings[innerIdx].name);
			setConnectStatus(innerIdx, false);

			return false;
		}
	}
	else
	{
		LOG_E("Wrong protocol");
		return false;
	}

	return true;
}

/**
 * Receive data from client, reconnect if error
 *
 * @param InnerIdx idx of client
 * @param ClientData receiving data
 * @return true - ok, false - error
 */
bool manager_receive_data(InnerIdx innerIdx, ClientData *pClientData)
{
	if((innerIdx < 0) || (innerIdx > _clientsCnt-1))
	{
		LOG_E("Wrong innerIdx: %d, min 0, max %d, client: %s",
				innerIdx, _clientsCnt-1, _clientSettings[innerIdx].name);
			return false;
	}

	// Check client connection state
	if(!_clientSettings[innerIdx].connected)
	{
		LOG_E("Client: %s not connected, try to reconnect", _clientSettings[innerIdx].name);

		// Reconnect
		if(!manager_reconnect(innerIdx))
		{
			LOG_E("Reconnection error, client: %s", _clientSettings[innerIdx].name);
			return false;
		}
	}

	// Receive data
	if(!manager_receive_data_simple(innerIdx, pClientData))
	{
		LOG_E("Error receiving data, client: %s, try to reconnect", _clientSettings[innerIdx].name);
		setConnectStatus(innerIdx, false);

		// Reconnect
		if(!manager_reconnect(innerIdx))
		{
			LOG_E("Reconnection error, client: %s", _clientSettings[innerIdx].name);
			return false;
		}

		// Receive data
		if(!manager_receive_data_simple(innerIdx, pClientData))
		{
			LOG_E("Error receiving data, client: %s", _clientSettings[innerIdx].name);
			return false;
		}

		setConnectStatus(innerIdx, true);
	}

	return true;
}

/**
 * Close all connections
 *
 */
void manager_close_all_connections()
{
	int i;
	bool opcClose = false;

	// For all clients
	for(i = 0; i < _clientsCnt; i++)
	{
		if(_clientSettings[i].protocol == CP_MODBUS)
		{
			modbusCloseConnection(&(_clientSettings[i].modbus));
		}
		else if(_clientSettings[i].protocol == CP_OPC)
		{
			if(!opcClose)
			{
				opcCloseConnection();
				opcClose = true;
			}
		}
		else
		{
			LOG_E("Wrong protocol");
		}

		_clientSettings[i].connected = false;
	}
}

/**
 * Get interval to send data
 *
 * @return interval in minutes
 */
int manager_get_send_interval()
{
	if(_sendInterval <= 0)
	{
		LOG_E("Wrong _sendInterval: %d, need reinit", _sendInterval);
	}

	return _sendInterval;
}

/**
 * Get number of clients
 *
 * @return number of clients
 */
int manager_get_clients_count()
{
	return _clientsCnt;
}
