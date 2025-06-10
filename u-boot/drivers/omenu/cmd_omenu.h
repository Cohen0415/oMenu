#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H

#define OMENU_VERSION                   "0.1.0"         // oMenu 版本号

#define OMENU_MAX_SELECTION 	        128             // 已选择的设备树插件数量的最大限值
#define OMENU_MAX_PATH 		            256             // 菜单路径的最大长度
#define OMENU_MAX_DTBO_SIZE             0x20000         // 设备树插件文本大小的最大限值

#define OMENU_DIR_FILE_NAME             "list.txt"      // 菜单目录列表文件名
#define OMENU_SELECTED_FILE_NAME        "selected.txt"  // 已选择的设备树插件将保存在此文件中
#define OMENU_FS_TYPE                   FS_TYPE_FAT     // 文件系统类型，目前支持FAT

#ifdef CONFIG_OMENU_STORAGE_MMC
#define OMENU_STORAGE_DEV "mmc"
#else
#ifdef CONFIG_OMENU_STORAGE_USB
#define OMENU_STORAGE_DEV "usb"
#else
#define OMENU_STORAGE_DEV "mmc"
#endif
#endif

#define MAX_CFG_LEN 32      
typedef struct configs {
    char stroage_type[MAX_CFG_LEN];  
    char stroage_dev_num[MAX_CFG_LEN];
    char stroage_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
} configs_t;

#endif