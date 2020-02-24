#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <stdbool.h>
#include <libconfig.h>
#include "ts_module_const.h"
#include "modbus_connect.h"
#include "opc_connect.h"

/**
 * Client protocol
 */
typedef enum
{
	CP_MODBUS,
	CP_OPC
} ClientProtocol;

/**
 * Counter type
 */
typedef enum
{
	CT_NONE,
	CT_DIFFERENCE,
	CT_CUMUL_NOT_100,
	CT_CUMUL
} CounterType;

/**
 * Local computation type
 */
typedef enum
{
	LCT_NONE,
	LCT_MUL_60,
	LCT_MUL_60000000,
	LCT_MUL_1000,
} LocalCompType;

/**
 * Client info
 */
typedef struct
{
	const char     *name;  			// Name(Data Point, Variable)
	const char     *unit;			// Unit (h, kg, cm, m/s ...)
	bool           connected;		// Connected sign, changed automatically
	InnerIdx       innerIdx; 		// Inner index 0 - MAX_CLIENT_NUM
	RefreshRate    refreshRate;		// Time to refresh data
	ClientProtocol protocol;		// Communication protocol OPC or Modbus
	DataType       dataType;        // Type of receive data
	CounterType    counter;			// Counter for this client
	LocalCompType  computation;		// Local computation type
	ModbusClient   modbus;			// Modbus settings
	OpcClient      opc;				// OPC UA settings
} ClientSettings;

/**
 * MQTT Send Fields
 */
typedef enum
{
	//SF_TYPE = 0,
	//SF_EP   = 1,
	//SF_POT  = 2,
	//SF_TS   = 3,
	//SF_VER  = 4,

	SF_BAT  = 0,
	SF_SIG  = 1,
	SF_VER  = 2,
	SF_TS   = 3,
	SF_ID   = 4,
	SF_EP   = 5,
	SF_TYPE = 6,
	SF_LON  = 7,
	SF_LAT  = 8,
	SF_LOC  = 9,

	SF_CLIENT_POS // Must be last
} SendFields;

bool manager_init_config(config_t cfg);
bool manager_init_connections();
bool manager_reconnect(InnerIdx innerIdx);
bool manager_receive_data(InnerIdx innerIdx, ClientData *pClientData);
bool manager_receive_data_simple(InnerIdx innerIdx, ClientData *pClientData);
const ClientSettings* manager_get_client(InnerIdx innerIdx);
void manager_close_all_connections();
int manager_get_send_interval();
int manager_get_clients_count();

#endif //CLIENT_MANAGER_H
