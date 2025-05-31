#include <common.h>
#include <command.h>
#include <fs.h>
#include <malloc.h>
#include <stdio.h>
#include <console.h>
#include <cli.h>

#include "cmd_omenu.h"

// 保存选中状态
static char *selections[MAX_SELECTION];
static int selection_count = 0;

static configs_t cfg;

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

static void get_omenu_config(configs_t *cfg)
{
	if (!cfg) 
		return;

    memset(cfg, 0, sizeof(configs_t));

#ifdef CONFIG_OMENU_MMC_DEV_NUM
    strncpy(cfg->mmc_dev_num, CONFIG_OMENU_MMC_DEV_NUM, MAX_CFG_LEN - 1);
#else
    printf("Warning: CONFIG_OMENU_MMC_DEV_NUM not defined\n");
#endif

#ifdef CONFIG_OMENU_MMC_PARTITION
    strncpy(cfg->mmc_partition, CONFIG_OMENU_MMC_PARTITION, MAX_CFG_LEN - 1);
#else
    printf("Warning: CONFIG_OMENU_MMC_PARTITION not defined\n");
#endif

#ifdef CONFIG_OMENU_DIRECTORY_NAME
    strncpy(cfg->directory_name, CONFIG_OMENU_DIRECTORY_NAME, MAX_CFG_LEN - 1);
#else
    printf("Warning: CONFIG_OMENU_DIRECTORY_NAME not defined\n");
#endif
}

static int is_selected(const char *path) 
{
    for (int i = 0; i < selection_count; i++) 
	{
        if (!strcmp(selections[i], path))
            return 1;
    }
    return 0;
}

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

static int parse_list_file(const char *base_path, char *entries[], int is_dir[]) 
{
    char file_path[MAX_PATH];
    snprintf(file_path, sizeof(file_path), "%s/list.txt", base_path);

	char dev_part[10];
	snprintf(dev_part, sizeof(dev_part), "%s:%s", cfg.mmc_dev_num, cfg.mmc_partition);
	if (fs_set_blk_dev(STORE_DEV, dev_part, FS_TYPE_ANY)) 
	{
        printf("Failed to set blk dev\n");
        return CMD_RET_FAILURE;
    }

    loff_t len;
    char buf[2048];
    if (fs_read(file_path, (ulong)buf, 0, sizeof(buf), &len)) 
	{
        printf("Failed to read %s\n", file_path);
        return 0;
    }

    int count = 0;
	int started = 0;
    char *line = strtok(buf, "\r\n");
    while (line && count < MAX_SELECTION) 
	{
		if (!strcmp(line, "[start]"))
		{
			started = 1;
		}
		else if (!strcmp(line, "[end]"))
		{
			break;
		}
		else if (started && line[0] != '#')
		{
			entries[count] = strdup(line);
			is_dir[count] = strstr(line, ".dtbo") == NULL;
			count++;
		}
        line = strtok(NULL, "\r\n");
    }
	
    return count;
}

static void show_menu(const char *base_path) 
{
    char *entries[MAX_SELECTION];
    int is_dir[MAX_SELECTION];
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
                char full_path[MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[i]);
                printf("[%d] [%c] %s\n", i + 1, is_selected(full_path) ? '*' : ' ', entries[i]);
            }
        }
        printf("[0] %s\n", strcmp(base_path, cfg.directory_name) == 0 ? "Quit" : "Back");

		// 用户输入
        char inbuf[16] = {0};
		printf("Select: ");
		read_line(inbuf, sizeof(inbuf));
		if (inbuf[0] == '\0')
			continue;

		char *endptr;
		int sel = simple_strtoul(inbuf, &endptr, 10);
		
		// 非负整数 输入检查
		if (*endptr != '\0')
			continue;

		// 输入数值超过现有选项
		if (sel > count)
			continue;

		// 输入0，返回上级目录或退出菜单
		if (sel == 0)
    		break;

        if (is_dir[sel - 1]) 
		{
            char new_path[MAX_PATH];
            snprintf(new_path, sizeof(new_path), "%s/%s", base_path, entries[sel - 1]);
            show_menu(new_path);
        } 
		else 
		{
            char full_path[MAX_PATH];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entries[sel - 1]);
            toggle_selection(full_path);
        }
    }

    for (int i = 0; i < count; i++) 
	{
        free(entries[i]);
    }
}

static int do_omenu(struct cmd_tbl_s *cmdtp, int flag, int argc, char *const argv[])
{  
    // 获取配置
    get_omenu_config(&cfg);
    printf("OMENU Configurations:\n");
    printf("  MMC Device      : %s\n", cfg.mmc_dev_num);
    printf("  MMC Partition   : %s\n", cfg.mmc_partition);
    printf("  Directory Name  : %s\n", cfg.directory_name);

	show_menu(cfg.directory_name);

	return 0;
}

U_BOOT_CMD(
	omenu, 1, 0, do_omenu,
	"dtbo menu",
	"dtbo menu"
);