#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <cli.h>
#include <fs.h>
#include <console.h>
#include <stdio.h>

#include "cmd_omenu.h"
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

static int do_omenu(struct cmd_tbl_s *cmdtp, int flag, int argc, char *const argv[])
{  
	configs_t cfg;

	// 1、获取menuconfig里的配置
    get_omenu_config(&cfg);
	printf("OMENU Configurations:\n");
    printf("  MMC Device      : %s\n", cfg.mmc_dev_num);
    printf("  MMC Partition   : %s\n", cfg.mmc_partition);
    printf("  Directory Name  : %s\n", cfg.directory_name);

	return 0;
}

U_BOOT_CMD(
	omenu, 1, 0, do_omenu,
	"dtbo menu",
	"dtbo menu"
);