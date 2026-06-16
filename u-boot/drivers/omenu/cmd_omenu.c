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

#include <common.h>
#include <command.h>
#include <fs.h>
#include <malloc.h>
#include <stdio.h>
#include <console.h>
#include <cli.h>
#include <mapmem.h>
#include <fdt_support.h>
#include <linux/libfdt.h>
#include <asm/global_data.h>

#include "cmd_omenu.h"
#include "log_omenu.h"

DECLARE_GLOBAL_DATA_PTR;

// 保存选中状态
static char *selections[OMENU_MAX_SELECTION];
static int selection_count = 0;

static configs_t cfg;

static int omenu_prepare_fdt(void *fdt, const char *name)
{
    int ret;

    ret = fdt_increase_size(fdt, OMENU_FDT_PAD_SIZE);
    if (ret && ret != -FDT_ERR_NOSPACE)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to resize %s fdt: %s\n",
                  name, fdt_strerror(ret));
        return ret;
    }

    return 0;
}

static int omenu_is_string_prop(const void *prop, int len)
{
    if (!prop || len <= 0)
        return 0;

    return ((const char *)prop)[len - 1] == '\0';
}

static int omenu_rewrite_fixups_target_uboot(void *dtbo_buf)
{
    int fixups;
    int prop_off;
    int prop_count = 0;
    int i;
    int ret = 0;
    char *prop_names[OMENU_MAX_SELECTION];

    memset(prop_names, 0, sizeof(prop_names));

    fixups = fdt_path_offset(dtbo_buf, OMENU_FIXUPS_NODE_PATH);
    if (fixups < 0)
        return 0;

    prop_off = fdt_first_property_offset(dtbo_buf, fixups);
    while (prop_off >= 0)
    {
        const struct fdt_property *prop;
        const char *prop_name;
        int len;

        prop = fdt_get_property_by_offset(dtbo_buf, prop_off, &len);
        if (!prop)
        {
            ret = len;
            goto out;
        }

        if (prop_count >= OMENU_MAX_SELECTION)
        {
            ret = -ENOSPC;
            goto out;
        }

        prop_name = fdt_string(dtbo_buf, fdt32_to_cpu(prop->nameoff));
        prop_names[prop_count] = strdup(prop_name);
        if (!prop_names[prop_count])
        {
            ret = -ENOMEM;
            goto out;
        }
        prop_count++;

        prop_off = fdt_next_property_offset(dtbo_buf, prop_off);
    }

    for (i = 0; i < prop_count; i++)
    {
        const struct fdt_property *prop;
        const char *cursor;
        char *new_buf;
        char *dst;
        int len;
        int new_len = 0;

        prop = fdt_get_property(dtbo_buf, fixups, prop_names[i], &len);
        if (!prop)
            continue;

        cursor = prop->data;
        new_buf = memalign(4, len + 1);
        if (!new_buf)
        {
            ret = -ENOMEM;
            goto out;
        }

        dst = new_buf;
        while (cursor < ((const char *)prop->data + len))
        {
            const char *needle = strstr(cursor, ":" OMENU_TARGET_UBOOT_PROP ":");
            if (needle)
            {
                int prefix_len = needle - cursor;

                memcpy(dst, cursor, prefix_len);
                dst += prefix_len;
                memcpy(dst, ":" OMENU_TARGET_PROP ":", sizeof(":" OMENU_TARGET_PROP ":") - 1);
                dst += sizeof(":" OMENU_TARGET_PROP ":") - 1;
                strcpy(dst, needle + sizeof(":" OMENU_TARGET_UBOOT_PROP ":") - 1);
                dst += strlen(needle + sizeof(":" OMENU_TARGET_UBOOT_PROP ":") - 1) + 1;
            }
            else
            {
                strcpy(dst, cursor);
                dst += strlen(cursor) + 1;
            }

            cursor += strlen(cursor) + 1;
        }

        new_len = dst - new_buf;
        ret = fdt_setprop(dtbo_buf, fixups, prop_names[i], new_buf, new_len);
        free(new_buf);
        if (ret)
            goto out;
    }

out:
    for (i = 0; i < prop_count; i++)
        free(prop_names[i]);

    return ret;
}

static int omenu_fixup_has_live_target(void *dtbo_buf, const char *fixup)
{
    char *work;
    char *node_path;
    char *prop_name;
    char *offset;
    int node_off;
    int prop_len;
    int ret = 0;

    work = strdup(fixup);
    if (!work)
        return 0;

    node_path = work;
    if (node_path[0] != '/')
        goto out;

    prop_name = strchr(node_path + 1, ':');
    if (!prop_name)
        goto out;
    *prop_name++ = '\0';

    offset = strchr(prop_name, ':');
    if (!offset)
        goto out;
    *offset = '\0';

    node_off = fdt_path_offset(dtbo_buf, node_path);
    if (node_off < 0)
        goto out;

    if (fdt_getprop(dtbo_buf, node_off, prop_name, &prop_len))
        ret = 1;

out:
    free(work);
    return ret;
}

static int omenu_prune_fixups(void *dtbo_buf)
{
    int fixups;
    int prop_off;
    int prop_count = 0;
    int i;
    int ret = 0;
    char *prop_names[OMENU_MAX_SELECTION];

    memset(prop_names, 0, sizeof(prop_names));

    fixups = fdt_path_offset(dtbo_buf, OMENU_FIXUPS_NODE_PATH);
    if (fixups < 0)
        return 0;

    prop_off = fdt_first_property_offset(dtbo_buf, fixups);
    while (prop_off >= 0)
    {
        const struct fdt_property *prop;
        const char *prop_name;
        int len;

        prop = fdt_get_property_by_offset(dtbo_buf, prop_off, &len);
        if (!prop)
        {
            ret = len;
            goto out;
        }

        if (prop_count >= OMENU_MAX_SELECTION)
        {
            ret = -ENOSPC;
            goto out;
        }

        prop_name = fdt_string(dtbo_buf, fdt32_to_cpu(prop->nameoff));
        prop_names[prop_count] = strdup(prop_name);
        if (!prop_names[prop_count])
        {
            ret = -ENOMEM;
            goto out;
        }
        prop_count++;

        prop_off = fdt_next_property_offset(dtbo_buf, prop_off);
    }

    for (i = 0; i < prop_count; i++)
    {
        const char *cursor;
        char *new_buf;
        char *dst;
        const void *prop;
        int len;
        int new_len = 0;

        prop = fdt_getprop(dtbo_buf, fixups, prop_names[i], &len);
        if (!prop)
            continue;

        cursor = prop;
        new_buf = memalign(4, len + 1);
        if (!new_buf)
        {
            ret = -ENOMEM;
            goto out;
        }

        dst = new_buf;
        while (cursor < ((const char *)prop + len))
        {
            if (omenu_fixup_has_live_target(dtbo_buf, cursor))
            {
                strcpy(dst, cursor);
                dst += strlen(cursor) + 1;
            }
            cursor += strlen(cursor) + 1;
        }

        new_len = dst - new_buf;
        if (new_len == 0)
            ret = fdt_delprop(dtbo_buf, fixups, prop_names[i]);
        else
            ret = fdt_setprop(dtbo_buf, fixups, prop_names[i], new_buf, new_len);

        free(new_buf);
        if (ret)
            goto out;
    }

out:
    for (i = 0; i < prop_count; i++)
        free(prop_names[i]);

    return ret;
}

static int omenu_filter_kernel_overlay(void *dtbo_buf)
{
    int frag;
    int frag_count = 0;
    int ret = 0;
    char *frag_names[OMENU_MAX_SELECTION];

    memset(frag_names, 0, sizeof(frag_names));

    frag = fdt_first_subnode(dtbo_buf, 0);
    while (frag >= 0)
    {
        const char *frag_name;
        if (frag_count >= OMENU_MAX_SELECTION)
        {
            ret = -ENOSPC;
            goto out;
        }

        frag_name = fdt_get_name(dtbo_buf, frag, NULL);
        if (frag_name && !strncmp(frag_name, "fragment@", 9))
        {
            frag_names[frag_count] = strdup(frag_name);
            if (!frag_names[frag_count])
            {
                ret = -ENOMEM;
                goto out;
            }
            frag_count++;
        }

        frag = fdt_next_subnode(dtbo_buf, frag);
    }

    for (frag = 0; frag < frag_count; frag++)
    {
        int frag_off;
        int prop_len;

        frag_off = fdt_subnode_offset(dtbo_buf, 0, frag_names[frag]);
        if (frag_off < 0)
            continue;

        if (!fdt_getprop(dtbo_buf, frag_off, OMENU_TARGET_PROP, &prop_len) &&
            !fdt_getprop(dtbo_buf, frag_off, OMENU_TARGET_PATH_PROP, &prop_len))
        {
            ret = fdt_del_node(dtbo_buf, frag_off);
            if (ret)
                goto out;
            continue;
        }

        ret = fdt_delprop(dtbo_buf, frag_off, OMENU_TARGET_UBOOT_PROP);
        if (ret && ret != -FDT_ERR_NOTFOUND)
            goto out;
    }

    ret = omenu_prune_fixups(dtbo_buf);

out:
    while (frag_count-- > 0)
        free(frag_names[frag_count]);

    return ret;
}

static int omenu_build_uboot_overlay(const void *src_dtbo, void *dst_dtbo)
{
    int ret;
    int frag;
    int frag_count = 0;
    int keep_count = 0;
    char *frag_names[OMENU_MAX_SELECTION];

    memset(frag_names, 0, sizeof(frag_names));

    ret = fdt_open_into(src_dtbo, dst_dtbo, OMENU_MAX_DTBO_SIZE);
    if (ret)
        return ret;

    frag = fdt_first_subnode(dst_dtbo, 0);
    while (frag >= 0)
    {
        const char *frag_name;
        if (frag_count >= OMENU_MAX_SELECTION)
            return -ENOSPC;

        frag_name = fdt_get_name(dst_dtbo, frag, NULL);
        if (frag_name && !strncmp(frag_name, "fragment@", 9))
            frag_names[frag_count++] = strdup(frag_name);

        frag = fdt_next_subnode(dst_dtbo, frag);
    }

    for (frag = 0; frag < frag_count; frag++)
    {
        int frag_off;
        const void *uboot_target;
        void *uboot_target_copy = NULL;
        int target_len;

        frag_off = fdt_subnode_offset(dst_dtbo, 0, frag_names[frag]);
        if (frag_off < 0)
            continue;

        uboot_target = fdt_getprop(dst_dtbo, frag_off, OMENU_TARGET_UBOOT_PROP, &target_len);
        if (!uboot_target)
        {
            fdt_del_node(dst_dtbo, frag_off);
            continue;
        }

        uboot_target_copy = memalign(4, target_len);
        if (!uboot_target_copy)
        {
            ret = -ENOMEM;
            goto out;
        }

        memcpy(uboot_target_copy, uboot_target, target_len);

        fdt_delprop(dst_dtbo, frag_off, OMENU_TARGET_PROP);
        fdt_delprop(dst_dtbo, frag_off, OMENU_TARGET_PATH_PROP);

        if (omenu_is_string_prop(uboot_target_copy, target_len))
            ret = fdt_setprop_string(dst_dtbo, frag_off, OMENU_TARGET_PATH_PROP, uboot_target_copy);
        else
            ret = fdt_setprop(dst_dtbo, frag_off, OMENU_TARGET_PROP, uboot_target_copy, target_len);

        free(uboot_target_copy);
        uboot_target_copy = NULL;

        if (ret)
            goto out;

        fdt_delprop(dst_dtbo, frag_off, OMENU_TARGET_UBOOT_PROP);
        keep_count++;
    }

    if (!keep_count)
    {
        ret = -ENOENT;
        goto out;
    }

    ret = omenu_rewrite_fixups_target_uboot(dst_dtbo);
    if (ret)
        goto out;

    ret = omenu_prune_fixups(dst_dtbo);
out:
    while (frag_count-- > 0)
        free(frag_names[frag_count]);

    return ret;
}

static int omenu_apply_uboot_overlay(const char *dtbo_path, void *dtbo_buf)
{
    int ret;
    void *uboot_dtbo_buf;

    uboot_dtbo_buf = memalign(4, OMENU_MAX_DTBO_SIZE);
    if (!uboot_dtbo_buf)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate uboot overlay buffer\n");
        return -ENOMEM;
    }

    ret = omenu_build_uboot_overlay(dtbo_buf, uboot_dtbo_buf);
    if (ret == -ENOENT)
    {
        free(uboot_dtbo_buf);
        return 0;
    }

    if (ret)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to build uboot overlay for %s: %s\n",
                  dtbo_path, fdt_strerror(ret));
        free(uboot_dtbo_buf);
        return ret;
    }

    if (!gd->fdt_blob)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "U-Boot control fdt is NULL, skip %s\n", dtbo_path);
        free(uboot_dtbo_buf);
        return -EINVAL;
    }

    ret = omenu_prepare_fdt((void *)gd->fdt_blob, "uboot");
    if (ret)
    {
        free(uboot_dtbo_buf);
        return ret;
    }

    ret = fdt_overlay_apply_verbose((void *)gd->fdt_blob, uboot_dtbo_buf);
    if (ret < 0)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "U-Boot overlay apply failed for %s: %s\n",
                  dtbo_path, fdt_strerror(ret));
        free(uboot_dtbo_buf);
        return ret;
    }
    else
    {
        OMENU_LOG(OMENU_LOG_INFO, "U-Boot overlay applied: %s\n", dtbo_path);
    }

    free(uboot_dtbo_buf);
    return 1;
}

/*******************************
 * @brief  : 从控制台读取一行输入，支持退格处理
 * @param  : buf - 存储输入内容的缓冲区
 * @param  : maxlen - 缓冲区最大长度
 * @return : 无
 *******************************/
static void read_line(char *buf, int maxlen)
{
    int i = 0;
    while (i < maxlen - 1)
    {
        int ch = getchar();

        if (ch == '\r' || ch == '\n')
        {
            putc('\n');
            break;
        }

        // 退格键
        if (ch == 0x7F || ch == 0x08)
        {
            if (i > 0)
            {
                i--;
                puts("\b \b"); // 删除终端上的字符
            }
            continue;
        }

        // 可见字符
        if (ch >= 0x20 && ch <= 0x7E)
        {
            buf[i++] = ch;
            putc(ch);
        }
    }
    buf[i] = '\0';
}

/*******************************
 * @brief  : 获取 oMenu 配置参数
 * @param  : cfg - 配置结构体指针
 * @return : 无
 *******************************/
static void get_omenu_config(configs_t *cfg)
{
    if (!cfg)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "cfg is NULL\n");
        return;
    }

    memset(cfg, 0, sizeof(configs_t));

    // Storage Type
    strncpy(cfg->stroage_type, OMENU_STORAGE_DEV, MAX_CFG_LEN - 1);

    // Storage Device Number
#ifdef CONFIG_OMENU_STORAGE_DEV_NUM
    strncpy(cfg->stroage_dev_num, CONFIG_OMENU_STORAGE_DEV_NUM, MAX_CFG_LEN - 1);
#else
    OMENU_LOG(OMENU_LOG_ERROR, "CONFIG_OMENU_STORAGE_DEV_NUM not defined, using default 0\n");
    strcpy(cfg->stroage_dev_num, "0");
#endif

    // Storage Partition
#ifdef CONFIG_OMENU_STORAGE_PART_NUM
    strncpy(cfg->stroage_partition, CONFIG_OMENU_STORAGE_PART_NUM, MAX_CFG_LEN - 1);
#else
    OMENU_LOG(OMENU_LOG_ERROR, "CONFIG_OMENU_STORAGE_PART_NUM not defined, using default 0\n");
    strcpy(cfg->stroage_partition, "0");
#endif

    // Directory Name
#ifdef CONFIG_OMENU_DIRECTORY_NAME
    strncpy(cfg->directory_name, CONFIG_OMENU_DIRECTORY_NAME, MAX_CFG_LEN - 1);
#else
    OMENU_LOG(OMENU_LOG_ERROR, "CONFIG_OMENU_DIRECTORY_NAME not defined, using default \"omenu\"\n");
    strcpy(cfg->directory_name, "omenu");
#endif
}

/*******************************
 * @brief  : 判断路径是否已选中
 * @param  : path - 路径字符串
 * @return : 1 表示已选中，0 表示未选中
 *******************************/
static int is_selected(const char *path)
{
    for (int i = 0; i < selection_count; i++)
    {
        if (!strcmp(selections[i], path))
            return 1;
    }
    return 0;
}

/*******************************
 * @brief  : 切换选中状态（选中或取消）
 * @param  : path - 路径字符串
 * @return : 无
 *******************************/
static void toggle_selection(const char *path)
{
    for (int i = 0; i < selection_count; i++)
    {
        if (!strcmp(selections[i], path))
        {
            free(selections[i]);
            for (int j = i; j < selection_count - 1; j++)
                selections[j] = selections[j + 1];
            selection_count--;
            return;
        }
    }
    selections[selection_count++] = strdup(path);
}

/*******************************
 * @brief  : 解析 list.txt 文件，提取条目并标记目录或文件
 * @param  : base_path - 当前菜单路径
 * @param  : entries - 存储条目名称数组
 * @param  : is_dir - 标记每个条目是否为目录的数组
 * @return : 返回有效条目数量
 *******************************/
static int parse_list_file(const char *base_path, char *entries[], int is_dir[])
{
    char file_path[OMENU_MAX_PATH];
    snprintf(file_path, sizeof(file_path), "%s/%s", base_path, OMENU_DIR_FILE_NAME);

    char dev_part[10];
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.stroage_dev_num, cfg.stroage_partition);
    if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return -1;
    }

    loff_t file_size;
    if (fs_size(file_path, &file_size))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to get size of %s\n", file_path);
        return 0;
    }

    if (file_size == 0)
        return 0;

    if (file_size > 8192)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Invalid file size: %lld\n", file_size);
        return -1;
    }

    char *buf = memalign(4, file_size + 1);
    if (!buf)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return 0;
    }

    if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return -1;
    }

    loff_t len;
    if (fs_read(file_path, (ulong)buf, 0, file_size, &len))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to read %s\n", file_path);
        free(buf);
        return 0;
    }

    buf[len] = '\0'; // null-terminate

    int count = 0;
    char *line = strtok(buf, "\r\n");
    while (line && count < OMENU_MAX_SELECTION)
    {
        if (line[0] != '#')
        {
            entries[count] = strdup(line);
            is_dir[count] = strstr(line, ".dtbo") == NULL;
            count++;
        }
        line = strtok(NULL, "\r\n");
    }

    free(buf);
    return count;
}

/*******************************
 * @brief  : 清空当前选中的插件列表
 * @param  : 无
 * @return : 无
 *******************************/
static void clear_selections(void)
{
    for (int i = 0; i < selection_count; i++)
    {
        free(selections[i]);
    }
    selection_count = 0;
}

/*******************************
 * @brief  : 从已保存文件中加载已选中插件列表
 * @param  : 无
 * @return : 无
 *******************************/
static void update_selections(void)
{
    clear_selections();

    char dev_part[10];
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.stroage_dev_num, cfg.stroage_partition);
    if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return;
    }

    loff_t file_size;
    if (fs_size(OMENU_SELECTED_FILE_NAME, &file_size))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to get size of %s\n", OMENU_SELECTED_FILE_NAME);
        return;
    }

    if (file_size == 0)
        return;

    if (file_size > 8192)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Invalid file size: %lld\n", file_size);
        return;
    }

    char *buf = memalign(4, file_size + 1); // +1 for '\0'
    if (!buf)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return;
    }

    if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return;
    }

    loff_t len;
    if (fs_read(OMENU_SELECTED_FILE_NAME, (ulong)buf, 0, file_size, &len))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to read %s\n", OMENU_SELECTED_FILE_NAME);
        free(buf);
        return;
    }

    buf[len] = '\0'; // null-terminate for strtok

    char *line = strtok(buf, "\r\n");
    while (line && selection_count < OMENU_MAX_SELECTION)
    {
        if (line[0] != '#')
        {
            selections[selection_count++] = strdup(line);
        }
        line = strtok(NULL, "\r\n");
    }

    free(buf);
}

/*******************************
 * @brief  : 将当前选中的插件路径写入保存文件
 * @param  : 无
 * @return : 无
 *******************************/
static void save_selections(void)
{
    loff_t len;
    int ret;

    char dev_part[10];
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.stroage_dev_num, cfg.stroage_partition);
    if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set block device\n");
        return;
    }

    int total_size = 0;
    for (int i = 0; i < selection_count; i++)
    {
        if (selections[i])
        {
            total_size += strlen(selections[i]) + 1; // +1 for '\n'
        }
    }

    if (total_size > 4096)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Total selection size exceeds 4096 bytes\n");
        return;
    }

    char *buf = memalign(4, total_size);
    if (!buf)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return;
    }

    size_t offset = 0;
    for (int i = 0; i < selection_count; i++)
    {
        if (!selections[i])
        {
            continue;
        }

        int n = sprintf(buf + offset, "%s\n", selections[i]);
        if (n <= 0 || offset + n > total_size)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Selection list too long or sprintf error!\n");
            free(buf);
            return;
        }
        offset += n;
    }

    ret = fs_write(OMENU_SELECTED_FILE_NAME, (ulong)buf, 0, offset, &len);
    free(buf);

    if (ret != 0 || len != offset)
    {
        OMENU_LOG(OMENU_LOG_INFO, "Failed to write %s (ret=%d, len=%llu)\n", OMENU_SELECTED_FILE_NAME, ret, len);
    }
    else
    {
        OMENU_LOG(OMENU_LOG_INFO, "Saved %d selections to %s\n", selection_count, OMENU_SELECTED_FILE_NAME);
    }
}

/*******************************
 * @brief  : 应用已选中的 U-Boot 设备树覆盖（dtbo）文件
 * @param  : 无
 * @return : 成功应用到 U-Boot control FDT 的插件数量
 *******************************/
int omenu_uboot_fdt_apply(void)
{
    int applied_count = 0;
    char dev_part[10];

    get_omenu_config(&cfg);
    update_selections();

    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.stroage_dev_num, cfg.stroage_partition);

    for (int i = 0; i < selection_count; i++)
    {
        const char *dtbo_path = selections[i];
        loff_t len;
        void *dtbo_buf;
        int ret;

        dtbo_buf = memalign(4, OMENU_MAX_DTBO_SIZE);
        if (!dtbo_buf)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate memory for overlay\n");
            return -ENOMEM;
        }

        if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
            free(dtbo_buf);
            return -ENODEV;
        }

        if (fs_read(dtbo_path, (ulong)dtbo_buf, 0, OMENU_MAX_DTBO_SIZE, &len))
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to read dtbo file: %s\n", dtbo_path);
            free(dtbo_buf);
            continue;
        }

        if (fdt_check_header(dtbo_buf) != 0)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Invalid FDT overlay file: %s\n", dtbo_path);
            free(dtbo_buf);
            continue;
        }

        ret = omenu_apply_uboot_overlay(dtbo_path, dtbo_buf);
        if (ret > 0)
            applied_count++;

        free(dtbo_buf);
    }

    return applied_count;
}

/*******************************
 * @brief  : 应用已选中的设备树覆盖（dtbo）文件
 * @param  : 无
 * @return : 无
 *******************************/
int omenu_fdt_apply(void)
{
    int applied_count = 0;

    get_omenu_config(&cfg);
    update_selections();

    if (!working_fdt)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Kernel working fdt is NULL, skip overlays\n");
        return -EINVAL;
    }

    char dev_part[10];
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.stroage_dev_num, cfg.stroage_partition);

    for (int i = 0; i < selection_count; i++)
    {
        const char *dtbo_path = selections[i];

        OMENU_LOG(OMENU_LOG_INFO, "Applying overlay: %s\n", dtbo_path);

        loff_t len;
        void *dtbo_buf = memalign(4, OMENU_MAX_DTBO_SIZE); // 分配 128KB 缓冲区
        if (!dtbo_buf)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate memory for overlay\n");
            return -ENOMEM;
        }

        if (fs_set_blk_dev(cfg.stroage_type, dev_part, OMENU_FS_TYPE))
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
            free(dtbo_buf);
            return -ENODEV;
        }

        if (fs_read(dtbo_path, (ulong)dtbo_buf, 0, OMENU_MAX_DTBO_SIZE, &len))
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to read dtbo file: %s\n", dtbo_path);
            free(dtbo_buf);
            continue;
        }

        if (fdt_check_header(dtbo_buf) != 0)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Invalid FDT overlay file: %s\n", dtbo_path);
            free(dtbo_buf);
            continue;
        }

        int ret = omenu_filter_kernel_overlay(dtbo_buf);
        if (ret)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to filter kernel overlay for %s: %s\n",
                      dtbo_path, fdt_strerror(ret));
            free(dtbo_buf);
            continue;
        }

        if (omenu_prepare_fdt(working_fdt, "kernel"))
        {
            free(dtbo_buf);
            continue;
        }

        ret = fdt_overlay_apply_verbose(working_fdt, dtbo_buf);
        if (ret < 0)
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Kernel overlay apply failed for %s: %s\n",
                      dtbo_path, fdt_strerror(ret));
        }
        else
        {
            OMENU_LOG(OMENU_LOG_INFO, "Kernel overlay applied: %s\n", dtbo_path);
            applied_count++;
        }

        free(dtbo_buf);
    }

    return applied_count;
}

/*******************************
 * @brief  : 展示交互式插件菜单界面，支持递归进入子目录
 * @param  : base_path - 当前显示菜单的路径
 * @return : 无
 *******************************/
static void show_menu(const char *base_path)
{
    char *entries[OMENU_MAX_SELECTION];
    int is_dir[OMENU_MAX_SELECTION];

    int count = parse_list_file(base_path, entries, is_dir);
    if (count < 0)
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to parse list file in %s/%s\n", base_path, OMENU_DIR_FILE_NAME);
    }

    while (1)
    {
        // 打印菜单
        printf("\n========== %s ==========\n", base_path);
        for (int i = 0; i < count; i++)
        {
            if (is_dir[i])
                printf("[%d] %s\n", i + 1, entries[i]);
            else
            {
                char full_path[OMENU_MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[i]);
                printf("[%d] [%c] %s\n", i + 1, is_selected(full_path) ? '*' : ' ', entries[i]);
            }
        }
        if (strcmp(base_path, cfg.directory_name) == 0)
        {
            printf("[c] clear:  clear selections and uncheck all plugins\n");
            printf("[s] save:   save current selections to %s\n", OMENU_SELECTED_FILE_NAME);
            printf("[r] reboot: restart the system without saving changes\n");
            printf("[q] quit:   quit the menu without saving changes\n");
        }
        else
        {
            printf("[0] return to previous menu\n");
        }

        // 用户输入
        char inbuf[16] = {0};
        printf("Select: ");
        read_line(inbuf, sizeof(inbuf));
        if (inbuf[0] == '\0')
            continue;

        // 对顶层目录菜单选项做特殊判断
        if (strcmp(base_path, cfg.directory_name) == 0)
        {
            if (inbuf[0] == 'q') // 退出菜单
                break;

            if (inbuf[0] == 's') // 保存已选择的插件
            {
                save_selections();
                continue;
            }

            if (inbuf[0] == 'c') // 重置选择
            {
                clear_selections();
                OMENU_LOG(OMENU_LOG_INFO, "Selections cleared\n");
                continue;
            }

            if (inbuf[0] == 'r') // 重启系统
            {
                run_command("reset", 0);
                continue;
            }
        }

        char *endptr;
        int sel = simple_strtoul(inbuf, &endptr, 10);

        // 非负整数 输入检查
        if (*endptr != '\0')
            continue;

        // 输入数值超过现有选项
        if (sel > count)
            continue;

        // 输入0，返回上级目录
        if (sel == 0)
            break;

        if (is_dir[sel - 1])
        {
            char new_path[OMENU_MAX_PATH];
            snprintf(new_path, sizeof(new_path), "%s/%s", base_path, entries[sel - 1]);
            show_menu(new_path);
        }
        else
        {
            char full_path[OMENU_MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[sel - 1]);
            toggle_selection(full_path);
        }
    }

    for (int i = 0; i < count; i++)
    {
        free(entries[i]);
    }
}

/*******************************
 * @brief  : omenu 命令入口
 * @param  : cmdtp, flag, argc, argv - 命令行参数
 * @return : 0
 *******************************/
static int do_omenu(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    // 获取配置
    get_omenu_config(&cfg);
    OMENU_LOG(OMENU_LOG_DEBUG, "OMENU Version: %s\n", OMENU_VERSION);
    OMENU_LOG(OMENU_LOG_DEBUG, "OMENU Configurations:\n");
    OMENU_LOG(OMENU_LOG_DEBUG, "Storage Type        : %s\n", cfg.stroage_type);
    OMENU_LOG(OMENU_LOG_DEBUG, "Storage Device      : %s\n", cfg.stroage_dev_num);
    OMENU_LOG(OMENU_LOG_DEBUG, "Storage Partition   : %s\n", cfg.stroage_partition);
    OMENU_LOG(OMENU_LOG_DEBUG, "Directory Name      : %s\n", cfg.directory_name);

    if (strcmp(cfg.stroage_type, "usb") == 0)
    {
        run_command("usb start", 0);
    }

    // 更新选择列表
    update_selections();

    // 菜单解析
    show_menu(cfg.directory_name);

    return 0;
}

U_BOOT_CMD(
    omenu, 1, 0, do_omenu,
    "oMenu - interactive device tree overlay selection menu",
    "oMenu - interactive device tree overlay selection menu");
