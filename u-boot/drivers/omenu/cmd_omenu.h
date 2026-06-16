/*
 * Copyright (C) 2025 Cohen0415
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _CMD_OMENU_H
#define _CMD_OMENU_H

#define OMENU_VERSION "0.1.0" // oMenu 版本号

#define OMENU_MAX_SELECTION 128     // 已选择的设备树插件数量的最大限值
#define OMENU_MAX_PATH 256          // 菜单路径的最大长度
#define OMENU_MAX_DTBO_SIZE 0x20000 // 设备树插件文本大小的最大限值
#define OMENU_FDT_PAD_SIZE 0x2000   // 预留给 overlay merge 的额外空间

#define OMENU_DIR_FILE_NAME "list.txt"          // 菜单目录列表文件名
#define OMENU_SELECTED_FILE_NAME "/omenu/selected.txt" // 已选择的设备树插件将保存在此文件中
#define OMENU_FS_TYPE FS_TYPE_EXT               // 文件系统类型，目前支持FAT

#define OMENU_TARGET_PROP "target"
#define OMENU_TARGET_PATH_PROP "target-path"
#define OMENU_TARGET_UBOOT_PROP "target-uboot"
#define OMENU_OVERLAY_NODE_NAME "__overlay__"
#define OMENU_FIXUPS_NODE_PATH "/__fixups__"

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
typedef struct configs
{
    char stroage_type[MAX_CFG_LEN];
    char stroage_dev_num[MAX_CFG_LEN];
    char stroage_partition[MAX_CFG_LEN];
    char directory_name[MAX_CFG_LEN];
} configs_t;

int omenu_fdt_apply(void);
int omenu_uboot_fdt_apply(void);

#endif
