#include "counters.h"
#include "logger.h"

CounterInfo _counters[MAX_CLIENT_NUM] = {'\0'};

void counterAdd(const ClientSettings *pClient)
{
	_counters[pClient->innerIdx].pClient = pClient;
	if(pClient->counter == CT_CUMUL)
		_counters[pClient->innerIdx].cumulatedCounter = -1;
	//_counters[pClient->innerIdx].lastChangeTime = time(NULL);
}

bool counterUpdate(const ClientSettings *pClient, const ClientData *pClientData)
{
	unsigned long int tempULData;

	if(_counters[pClient->innerIdx].pClient == NULL)
	{
		LOG_E("Not initialized client: %s", pClient->name);
		return false;
	}

	// Process counters
	switch(pClient->counter)
	{
		case CT_NONE:

			break;

		case CT_DIFFERENCE:

			// Count data according to type
			switch(pClient->dataType)
			{
				case MDT_INT:

					if(pClientData->data_int > _counters[pClient->innerIdx].currentData.data_int)
					{
						tempULData = pClientData->data_int - _counters[pClient->innerIdx].currentData.data_int; // New data - old data
						_counters[pClient->innerIdx].differenceCounter = _counters[pClient->innerIdx].differenceCounter + tempULData;
					}

					break;

				case MDT_DWORD:

					if(pClientData->data_dword > _counters[pClient->innerIdx].currentData.data_dword)
					{
						tempULData = pClientData->data_dword - _counters[pClient->innerIdx].currentData.data_dword; // New data - old data
						_counters[pClient->innerIdx].differenceCounter = _counters[pClient->innerIdx].differenceCounter + tempULData;
					}

					break;

				default:
					LOG_E("Wrong data type for unsigned long int counter");
					return false;
			}

			break;

			case CT_CUMUL_NOT_100:

				// Count data according to type
				switch(pClient->dataType)
				{
					case MDT_INT:

						if(pClientData->data_int != 100)
						{
							_counters[pClient->innerIdx].cumulatedNot100Counter++;
						}

						break;

					case MDT_DWORD:

						if(pClientData->data_dword != 100)
						{
							_counters[pClient->innerIdx].cumulatedNot100Counter++;
						}

						break;

					default:
						LOG_E("Wrong data type for unsigned long int counter");
						return false;
				}

				break;


				case CT_CUMUL:

					if(_counters[pClient->innerIdx].cumulatedCounter == -1)
					{
						_counters[pClient->innerIdx].cumulatedCounter = 0;
						break;
					}

					// Count data according to type
					switch(pClient->dataType)
					{
						case MDT_INT:

							if(pClientData->data_int > _counters[pClient->innerIdx].currentData.data_int)
							{
								// _counters[pClient->innerIdx].cumulatedCounter++;

								tempULData = pClientData->data_int - _counters[pClient->innerIdx].currentData.data_int; // New data - old data
								_counters[pClient->innerIdx].cumulatedCounter = _counters[pClient->innerIdx].cumulatedCounter + tempULData;
							}

							break;

						case MDT_DWORD:

							if(pClientData->data_dword != _counters[pClient->innerIdx].currentData.data_dword)
							{
								// _counters[pClient->innerIdx].cumulatedCounter++;
								tempULData = pClientData->data_dword - _counters[pClient->innerIdx].currentData.data_dword; // New data - old data
								_counters[pClient->innerIdx].cumulatedCounter = _counters[pClient->innerIdx].cumulatedCounter + tempULData;
							}

							break;

						default:
							LOG_E("Wrong data type for unsigned long int counter");
							return false;
					}

					break;
	}

	_counters[pClient->innerIdx].currentData = *pClientData;

	/*
	if(memcmp(&(_counters[pClient->innerIdx].currentData), pClientData, sizeof(ClientData)))
	{
		time_t currentTime = time(NULL);

		_counters[pClient->innerIdx].currentData = *pClientData;
		_counters[pClient->innerIdx].lastChangeInterval = currentTime - _counters[pClient->innerIdx].lastChangeTime;
		_counters[pClient->innerIdx].lastChangeTime = currentTime;
	}
	*/

	return true;
}

bool counterGet(const ClientSettings *pClient, ClientData *pClientData)
{
	// Chose counter
	switch(pClient->counter)
	{
		case CT_NONE:

			*pClientData = _counters[pClient->innerIdx].currentData;
			break;

		case CT_DIFFERENCE:

			// Count data according to type
			switch(pClient->dataType)
			{
				case MDT_INT:

					(*pClientData).dataType = MDT_INT;
					(*pClientData).data_int = (int)_counters[pClient->innerIdx].differenceCounter;

					break;

				case MDT_DWORD:

					(*pClientData).dataType = MDT_DWORD;
					(*pClientData).data_dword = _counters[pClient->innerIdx].differenceCounter;

					break;

				default:
					LOG_E("Wrong data type for unsigned long int counter");
					return false;
			}

			break;

		case CT_CUMUL_NOT_100:

			// Count data according to type
			switch(pClient->dataType)
			{
				case MDT_INT:

					(*pClientData).dataType = MDT_INT;
					(*pClientData).data_int = (int)_counters[pClient->innerIdx].cumulatedNot100Counter;

					break;

				case MDT_DWORD:

					(*pClientData).dataType = MDT_DWORD;
					(*pClientData).data_dword = _counters[pClient->innerIdx].cumulatedNot100Counter;

					break;

				default:
					LOG_E("Wrong data type for unsigned long int counter");
					return false;
			}

			break;

		case CT_CUMUL:

			// Count data according to type
			switch(pClient->dataType)
			{
				case MDT_INT:

					(*pClientData).dataType = MDT_INT;
					(*pClientData).data_int = (int)_counters[pClient->innerIdx].cumulatedCounter;

					break;

				case MDT_DWORD:

					(*pClientData).dataType = MDT_DWORD;
					(*pClientData).data_dword = _counters[pClient->innerIdx].cumulatedCounter;

					break;

				default:
					LOG_E("Wrong data type for unsigned long int counter");
					return false;
			}

			break;
}

	return true;
}

void counterReset(const ClientSettings *pClient)
{
	if(pClient->counter == CT_DIFFERENCE)
	{
		_counters[pClient->innerIdx].differenceCounter = 0;
	}
	else if(pClient->counter == CT_CUMUL_NOT_100)
	{
		_counters[pClient->innerIdx].cumulatedNot100Counter = 0;
	}
	else if(pClient->counter == CT_CUMUL)
	{
		_counters[pClient->innerIdx].cumulatedCounter = 0;
	}
}
