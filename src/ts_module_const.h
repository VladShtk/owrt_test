#ifndef TS_MODULE_CONST_H
#define TS_MODULE_CONST_H

#include <time.h>

#define MAX_BUF 1024 * 3

#define SYSLOG

// Data alias
#define TIME 0
#define TEMP 1
#define TYPE 2

/**
 * Data type
 */

#define	TS_BOOL    1		// Boolean
#define	TS_INT     2		// Integer
#define	TS_DWORD   3		// Double word
#define	TS_LINT    4		// Long integer
#define	TS_DOUBLE  5		// Double
#define	TS_TIME    6		// Time
#define	TS_ENUM    7		// Enumerate
#define	TS_CHAR    8		// *char

#define MAX_CLIENT_NUM      50  // Max numbers of client, min 0 - max 2 147 483 647
#define MAX_CLIENT_NAME_LEN 80  // Max client name len
#define MAX_OPC_NODE_LEN    80  // Max opc ua node name len
#define MAX_MQTT_FIELDS     10  // Max fields in mqtt (json) data
#define MAX_OPC_STR_LEN     500  // Max len of opc ua string

#define OPC_CERT_PATH "/etc/ts_module/opc_client_cert.der"
#define OPC_KEY_PATH  "/etc/ts_module/opc_client_key.der"
#define OPC_APP_URI   "urn:ts_owrt_module"

typedef unsigned int DWORD;
typedef int InnerIdx;

/**
 * Refresh rate
 */
typedef enum
{
	RR_100ms,
	RR_1s,
	RR_1m
} RefreshRate;

/**
 * Data type
 */
typedef enum
{
	MDT_BOOL,		// Boolean
	MDT_INT,		// Integer
	MDT_DOUBLE,		// Double
	MDT_DWORD,		// Double word
	MDT_TIME,		// Time
	MDT_ENUM,		// Enumerate
	MDT_CHAR		// Char
} DataType;

/**
 * Client data
 */
typedef struct
{
	DataType dataType;		// Type of data

	bool     data_bool;		// Data in boolean format
	int      data_int;		// Data in integer format
	double   data_double;	// Data in double  format
	DWORD    data_dword;	// Data in double word format
	time_t   data_time;		// Data in time_t format
	int      data_enum;		// Data in enumeration format
	char     data_char[MAX_OPC_STR_LEN];// Data in char format
} ClientData;

#endif // TS_MODULE_CONST_H
