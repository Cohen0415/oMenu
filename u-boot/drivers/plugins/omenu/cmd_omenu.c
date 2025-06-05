#include <common.h>
#include <command.h>
#include <fs.h>
#include <malloc.h>
#include <stdio.h>
#include <console.h>
#include <cli.h>
#include <mapmem.h>

#include "cmd_omenu.h"
#include "log_omenu.h"

// 保存选中状态
static char *selections[OMENU_MAX_SELECTION];
static int selection_count = 0;

static configs_t cfg;

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
        int ch = getc();
        
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
                puts("\b \b");  // 删除终端上的字符
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

	// MMC Device Number
#ifdef CONFIG_OMENU_MMC_DEV_NUM
	strncpy(cfg->mmc_dev_num, CONFIG_OMENU_MMC_DEV_NUM, MAX_CFG_LEN - 1);
#else
	OMENU_LOG(OMENU_LOG_ERROR, "CONFIG_OMENU_MMC_DEV_NUM not defined, using default 0\n");
	strcpy(cfg->mmc_dev_num, "0");
#endif

	// MMC Partition
#ifdef CONFIG_OMENU_MMC_PARTITION
	strncpy(cfg->mmc_partition, CONFIG_OMENU_MMC_PARTITION, MAX_CFG_LEN - 1);
#else
	OMENU_LOG(OMENU_LOG_ERROR, "CONFIG_OMENU_MMC_PARTITION not defined, using default 0\n");
	strcpy(cfg->mmc_partition, "0");
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
    snprintf(file_path, sizeof(file_path), "%s/list.txt", base_path);

    char dev_part[10];
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.mmc_dev_num, cfg.mmc_partition);
    if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return CMD_RET_FAILURE;
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
        return CMD_RET_FAILURE;
    }

    char *buf = memalign(4, file_size + 1);
    if (!buf) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return 0;
    }

    if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return CMD_RET_FAILURE;
    }

    loff_t len;
    if (fs_read(file_path, (ulong)buf, 0, file_size, &len)) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to read %s\n", file_path);
        free(buf);
        return 0;
    }

    buf[len] = '\0';  // null-terminate

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
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.mmc_dev_num, cfg.mmc_partition);
    if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
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

    char *buf = memalign(4, file_size + 1);  // +1 for '\0'
    if (!buf) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return;
    }

    if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
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

    buf[len] = '\0';  // null-terminate for strtok

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
    snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.mmc_dev_num, cfg.mmc_partition);
    if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set block device\n");
        return;
    }

    char *buf = memalign(4, 4096);
    if (!buf) 
    {
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate buffer\n");
        return;
    }

    buf[0] = '\0';
    size_t offset = 0;
    for (int i = 0; i < selection_count; i++) 
    {
        if (!selections[i]) 
        {
            continue;
        }

        int n = snprintf(buf + offset, 4096 - offset, "%s\n", selections[i]);
        if (n < 0 || offset + n >= 4096) 
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Selection list too long!\n");
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
 * @brief  : 应用已选中的设备树覆盖（dtbo）文件
 * @param  : 无
 * @return : 无
 *******************************/
void omenu_fdt_apply(void)
{
    get_omenu_config(&cfg);
    update_selections();

    char dev_part[10];
	snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.mmc_dev_num, cfg.mmc_partition);
	if (fs_set_blk_dev(OMENU_STORE_DEV, dev_part, OMENU_FS_TYPE)) 
	{
        OMENU_LOG(OMENU_LOG_ERROR, "Failed to set blk dev\n");
        return;
    }

    for (int i = 0; i < selection_count; i++) 
    {
        const char *dtbo_path = selections[i];

        OMENU_LOG(OMENU_LOG_INFO, "Applying overlay: %s\n", dtbo_path);

        loff_t len;
        void *dtbo_buf = malloc(0x20000);  // 分配 128KB 缓冲区
        if (!dtbo_buf) 
        {
            OMENU_LOG(OMENU_LOG_ERROR, "Failed to allocate memory for overlay\n");
            return;
        }

        if (fs_read(dtbo_path, (ulong)dtbo_buf, 0, 0x20000, &len)) 
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

        run_command("fdt resize 8192", 0);

        int ret = fdt_overlay_apply_verbose(working_fdt, dtbo_buf);
        if (ret < 0) 
        {
            OMENU_LOG(OMENU_LOG_INFO, "Overlay apply failed for %s\n", dtbo_path);
        } 
        else 
        {
            OMENU_LOG(OMENU_LOG_INFO, "Overlay applied: %s\n", dtbo_path);
        }

        free(dtbo_buf);
    }
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
            printf("[c] clear selections and uncheck all plugins\n");
            printf("[s] save current selections to %s\n", OMENU_SELECTED_FILE_NAME);
            printf("[r] restart the system without saving changes\n");
            printf("[q] quit the menu without saving changes\n");
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
            if (inbuf[0] == 'q')    // 退出菜单
                break;

            if (inbuf[0] == 's')    // 保存已选择的插件
            {
                save_selections();
                continue;
            }

            if (inbuf[0] == 'c')    // 重置选择
            {
                clear_selections();
                OMENU_LOG(OMENU_LOG_INFO, "Selections cleared\n");
                continue;
            }

            if (inbuf[0] == 'r')    // 重启系统
            {
                run_command("reboot", 0);
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
static int do_omenu(struct cmd_tbl_s *cmdtp, int flag, int argc, char *const argv[])
{  
    // 获取配置
    get_omenu_config(&cfg);
    OMENU_LOG(OMENU_LOG_DEBUG, "OMENU Configurations:\n");
    OMENU_LOG(OMENU_LOG_DEBUG, "MMC Device      : %s\n", cfg.mmc_dev_num);
    OMENU_LOG(OMENU_LOG_DEBUG, "MMC Partition   : %s\n", cfg.mmc_partition);
    OMENU_LOG(OMENU_LOG_DEBUG, "Directory Name  : %s\n", cfg.directory_name);

    // 更新选择列表
    update_selections();

    // 菜单解析
	show_menu(cfg.directory_name);
    
	return 0;
}

U_BOOT_CMD(
	omenu, 1, 0, do_omenu,
	"oMenu - interactive device tree overlay selection menu",
	"oMenu - interactive device tree overlay selection menu"
);