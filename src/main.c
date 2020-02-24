#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <libconfig.h>
#include "mqtt_connect.h"
#include "modbus_connect.h"
#include "client_manager.h"
#include "client_queue.h"
#include "counters.h"
#include "ts_data.h"
#include "timer.h"
#include "logger.h"

#define INIT_CFG 0
#define INIT_SYS 1
#define PROCESS 2
#define SEND_DATA 3

// Global buf to send mqtt data
char send_buf[MAX_BUF];

long int power_on_time;

typedef struct DeviceStatus
{
	int  state;		// set device process status
	bool timer;		// t - timer enable
	bool mqtt;		// t - MQTT init
	bool manager;	// t - Manager init
} device_status;

int send_data(t_data data[], int count)
{
	// sprintf(buf,"\nUnix time: %d",data.time);
	// int rc = mqtt_send(buf, "temp");

	//Temporary solution
	get_json(send_buf, MAX_BUF, data, count);
	LOG("%s", send_buf);

	int rc = mqtt_send(send_buf);
		
	return rc;
}

int init_sys(device_status *dev_status, config_t cfg) {

	int rc = 0;
	int i;
	const ClientSettings *pClientSettings;

	timer_stop();
	dev_status->timer = false;

	/* Init MQTT */
	if (!dev_status->mqtt) {
		rc = mqtt_init(cfg);
		if (rc == 0) dev_status->mqtt = true;
	}

	/* Init Clients Manager and Queue */
	if (!dev_status->manager) {

		if(!manager_init_config(cfg))
		{
			LOG_E("Manager ERROR: Wrong config");
			return -1;
		}

		if(!manager_init_connections())
			LOG_E("Manager ERROR: Connection fail");

		// Init queue
		queueCleen();
		for(i = 0; i < MAX_CLIENT_NUM; i++)
		{
			pClientSettings = manager_get_client(i);
			if(pClientSettings == NULL)
				break; // All clients added

			if(!queueAdd(pClientSettings->refreshRate, pClientSettings->innerIdx))
			{
				LOG_E("Queue ERROR: Can't add client: %s", pClientSettings->name);
				return -1;
			}

			// Init counter
			counterAdd(pClientSettings);
		}

		dev_status->manager = true;
	}

	/* Init Timer after all */
	if (!dev_status->timer && dev_status->manager && dev_status->mqtt) {
		timer_init();
		rc = timer_start(TIMER_100mc);
		if (rc == 0) dev_status->timer = true;
	}

	/* If all ok - change the status */
	if (dev_status->timer && dev_status->manager && dev_status->mqtt) {
		LOG("Init finished");
		dev_status->state = PROCESS;
	/* else wait and repeat */
	} else {
		LOG("Wait init");
		sleep(5);
	}

	return 0;
}

/**
 * Convert client data to mqtt t_data
 *
 * @param  pClient     - client ptr
 * @param  pClientData - client data ptr
 * @param  data        - result data
 * @return true - ok, false - error
 */
bool client_to_mqtt(const ClientSettings *pClient, ClientData *pClientData, t_data *pDdata)
{
	pDdata->alias = pClient->name;

	if(pClientData->dataType == MDT_INT)
	{
		LOG_D("Int data: %d", pClientData->data_int);

		pDdata->type   = TS_INT;
		pDdata->i_data = pClientData->data_int;
	}
	else if(pClientData->dataType == MDT_BOOL)
	{
		LOG_D("Bool data: %s", pClientData->data_bool ? "true":"false");

		pDdata->type   = TS_BOOL;
		pDdata->b_data = pClientData->data_bool;
	}
	else if(pClientData->dataType == MDT_DWORD)
	{
		LOG_D("Dword data: %d", pClientData->data_dword);
		LOG_E("NOT REALIZED");

		pDdata->type   = TS_LINT;
		pDdata->l_data = pClientData->data_dword; // svv change to dword
	}
	else if(pClientData->dataType == MDT_DOUBLE)
	{
		LOG_D("Double data: %f", pClientData->data_double);

		pDdata->type   = TS_DOUBLE;
		pDdata->d_data = pClientData->data_double;
	}
	else if(pClientData->dataType == MDT_TIME)
	{
		LOG_D("Time data: %s", ctime (&pClientData->data_time));
		LOG_E("NOT REALIZED");

		pDdata->type   = TS_LINT;
		pDdata->l_data = pClientData->data_time; // svv change to time
	}
	else if(pClientData->dataType == MDT_ENUM)
	{
		LOG_D("Enum (int) data: %d", pClientData->data_enum);
		LOG_E("NOT REALIZED");

		pDdata->type   = TS_LINT;
		pDdata->l_data = (int)pClientData->data_enum; // svv change to enum
	}
	else if(pClientData->dataType == MDT_CHAR)
	{
		LOG_D("Char data: %s", pClientData->data_char);

		pDdata->type    = TS_CHAR;
		strncpy(pDdata->ch_data, pClientData->data_char, MAX_OPC_STR_LEN);
	}
	else
	{
		LOG_E("Wrong data type: %d", (int)pClientData->dataType);
		return false;
	}

	return true;
}

void set_mqtt_dynamic_fields(t_data data[])
{
	data[SF_BAT].alias   = "bat";
	data[SF_BAT].type    = TS_INT;
	data[SF_BAT].i_data  = 95;

	data[SF_SIG].alias   = "sig";
	data[SF_SIG].type    = TS_INT;
	data[SF_SIG].i_data  = 25;

	//data[SF_FLR].alias   = "flr";
	//data[SF_FLR].type    = TS_INT;
	//data[SF_FLR].i_data  = 5;

	//data[SF_SNR].alias   = "snr";
	//data[SF_SNR].type    = TS_CHAR;
	//strncpy(data[SF_SNR].ch_data, "FHG2213", MAX_OPC_STR_LEN);

	data[SF_TS].alias   = "ts";
	data[SF_TS].type    = TS_LINT;
	data[SF_TS].l_data  = time(NULL);

	data[SF_ID].alias   = "id";
	data[SF_ID].type    = TS_INT;
	data[SF_ID].i_data  = 0;

	/*
	long int currTime;

	time(&currTime);

	data[SF_POT].alias  = "pot";
	data[SF_POT].type   = TS_LINT;
	data[SF_POT].l_data = currTime - power_on_time;

	data[SF_TS].alias   = "ts";
	data[SF_TS].type    = TS_LINT;
	data[SF_TS].l_data  = currTime;
	*/
}

void set_mqtt_static_fields(t_data data[])
{
	data[SF_VER].alias     = "ver";
	data[SF_VER].type      = TS_CHAR;
	strncpy(data[SF_VER].ch_data, "0.0.1+linux", MAX_OPC_STR_LEN);

	data[SF_EP].alias      = "ep";
	data[SF_EP].type       = TS_CHAR;
	strncpy(data[SF_EP].ch_data, "868186040127613", MAX_OPC_STR_LEN);

	data[SF_TYPE].alias    = "type";
	data[SF_TYPE].type     = TS_CHAR;
	strncpy(data[SF_TYPE].ch_data, "full", MAX_OPC_STR_LEN);

	data[SF_LON].alias    = "lon";
	data[SF_LON].type     = TS_DOUBLE;
	data[SF_LON].d_data   = 18.0993;

	data[SF_LAT].alias    = "lat";
	data[SF_LAT].type     = TS_DOUBLE;
	data[SF_LAT].d_data   = 54.084;

	data[SF_LOC].alias    = "loc";
	data[SF_LOC].type     = TS_INT;
	data[SF_LOC].i_data   = 1;

	/*
	data[SF_TYPE].alias    = "type";
	data[SF_TYPE].type     = TS_CHAR;
	data[SF_TYPE].ch_data  = "full";
	*/
}

/**
 * Make computation
 *
 * @param  pClient     - client ptr
 * @param  pClientData - client data ptr
 * @return true - ok, false - error
 */
bool client_computation(const ClientSettings *pClient, ClientData *pClientData)
{
	if(pClient->computation == LCT_NONE)
		return true;

	if(pClientData->dataType == MDT_INT)
	{
		if(pClient->computation == LCT_MUL_60)
		{
			pClientData->data_int = (int)pClientData->data_int * 60;
		}
		else if(pClient->computation == LCT_MUL_60000000)
		{
			pClientData->data_int = (int)pClientData->data_int * 60000000;
		}
		else if(pClient->computation == LCT_MUL_1000)
		{
			pClientData->data_int = (int)pClientData->data_int * 1000;
		}
		else
		{
			LOG_E("Wrong computation type");
			return false;
		}
	}
	else if(pClientData->dataType == MDT_DWORD)
	{
		if(pClient->computation == LCT_MUL_60)
		{
			pClientData->data_dword = (DWORD)pClientData->data_dword * 60;
		}
		else if(pClient->computation == LCT_MUL_60000000)
		{
			pClientData->data_dword = (DWORD)pClientData->data_dword * 60000000;
		}
		else if(pClient->computation == LCT_MUL_1000)
		{
			pClientData->data_dword = (DWORD)pClientData->data_dword * 1000;
		}
		else
		{
			LOG_E("Wrong computation type");
			return false;
		}
	}
	else if(pClientData->dataType == MDT_DOUBLE)
	{
		if(pClient->computation == LCT_MUL_60)
		{
			pClientData->data_double = (double)pClientData->data_double * 60;
		}
		else if(pClient->computation == LCT_MUL_60000000)
		{
			pClientData->data_double = (double)pClientData->data_double * 60000000;
		}
		else if(pClient->computation == LCT_MUL_1000)
		{
			pClientData->data_double = (double)pClientData->data_double * 1000;
		}
		else
		{
			LOG_E("Wrong computation type");
			return false;
		}
	}
	else
	{
		LOG_E("Wrong data type no: %d for computation", (int)pClientData->dataType);
		return false;
	}

	return true;
}

int main(int argc, char *argv[])
{
	printf("\r\nStart\r\n");

	//First, need to init devices and systems
	device_status dev_status;
	dev_status.state   = INIT_SYS;
	dev_status.timer   = false;
	dev_status.mqtt    = false;
	dev_status.manager = false;

	int        mqtt_data_count = 0;
	int        sendDataMin; // Timer to send data
	int        clientsCount;
	bool       firstClient;
	ClientData clientData;
	InnerIdx   tempClientIdx;
	t_data     mqtt_data[SF_CLIENT_POS + MAX_CLIENT_NUM] = {'\0'};
	const ClientSettings *pClient;

	//Use log in /tmp
	logger_init("/etc/ts_module/ts_module_log.txt");
	LOG("\n**************************************************");

	/* Init config file & data */
	char *cfg_file = "/etc/ts_module/ts_module.cfg";
	if (argc > 1) cfg_file = argv[1];

	config_t cfg;
	config_init(&cfg);

	/* Read the file. If there is an error, report it and exit. */
	if(! config_read_file(&cfg, cfg_file))
	{
		LOG_E("%s:%d - %s", config_error_file(&cfg),
				config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return(EXIT_FAILURE);
	}

	time(&power_on_time);

	while(1) {

		switch (dev_status.state) {
			case INIT_SYS:
				/* Init system & devices */
				init_sys(&dev_status, cfg);

				if(dev_status.state != INIT_SYS) // If init ok
				{
					clientsCount = manager_get_clients_count();
					sendDataMin  = manager_get_send_interval();

					// Init global mqtt data
					set_mqtt_static_fields(mqtt_data);
					set_mqtt_dynamic_fields(mqtt_data);
					mqtt_data_count = (int)SF_CLIENT_POS + manager_get_clients_count();

					// Get data from all clients
					for(int tempClientIdx = 0; tempClientIdx < clientsCount; tempClientIdx++)
					{
						// Get Client
						pClient = manager_get_client(tempClientIdx);
						if(pClient == NULL)
						{
							LOG_E("Manager: Wrong client num %d", tempClientIdx);
							continue;
						}

						LOG_D("Read data, client: %s", pClient->name);

						// Receive data
						if(manager_receive_data(pClient->innerIdx, &clientData))
						{
							// Update counter
							if(!counterUpdate(pClient, &clientData))
								LOG_E("Can't update counter, client: %s", pClient->name);

							// Get counter val in clientData
							if(!counterGet(pClient, &clientData))
								LOG_E("Can't get counter val, client: %s", pClient->name);

							// Make computation
							if(!client_computation(pClient, &clientData))
								LOG_E("Can't make computation, client: %s", pClient->name);

							// Update data in mqtt_data
							if(!client_to_mqtt(pClient, &clientData, &mqtt_data[SF_CLIENT_POS + pClient->innerIdx]))
							{
								LOG_E("Can't convert data, client: %s", pClient->name);
								continue;
							}

							LOG_D("Receive OK, client %s", pClient->name);
						}
						else
						{
							// Receive error, set empty data
							memset(&clientData, '\0', sizeof(ClientData));
							clientData.dataType = pClient->dataType;

							// Update counter
							if(!counterUpdate(pClient, &clientData))
								LOG_E("Can't update counter, client: %s", pClient->name);

							// Get counter val in clientData
							if(!counterGet(pClient, &clientData))
								LOG_E("Can't get counter val, client: %s", pClient->name);

							// Update data in mqtt_data
							if(!client_to_mqtt(pClient, &clientData, &mqtt_data[SF_CLIENT_POS + pClient->innerIdx]))
							{
								LOG_E("Can't convert data, client: %s", pClient->name);
								continue;
							}

							LOG_E("Receive error, client %s", pClient->name);
						}
					}

					dev_status.state = SEND_DATA;
				}

				sleep(1);

			break;

			case PROCESS: 

				if (t_period.period_100ms) {

					firstClient = true;

					while(true)
					{
						// Get client ID
						if(firstClient)
						{
							tempClientIdx = queueGetFirst(RR_100ms);
							firstClient = false;
						}
						else
							tempClientIdx = queueGetNext(RR_100ms);

						if(tempClientIdx == -1)
							break; // All clients were surveyed

						// Get Client
						pClient = manager_get_client(tempClientIdx);
						if(pClient == NULL)
						{
							LOG_E("Manager: Wrong client num %d", tempClientIdx);
							continue;
						}

						LOG_D("Read data, client: %s", pClient->name);

						// Receive data
						if(manager_receive_data(pClient->innerIdx, &clientData))
						{
							// Update counter
							if(!counterUpdate(pClient, &clientData))
								LOG_E("Can't update counter, client: %s", pClient->name);

							// Get counter val in clientData
							if(!counterGet(pClient, &clientData))
								LOG_E("Can't get counter val, client: %s", pClient->name);

							// Make computation
							if(!client_computation(pClient, &clientData))
								LOG_E("Can't make computation, client: %s", pClient->name);

							// Update data in mqtt_data
							if(!client_to_mqtt(pClient, &clientData, &mqtt_data[SF_CLIENT_POS + pClient->innerIdx]))
							{
								LOG_E("Can't convert data, client: %s", pClient->name);
								continue;
							}

							LOG_D("Receive OK, client %s", pClient->name);
						}
						else
						{
							LOG_E("Receive error, client %s", pClient->name);
							continue;
						}
					}

					t_period.period_100ms = false;
				}

				if (t_period.period_1m) {

					// Process send data timer each minute
					sendDataMin--;
					if(sendDataMin <= 0)
					{
						dev_status.state = SEND_DATA;
						sendDataMin = manager_get_send_interval();
					}

					firstClient = true;

					while(true)
					{
						// Get client ID
						if(firstClient)
						{
							tempClientIdx = queueGetFirst(RR_1m);
							firstClient = false;
						}
						else
							tempClientIdx = queueGetNext(RR_1m);

						if(tempClientIdx == -1)
							break; // All clients were surveyed

						// Get Client
						pClient = manager_get_client(tempClientIdx);
						if(pClient == NULL)
						{
							LOG_E("Manager: Wrong client num %d", tempClientIdx);
							continue;
						}

						LOG_D("Read data, client: %s", pClient->name);

						// Receive data
						if(manager_receive_data(pClient->innerIdx, &clientData))
						{
							// Update counter
							if(!counterUpdate(pClient, &clientData))
								LOG_E("Can't update counter, client: %s", pClient->name);

							// Get counter val in clientData
							if(!counterGet(pClient, &clientData))
								LOG_E("Can't get counter val, client: %s", pClient->name);

							// Make computation
							if(!client_computation(pClient, &clientData))
								LOG_E("Can't make computation, client: %s", pClient->name);

							// Update data in mqtt_data
							if(!client_to_mqtt(pClient, &clientData, &mqtt_data[SF_CLIENT_POS + pClient->innerIdx]))
							{
								LOG_E("Can't convert data, client: %s", pClient->name);
								continue;
							}

							LOG_D("Receive OK, client %s", pClient->name);
						}
						else
						{
							LOG_E("Receive error, client %s", pClient->name);
							continue;
						}
					}

					t_period.period_1m = false;
				}

				if (t_period.period_1s) {

					firstClient = true;

					while(true)
					{
						// Get client ID
						if(firstClient)
						{
							tempClientIdx = queueGetFirst(RR_1s);
							firstClient = false;
						}
						else
							tempClientIdx = queueGetNext(RR_1s);

						if(tempClientIdx == -1)
							break; // All clients were surveyed

						// Get Client
						pClient = manager_get_client(tempClientIdx);
						if(pClient == NULL)
						{
							LOG_E("Manager: Wrong client num %d", tempClientIdx);
							continue;
						}

						LOG_D("Read data, client: %s", pClient->name);

						// Receive data
						if(manager_receive_data(pClient->innerIdx, &clientData))
						{
							// Update counter
							if(!counterUpdate(pClient, &clientData))
								LOG_E("Can't update counter, client: %s", pClient->name);

							// Get counter val in clientData
							if(!counterGet(pClient, &clientData))
								LOG_E("Can't get counter val, client: %s", pClient->name);

							// Make computation
							if(!client_computation(pClient, &clientData))
								LOG_E("Can't make computation, client: %s", pClient->name);

							// Update data in mqtt_data
							if(!client_to_mqtt(pClient, &clientData, &mqtt_data[SF_CLIENT_POS + pClient->innerIdx]))
							{
								LOG_E("Can't convert data, client: %s", pClient->name);
								continue;
							}

							LOG_D("Receive OK, client %s", pClient->name);
						}
						else
						{
							LOG_E("Receive error, client %s", pClient->name);
							continue;
						}
					}

					t_period.period_1s = false;
				}

			break;

			case SEND_DATA:

				// Send data

				/* Hard coded format
				{
				 "type" : "full",
				 "ep" : "<imei>",
				 "pot" : <power-on in seconds>,
				 "ts" : <unix timestamp>,
				 "ver" : "0.0.0-linux",
				 "tp0": ...
				}
				*/

				LOG_D("Send data");

				set_mqtt_dynamic_fields(mqtt_data);
				if(send_data(mqtt_data, mqtt_data_count) == 0)
				{
					// Reset counters for all clients
					for(int tempClientIdx = 0; tempClientIdx < clientsCount; tempClientIdx++)
					{
						// Get Client
						pClient = manager_get_client(tempClientIdx);
						if(pClient == NULL)
						{
							LOG_E("Manager: Wrong client num %d", tempClientIdx);
							continue;
						}

						counterReset(pClient);
					}
				}
				else
				{
					LOG_E("MQTT: Send error");
				}

				dev_status.state = PROCESS;

			break;

			default: usleep(100);
		}

		// usleep(100);
	}

	manager_close_all_connections();

	LOG("ts_owrt_module stopped");
	logger_deinit();
	
	// mosquitto_loop_forever(mosq, -1, 1);
	// mosquitto_destroy(mosq);
	// mosquitto_lib_cleanup();
	return 0;
}
