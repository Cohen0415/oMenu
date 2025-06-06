#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H

#define OMENU_VERSION                   "0.1.0"         // oMenu 版本号

#define OMENU_MAX_SELECTION 	        128             // 已选择的设备树插件数量的最大限值
#define OMENU_MAX_PATH 		            256             // 菜单路径的最大长度
#define OMENU_MAX_DTBO_SIZE             0x20000         // 设备树插件文本大小的最大限值

#define OMENU_STORE_DEV		            "mmc"           // 存储类型，目前支持mmc（如emmc、sd卡）
#define OMENU_SELECTED_FILE_NAME        "selected.txt"  // 已选择的设备树插件将保存在此文件中
#define OMENU_FS_TYPE                   FS_TYPE_FAT     // 文件系统类型，目前支持FAT

#define MAX_CFG_LEN 32      
typedef struct configs {
    char mmc_dev_num[MAX_CFG_LEN];
    char mmc_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
} configs_t;

#endif