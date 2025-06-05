#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H

#define OMENU_MAX_SELECTION 	        128
#define OMENU_MAX_PATH 		            256

#define OMENU_STORE_DEV		            "mmc"
#define OMENU_SELECTED_FILE_NAME        "selected.txt"
#define OMENU_FS_TYPE                   FS_TYPE_FAT

#define MAX_CFG_LEN 32      
typedef struct configs {
    char mmc_dev_num[MAX_CFG_LEN];
    char mmc_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
} configs_t;

#endif