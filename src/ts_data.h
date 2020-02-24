#include <stdbool.h>
#include <libconfig.h>
#include "ts_module_const.h"

/* Error values */
#define	DATA_ERR_SUCCESS 0
#define	DATA_ERR_CONF_LOOKUP 1
#define	DATA_ERR_COUNT 2
#define	DATA_ERR_JSON 3
#define	DATA_ERR_TYPE 4
#define	DATA_ERR_BUF 5

#define	DATA_MAX_LINE_LEN 100 // Max len of one line

typedef struct DataStruct
{
	const char *alias;
	// TSDataType type;
	int type;
	bool b_data;
	int i_data;
	long int l_data;
	double d_data;
	char ch_data[MAX_OPC_STR_LEN];
} t_data;

int init_count(int *count, config_t cfg);

int init_data(t_data data[], config_t cfg);

int get_json(char *buf, int buf_len, t_data data[], int count);
