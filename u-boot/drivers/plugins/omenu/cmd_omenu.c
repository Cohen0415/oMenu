#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <cli.h>
#include <fs.h>

static int do_omenu(struct cmd_tbl_s *cmdtp, int flag, int argc, char *const argv[])
{  
	printf("i am omenu\n");
	return 0;
}

U_BOOT_CMD(
	omenu, 1, 0, do_omenu,
	"dtbo menu",
	"dtbo menu"
);