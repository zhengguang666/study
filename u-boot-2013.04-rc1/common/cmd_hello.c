#include <common.h>
#include <command.h>

static int do_hello(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[] )
{
	int i = 0;
	for(i = 0; i < argc; i++)
		printf("argv[%d] = %s\n", i, argv[i]);
	return 0;
}

U_BOOT_CMD(hello, 10, 1, do_hello,
	"just for test\n",
	"\n"
	" hello command---\n"
	"    --Built a simple command");

