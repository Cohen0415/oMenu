#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H


#define MAX_CFG_LEN 32      
typedef struct configs {
    char mmc_dev_num[MAX_CFG_LEN];
    char mmc_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
}configs_t;

#define MAX_ENTRIES 64
typedef struct omenu_entry {
    char name[128];
    bool is_dir;
} omenu_entry_t;

#endif