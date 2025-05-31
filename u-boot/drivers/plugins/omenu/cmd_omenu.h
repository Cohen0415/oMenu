#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H

#define MAX_SELECTION 	128
#define MAX_PATH 		256

#define STORE_DEV		"mmc"

#define MAX_CFG_LEN 32      
typedef struct configs {
    char mmc_dev_num[MAX_CFG_LEN];
    char mmc_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
} configs_t;

#endif