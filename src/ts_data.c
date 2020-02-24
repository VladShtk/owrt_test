#include <string.h>
#include "ts_data.h"
#include "logger.h"

int init_count(int *count, config_t cfg)
{
	config_setting_t *data_conf;
	data_conf = config_lookup(&cfg, "data");

	if(data_conf != NULL)
	{
		*count = config_setting_length(data_conf);
		return DATA_ERR_SUCCESS;
	}
	return DATA_ERR_CONF_LOOKUP;
}

int init_data(t_data data[], config_t cfg)
{
	config_setting_t *data_conf;
	data_conf = config_lookup(&cfg, "data");

	if(data_conf != NULL)
	{
		int count = config_setting_length(data_conf);

		if (count == 0) return DATA_ERR_COUNT;

		char *type_alias;

		for(int i = 0; i < count; ++i)
		{
			config_setting_t *data_one = config_setting_get_elem(data_conf, i);

			config_setting_lookup_string(data_one, "alias", (const char**)&data[i].alias);
			config_setting_lookup_string(data_one, "type", (const char**)&type_alias);

			if (strcmp(type_alias, "bool") == 0) data[i].type = TS_BOOL;
			else if (strcmp(type_alias, "int") == 0) data[i].type = TS_INT;
			else if (strcmp(type_alias, "long int") == 0) data[i].type = TS_LINT;
			else if (strcmp(type_alias, "double") == 0) data[i].type = TS_DOUBLE;
			else if (strcmp(type_alias, "*char") == 0) data[i].type = TS_CHAR;
			else return DATA_ERR_TYPE;

		}

		return DATA_ERR_SUCCESS;
	}
	else return DATA_ERR_CONF_LOOKUP;
	
}

int get_json(char *buf, int buf_len, t_data data[], int count)
{
	int length = 0;
	length += sprintf (buf+length, "{");

	for(int i = 0; i < count; i++)
	{
		switch (data[i].type)
		{
			case TS_BOOL:
				length += sprintf (buf+length, "\"%s\":%d,", data[i].alias, data[i].b_data ? 1 : 0);
				break;

			case TS_INT:
				length += sprintf (buf+length, "\"%s\":%d,", data[i].alias, data[i].i_data);
				break;

			case TS_LINT:
				length += sprintf (buf+length, "\"%s\":%ld,", data[i].alias, data[i].l_data);
				break;

			case TS_DOUBLE:
				length += sprintf (buf+length, "\"%s\":\"%f\",", data[i].alias, data[i].d_data);
				break;

			case TS_CHAR:
				length += sprintf (buf+length, "\"%s\":\"%s\",", data[i].alias, data[i].ch_data);
				break;

			default: return 3;
		}

		if((length >= buf_len - DATA_MAX_LINE_LEN) && (i + 1 < count))
		{
			// Next line can overflow buffer
			LOG("Too small buff, got %d, need %d", buf_len, buf_len + DATA_MAX_LINE_LEN);
			return DATA_ERR_BUF;
		}
	}

	length += sprintf (buf+length-1, "}");
	return DATA_ERR_SUCCESS;
}
