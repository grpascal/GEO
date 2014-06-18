/*******************************************************************************
* 
* The content of this file or document is CONFIDENTIAL and PROPRIETARY
* to Maxim Integrated Products.  It is subject to the terms of a
* License Agreement between Licensee and Maxim Integrated Products.
* restricting among other things, the use, reproduction, distribution
* and transfer.  Each of the embodiments, including this information and
* any derivative work shall retain this copyright notice.
* 
* Copyright (c) 2011 Maxim Integrated Products.
* All rights reserved.
* 
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>

#if !defined(_WIN32)
	#include <unistd.h>
	#include <sys/stat.h>
	#include <getopt.h>
	#include <arpa/inet.h>
#else
	#include <sys/stat.h>
	#include "getopt-win.h"
	#include "devsetup.h"
#endif

#include "libmxcam.h"

#define MXCAM_MAX_TOTAL_OPTIONS 50 /* max number of different options in mxcam */
#define MXCAM_MAX_OPTIONS 20 /* max number of options per sub command */
#define MXCAM_MAX_ARGS 20 /* max number of args per sub commands */

/* Options that have no short option char should use an identifying
 * integer equal to or greater than this. */
#define FIRST_LONGOPT_ID 256

#if defined(_WIN32)
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#endif

#define MANDATORY 0
#define OPTIONAL  1

#if !defined(_WIN32) 

#define PRINTV(args...) do { if(verbose) printf(args); } while (0)
#define PRINTM(args...) do { printf(args); } while (0)
#define printf_bold(args...) \
	do { \
		printf("%c[1m", 27); \
		printf(args); \
		printf("%c[0m", 27); \
	} while(0)

#else
#define PRINTV(...) {if(verbose) printf(__VA_ARGS__); }
#define PRINTM(...) { printf(__VA_ARGS__); }
#define printf_bold printf
#endif

int verbose=0;

#if defined(_WIN32)
	int  opterr;
	int  optind=1;
	int  optopt;
	int  optreset;
	char *optarg=NULL;
#endif

/* OPTIONS */

/* Add an identifier here for long options that don't have a short
 * option. Options that have both long and short options should just
 * use the short option letter as identifier.  */

typedef enum {
	opt_poll=FIRST_LONGOPT_ID,
	opt_device,
	opt_bus,
	opt_addr,
	opt_verbose,
	opt_bootloader,
	opt_bootmode,
	opt_fw,
	opt_vendor,
	opt_silent,
	opt_i2cfile,
	opt_rom,
	opt_isprcnt,
	opt_isppause,
	opt_snor_hdr,
#if defined(_WIN32)
	opt_vid,
	opt_pid,
#endif
	opt_oldcam,
	opt_more,
	opt_ddr_size, 
	opt_fullmem,
	opt_loadmem,
	opt_duration
} mxcam_longopt_id_t;

typedef enum {
	arg_notype,
	arg_int,
	arg_string,
	arg_uint,
} arg_type_t;

struct mxcam_option {
	mxcam_longopt_id_t id;
	const char *long_name;
	int is_optional;
	int has_arg;
	arg_type_t arg_type;
	const char *arg_str;
	const char *help;
};

/* Global options are considered as always OPTIONAL */
const struct mxcam_option mxcam_global_options[] = {
	{opt_poll, "poll", OPTIONAL, no_argument, arg_notype,
		NULL, "wait until ANY maxim device is connected"},
	{opt_device, "device", OPTIONAL, required_argument, arg_int,
		"device-id", "select device with device-id"},
	{opt_bus, "bus", OPTIONAL, required_argument, arg_int,
		"X", "select device on bus X with addr Y (--addr required)"},
	{opt_addr, "addr", OPTIONAL, required_argument, arg_int,
		"Y", "select device on bus X with addr Y (--bus required)"},
	{opt_verbose, "verbose", OPTIONAL, no_argument, arg_notype,
		NULL, "display verbose messages"},
	{opt_oldcam, "oldcam", OPTIONAL, no_argument, arg_notype,
		NULL, "communicate with old generation Maxim cameras."},
	{0, NULL, 0, 0, 0, NULL, NULL}
};

const struct mxcam_option mxcam_options[] = {
	{opt_fw, "fw", OPTIONAL, required_argument, arg_string,
		"firmware-image", "firmware image to use"},
	{opt_bootloader, "bootloader", OPTIONAL, required_argument, arg_string,
		"image", "bootloader image to use"},
	{opt_bootmode, "bootmode", OPTIONAL, required_argument, arg_string,
		"usb|snor", "specify the boot mode"},
	{opt_rom, "rom", OPTIONAL, required_argument, arg_string,"rom-image",
		"composite rom image to use"},
	{opt_vendor, "vendor", OPTIONAL, no_argument, arg_notype, NULL,
		"use the vendor config area instead of Maxim "
		"config area"},
	{opt_silent, "silent", OPTIONAL, no_argument, arg_notype, NULL,
		"do not prompt for confirmation before performing the action"},
        {opt_i2cfile, "f", OPTIONAL, required_argument, arg_string, "file.csv",
                "i2c read/write form csv file having subaddr=value pair"},
	{opt_isprcnt, "count", OPTIONAL, required_argument, arg_int, "#",
        "read a block of data"},
	{opt_isppause, "pause", OPTIONAL, required_argument, arg_int, "0|1",
		"pause isp firmware"},
	{opt_snor_hdr, "snor", OPTIONAL, no_argument, arg_notype, NULL,
	"to read image header from snor"},
#if defined(_WIN32)
	{opt_vid, "vid", OPTIONAL, required_argument, arg_string, "vid",
	"vendor id"},
	{opt_pid, "pid", OPTIONAL, required_argument, arg_string, "pid",
	"product id"},
#endif
	{opt_more, "more", OPTIONAL, no_argument, arg_notype, NULL,
	"display more information"},
	{opt_ddr_size, "ddr_size", MANDATORY, required_argument, arg_int,
		"size", "size of the DDR in MB's"},
	{opt_fullmem, "fullmem", OPTIONAL, no_argument, arg_notype,
		NULL, "option to run DDR memory test in bootloader mode"},
	{opt_loadmem, "loadmem", OPTIONAL, no_argument, arg_notype,
		NULL, "option to run DDR load test"},
	{opt_duration, "duration", OPTIONAL, required_argument, arg_int,
		"duration", "DDR load test duration in seconds"},
	{0, NULL, 0, 0, 0, NULL, NULL}
};

/* Structure to hold options (and their values) gathered from command line  */
struct option_val {
	const struct mxcam_option *opt;
	const char *val;
};


/* ARGUMENTS */
typedef enum {
	arg_boot_image=1,
	arg_boot_optimage,
	arg_bootmode_usbsnor,
	arg_getkey_keyname,
	arg_getccr_keyname,
	arg_setkey_keyname,
	arg_setkey_value,
	arg_removekey_keyname,
	arg_erase_file,
	arg_erase_size,
	arg_help_subcmd,
	arg_seteptype_blukisoc,
	arg_i2c_inst,
	arg_i2c_type,
	arg_i2c_addr,
	arg_i2c_subaddr,
	arg_i2c_value,
	arg_tcw_value,
	arg_isp_addr,
	arg_isp_value,
	arg_usb_testmode,
	arg_qcc_bid,
	arg_qcc_addr,
	arg_qcc_length,
	arg_qcc_value,
} mxcam_arg_id_t;

struct mxcam_arg {
	mxcam_arg_id_t id;
	const char *name;
	int is_optional;
	arg_type_t type;
	const char *help;
};

const struct mxcam_arg mxcam_args[] = {
	{arg_boot_image, "image", MANDATORY, arg_string,
		"it can be ecos-image/linux-kernel-image"},
	{arg_boot_optimage, "optional-image", OPTIONAL, arg_string,
		"it can be initrd-image in case of linux"},
	{arg_bootmode_usbsnor, "usb|snor|uart", OPTIONAL, arg_string,
		"set boot mode to usb, snor or uart"},
	{arg_getkey_keyname, "keyname", OPTIONAL, arg_string,
		"only display value for the specified keyname"},
	{arg_getccr_keyname, "keyname", OPTIONAL, arg_string,
		"only display deatils for the specified ccr keyname"},
	{arg_setkey_keyname, "keyname", MANDATORY, arg_string,
		"key to set"},
	{arg_setkey_value, "value", MANDATORY, arg_string,
		"value to set the key with"},
	{arg_setkey_value, "value", MANDATORY, arg_string,
		"value to set the key with"},
	{arg_removekey_keyname, "keyname", MANDATORY, arg_string,
		"key to remove from the config area"},
	{arg_erase_file, "file.csv", OPTIONAL, arg_string,
		"CSV file where KEY=VALUE pairs are stored"},
	{arg_erase_size, "size", OPTIONAL, arg_int,
		"left out eeprom size"},
	{arg_help_subcmd, "subcommand", OPTIONAL, arg_string,
		"display help for subcommand"},
	{arg_seteptype_blukisoc, "bulk|isoc", MANDATORY, arg_string,
		"end point configuration, either bulk or isoc"},
	{arg_i2c_inst, "inst", MANDATORY, arg_int,
	        "i2c instance (0 or 1)"},
	{arg_i2c_type, "type", MANDATORY, arg_int,
	        "i2c device type (0: std 8b | 1: std 16b | 2: sccb 8b | "
		"3: sccb 16b | 4: no subaddr)"},
	{arg_i2c_addr, "addr", MANDATORY, arg_string,
		"i2c address (hex)"},
	{arg_i2c_subaddr, "subaddr", OPTIONAL, arg_string,
		"i2c sub address (hex)"},
	{arg_i2c_value, "value", OPTIONAL, arg_string,
		"i2c value (hex)"},
	{arg_tcw_value, "value", OPTIONAL, arg_uint,
		"tcw value"},
	{arg_isp_addr, "addr", MANDATORY, arg_uint,
        	"isp register address"},
	{arg_isp_value, "value", OPTIONAL, arg_uint,
	        "isp value to write"},
	{arg_usb_testmode, "testmode", MANDATORY, arg_uint,
		"USB testmode (0: disabled, 1: TEST_J, 2: TEST_K, "
		"3: TEST_SE0_NAK, 4: TEST_PACKET, 5: TEST_FORCE_ENABLE)"},
	{arg_qcc_bid, "bid", MANDATORY, arg_uint,
        	"QCC block id"},
	{arg_qcc_addr, "addr", MANDATORY, arg_uint,
        	"QCC register address"},
	{arg_qcc_length, "length", MANDATORY, arg_uint,
        	"QCC register length"},
	{arg_qcc_value, "value", OPTIONAL, arg_uint,
	        "QCC value to write"},
	{0, NULL, 0, 0, NULL}
};


/* Structure to hold arguments gathered from command line  */
struct arg_val {
	const struct mxcam_arg *arg;
	const char *val;
};


/* SUBCOMMANDS */
#define DECLARE_SUBCMD(cmd) \
	static int mxcam_subcmd_ ## cmd(struct option_val **optval, struct arg_val **argval)
DECLARE_SUBCMD(list);
DECLARE_SUBCMD(help);
DECLARE_SUBCMD(boot);
DECLARE_SUBCMD(flash);
DECLARE_SUBCMD(bootmode);
DECLARE_SUBCMD(getkey);
DECLARE_SUBCMD(setkey);
DECLARE_SUBCMD(removekey);
DECLARE_SUBCMD(erase);
DECLARE_SUBCMD(info);
DECLARE_SUBCMD(reset);
DECLARE_SUBCMD(seteptype);
DECLARE_SUBCMD(i2cwrite);
DECLARE_SUBCMD(i2cread);
DECLARE_SUBCMD(tcw);
DECLARE_SUBCMD(isp);
DECLARE_SUBCMD(version);
DECLARE_SUBCMD(getccr);
DECLARE_SUBCMD(usbtest);
DECLARE_SUBCMD(qcc);
#if defined(_WIN32)
DECLARE_SUBCMD(init);
DECLARE_SUBCMD(deinit);
#endif
DECLARE_SUBCMD(whoami);
DECLARE_SUBCMD(memtest);

typedef enum {
	FW = 0,
	BOOTLOADER,
	FW_AND_BOOTLOADER,
	UNDEFINED_MODE,
}SUPPORTING_MODE;

/* supporting core_id bitmap */
#define CORE_ID_UNDEFINED	0x0
#define CORE_ID_64180 		0x1
#define CORE_ID_64380 		(0x1<<1)
#define CORE_ID_64480 		(0x1<<2)

struct mxcam_subcmd {
	const char *name;
	const char *help;
	int (*func)(struct option_val **optval, struct arg_val **argval);
	int options[MXCAM_MAX_OPTIONS];
	arg_type_t args[MXCAM_MAX_ARGS];
	SUPPORTING_MODE mode;
	int core_id;
	int cmd_dependency;
};

struct mxcam_subcmd mxcam_subcmds[] = {
	/* list */
	{"list", "lists all compatible devices",
	  mxcam_subcmd_list,
	  {0}, {0}, FW_AND_BOOTLOADER, CORE_ID_64180|CORE_ID_64380, 0
	},
	/* boot */
	{"boot", "boot the camera with specified images",
	  mxcam_subcmd_boot,
	  {0}, {arg_boot_image, arg_boot_optimage}, BOOTLOADER, CORE_ID_64180|CORE_ID_64380,VENDOR_REQ_LAST //dummy dependency TBD
	},
	/* flash */
	{"flash", "flash the specified image(s) on camera",
	  mxcam_subcmd_flash,
	  {opt_fw, opt_bootloader, opt_bootmode,opt_rom,opt_silent}, {0}, FW, CORE_ID_64180|CORE_ID_64380, FW_UPGRADE_START
	},
	/* bootmode */
	{"bootmode", "display or change the existing bootmode",
	  mxcam_subcmd_bootmode,
	  {0}, {arg_bootmode_usbsnor}, FW, CORE_ID_64180|CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* getkey */
	{"getkey", "read the value stored on camera for the given key",
	  mxcam_subcmd_getkey,
	  {opt_vendor}, {arg_getkey_keyname}, FW, CORE_ID_64180|CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* getccr */
	{"getccr", "read the ccr details from camera",
	  mxcam_subcmd_getccr,
	  {0}, {arg_getccr_keyname}, FW, CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* setkey */
	{"setkey", "write the key and value on camera",
	  mxcam_subcmd_setkey,
	  {opt_vendor}, {arg_setkey_keyname, arg_setkey_value}, FW, CORE_ID_64180|CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* removekey */
	{"removekey", "remove the key from config area",
	  mxcam_subcmd_removekey,
	  {opt_vendor}, {arg_removekey_keyname}, FW, CORE_ID_64180|CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* erase */
	{"erase", "erase the config area on camera",
	  mxcam_subcmd_erase,
	  {opt_vendor, opt_silent}, {arg_erase_file, arg_erase_size}, FW, CORE_ID_64180|CORE_ID_64380, SET_EEPROM_KEY_VALUE
	},
	/* info */
	{"info", "prints the information about the camera from usb boot/snor",
	  mxcam_subcmd_info,
	  {opt_snor_hdr, opt_more}, {0}, FW_AND_BOOTLOADER, CORE_ID_64380|CORE_ID_64180, GET_CCR_LIST
	},
	/* reset */
	{"reset", "reset the camera",
	  mxcam_subcmd_reset,
	  {0}, {0}, FW, CORE_ID_64180|CORE_ID_64380, RESET_BOARD
	},
	/* help */
	{"help", "describe the usage of mxcam or subcommands",
		  mxcam_subcmd_help,
	  {0}, {arg_help_subcmd}, FW_AND_BOOTLOADER, CORE_ID_64180|CORE_ID_64380, 0
	},
	/* seteptype */
	{"seteptype", "set the end point of camera to the specified type",
	  mxcam_subcmd_seteptype,
	  {0}, {arg_seteptype_blukisoc}, FW, CORE_ID_64180|CORE_ID_64380, 0
	},
	/* i2cwrite */
	{"i2cwrite", "program i2c device on camera",
	  mxcam_subcmd_i2cwrite,
	  {opt_i2cfile}, {arg_i2c_inst, arg_i2c_type, arg_i2c_addr, arg_i2c_subaddr, arg_i2c_value}, FW,
	  CORE_ID_64380, I2C_WRITE
	},
        /* i2cread*/
        {"i2cread", "read i2c device from camera",
          mxcam_subcmd_i2cread,
	  {opt_i2cfile}, {arg_i2c_inst, arg_i2c_type, arg_i2c_addr, arg_i2c_subaddr}, FW,
	  CORE_ID_64380, I2C_READ
        },
        /* tcw */
        {"tcw", "read/write spi device's timing control word value of camera",
          mxcam_subcmd_tcw,
          {opt_silent}, {arg_tcw_value}, FW, CORE_ID_64380, TCW_WRITE
        },
	/* isp */
	{"isp", "read/program ISP register",
    	  mxcam_subcmd_isp,
     	 {opt_isprcnt, opt_isppause}, {arg_isp_addr, arg_isp_value}, FW, CORE_ID_64380, ISP_READ
    	},
	/* version */
	{"version", "display the version of mxcam",
	 mxcam_subcmd_version,
	 {0}, {0}, FW_AND_BOOTLOADER, CORE_ID_64180|CORE_ID_64380, 0
	},
	/* usbtest */
	{"usbtest", "set USB test mode",
	  mxcam_subcmd_usbtest,
	  {0}, {arg_usb_testmode}, FW, CORE_ID_64180|CORE_ID_64380, 0
	},
	/* qcc */
	{"qcc", "read/write QCC registers",
    	  mxcam_subcmd_qcc,
     	 {0}, {arg_qcc_bid, arg_qcc_addr, arg_qcc_length, arg_qcc_value}, FW, CORE_ID_64380, QCC_WRITE
    	},
#if defined(_WIN32)
	/* init */
	{	"init", 
		"initialize the winusb setup for the device",
		mxcam_subcmd_init,
		{opt_vid, opt_pid}, {0}, FW, CORE_ID_64380, 0
	},
	/* deinit */
	{	"deinit", 
		"deinitialize the winusb setup",
		mxcam_subcmd_deinit,
		{0}, {0}, FW, CORE_ID_64380, 0
	},
#endif
	{"whoami", "informations about maxim camera and supported commands",
	  mxcam_subcmd_whoami,
	  {0}, {0}, FW_AND_BOOTLOADER, CORE_ID_64180|CORE_ID_64380, GET_CMD_BITMAP
	},
	{"memtest", "DDR memory test",
	  mxcam_subcmd_memtest,
	  {opt_ddr_size, opt_fullmem, opt_loadmem, opt_duration}, {arg_boot_image}, 
	  FW_AND_BOOTLOADER, CORE_ID_64380, 0
	},
	{NULL, NULL, NULL, {0}, {0}, UNDEFINED_MODE, CORE_ID_UNDEFINED, 0}
};

/* global pointer to store scanned device list pointer */
struct mxcam_devlist *gdevlist = NULL;

static const struct mxcam_option* get_option_from_id(mxcam_longopt_id_t id)
{
	int i;
	for (i=0; mxcam_global_options[i].long_name; i++) {
		if (mxcam_global_options[i].id == id)
			return &mxcam_global_options[i];
	}
	for (i=0; mxcam_options[i].long_name; i++) {
		if (mxcam_options[i].id == id)
			return &mxcam_options[i];
	}
	return NULL;
}

static const struct mxcam_arg* get_arg_from_id(mxcam_arg_id_t id)
{
	int i;
	for (i=0; mxcam_args[i].id; i++) {
		if (mxcam_args[i].id == id)
			return &mxcam_args[i];
	}
	return NULL;
}

static struct mxcam_subcmd* get_subcmd_from_name(const char *name)
{
	int i;
	for (i=0; mxcam_subcmds[i].name; i++) {
		if (strcmp(mxcam_subcmds[i].name, name) == 0)
			return &mxcam_subcmds[i];
	}
	return NULL;
}

static int has_option(mxcam_longopt_id_t id, struct option_val **optval)
{
	int i;
	for(i=0; optval[i]; i++) {
		if(optval[i]->opt->id == id) {
			return 1;
		}
	}
	return 0;
}
static const void* get_value_from_option(mxcam_longopt_id_t id, struct option_val **optval)
{
	int i;
	for(i=0; optval[i]; i++) {
		if(optval[i]->opt->id == id) {
			if(optval[i]->opt->arg_type == arg_string)
				return (const void*) optval[i]->val;
			else if(optval[i]->opt->arg_type == arg_int)
				return (const void*)(intptr_t) atoi(optval[i]->val);
		}
	}

	/* Option not found: exit.
	 * If the option is mandatory, it presence should have already been
	 * check during the options parsing phase.
	 * If the option is optional, its presence should be checked using
	 * has_option() function before using this function */
	printf("Unexpected error in %s():\nOption (id = '%i') not found on "
			"command line.\n", __func__ , id);
	exit(1);
}

static int has_arg(mxcam_arg_id_t id, struct arg_val **argval)
{
	int i;
	for(i=0; argval[i]; i++) {
		if(argval[i]->arg->id == id) {
			return 1;
		}
	}
	return 0;
}
static const void* get_value_from_arg(mxcam_arg_id_t id, struct arg_val **argval)
{
	int i;
	for(i=0; argval[i]; i++) {
		if(argval[i]->arg->id == id) {
			if(argval[i]->arg->type == arg_string)
				return (const void*) argval[i]->val;
			else if(argval[i]->arg->type == arg_int)
				return (const void*)(intptr_t) atoi(argval[i]->val);
			else if(argval[i]->arg->type == arg_uint)
				return (const void*)(uintptr_t) strtoul(argval[i]->val, NULL, 16);
		}
	}

	/* Argument not found: exit.
	 * If the argument is mandatory, it presence should have already been
	 * check during the arguments parsing phase.
	 * If the argument is optional, its presence should be checked using
	 * has_arg() function before using this function */
	printf("Unexpected error in %s():\nArgument (id = '%i') not found on "
			"command line.\n", __func__ , id);
	exit(1);
}

static void available_subcommands()
{
	int i;

	printf("Usage:");
	printf("mxcam <subcommands> [options] [arguments]\n");
	printf("Type 'mxcam help <subcommand>' for help on a specific subcommand\n\n");
	printf("Available subcommands:\n");

	for(i=0; mxcam_subcmds[i].name; i++) {
		printf_bold("  %-22s: ", mxcam_subcmds[i].name);
		printf("%s\n", mxcam_subcmds[i].help);
	}
	for (i=0; mxcam_global_options[i].long_name; i++) {
		if (i == 0)
			printf("\nGlobal options:\n");
		printf("  --%-20s: ", mxcam_global_options[i].long_name);
		printf("%s\n", mxcam_global_options[i].help);
	}
}

/* Check Command Support */
int mxcam_check_command_support(const char *name, char *cmd_bitmap)
{
	int i=0;
	for (i=0; mxcam_subcmds[i].name != NULL; i++){
		if(strcmp(name, mxcam_subcmds[i].name)==0){
			if (mxcam_subcmds[i].cmd_dependency == 0)
				return MXCAM_OK;
			else if (*(cmd_bitmap + mxcam_subcmds[i].cmd_dependency) == 1)
				return MXCAM_OK;
			else 
				return MXCAM_ERR_FAIL;
		}
	}

	return MXCAM_ERR_FAIL;
}

static int open_device(struct option_val **optval, 
			struct mxcam_devlist *devlist);
/* scan for device list */
struct mxcam_devlist *get_device(struct option_val **optval)
{
	int r = 0;
	struct mxcam_devlist *devlist = NULL;

	while(r == 0) {
		if(has_option(opt_oldcam, optval))
			r = mxcam_scan_oldcam(&devlist);
		else
			r = mxcam_scan(&devlist);
		if(r > 0)
			break;
		else if(has_option(opt_poll, optval)) {
			sleep(1);
			continue;
		} else {
			PRINTM("No Compatible device found.\n");
			if(!has_option(opt_oldcam, optval)) {
				PRINTM("You might want to use %c[1m--oldcam"
					"%c[0m option to communicate with "
					"older generations Maxim cameras.\n",
					27, 27);
			}
			devlist=NULL;
			break;
		}
	}

	return devlist;
}

/* centralized command support check */
int check_command_support(const char *subcmd_name, 
			struct option_val **optval,
			struct mxcam_subcmd *subcmd, 
			struct mxcam_devlist *devlist)
{
	char cmd_bitmap[256];
	int r =0;
	int chip_id = 0;
	int mode = 0;

	memset(cmd_bitmap, 0, 256);
		
	r = open_device(optval, devlist);
	if (r) {
		//PRINTM("Unable to open device\n");
		return MXCAM_ERR_INVALID_DEVICE;
	}	
	if (devlist->type == DEVTYPE_BOOT)
	{
		mode = 	BOOTLOADER;
	}else if (devlist->type == DEVTYPE_UVC)
	{
		mode = FW;
	}else {
		PRINTM("Invalid device\n");
		return MXCAM_ERR_INVALID_DEVICE;
	}
	chip_id = devlist->soc;
	if ((chip_id == MAX64380) || (chip_id == MAX64480)){
		r = mxcam_get_cmd_bitmap(cmd_bitmap);
		r = mxcam_check_command_support(subcmd_name, cmd_bitmap);
		if(r == MXCAM_OK)
			return MXCAM_OK; //success
		else {
			//it's a old fw/bootloader of raptor
			if((mode == FW)
			&& ((subcmd->mode == FW) || (subcmd->mode == FW_AND_BOOTLOADER)))
				return MXCAM_OK; //success	
			else if ((mode == BOOTLOADER) 
			&& ((subcmd->mode == BOOTLOADER) || 
			(subcmd->mode == FW_AND_BOOTLOADER)))
				return MXCAM_OK; //success
			else
				return MXCAM_ERR_FAIL; //failed	
		}	
	}
	else
	if ((chip_id == MAX64180) || (mode == BOOTLOADER)){
		if (
		((mode == BOOTLOADER) 
		&& ((subcmd->mode == BOOTLOADER) || (subcmd->mode == FW_AND_BOOTLOADER)))		   			||
		((mode == FW)
		&& ((subcmd->mode == FW) || (subcmd->mode == FW_AND_BOOTLOADER)))
		){
			if (((chip_id == MAX64180) && (subcmd->core_id & CORE_ID_64180)) ||
			((chip_id == MAX64380) && (subcmd->core_id & CORE_ID_64380)) ||
			((chip_id == MAX64480) && (subcmd->core_id & CORE_ID_64480)))
				return MXCAM_OK; //success
		}	
	}

	return MXCAM_ERR_FAIL;
			
}

int main(int in_argc, char ** in_argv) {

	int ret, i=0, j=0, r=0;
	struct option_val *options_val[MXCAM_MAX_OPTIONS] = {NULL};
	struct arg_val *args_val[MXCAM_MAX_OPTIONS] = {NULL};
	struct mxcam_subcmd *subcmd;
	const char *subcmd_name;
	struct option long_options[MXCAM_MAX_TOTAL_OPTIONS];
	struct mxcam_devlist *devlist = NULL;
	int argc;
	char **argv;

	argc = in_argc;
	argv = malloc(sizeof(char*)*(argc+1));
	for(i=0;i<argc;i++){
		argv[i] = malloc(768);
		
#if !defined(_WIN32)
		strncpy(argv[i],in_argv[i], 768);
#else
		strcpy_s(argv[i], 768, in_argv[i]);
#endif

	}
	argv[i] = NULL;
	i=0;

	/* Fill getopt structure with global and non global options */
	while(mxcam_global_options[i].long_name) {
		long_options[i].name = mxcam_global_options[i].long_name;
		long_options[i].has_arg = mxcam_global_options[i].has_arg;
		long_options[i].flag = NULL;
		long_options[i].val = mxcam_global_options[i].id;
		i++;
	}
	while(mxcam_options[j].long_name) {
		long_options[i].name = mxcam_options[j].long_name;
		long_options[i].has_arg = mxcam_options[j].has_arg;
		long_options[i].flag = NULL;
		long_options[i].val = mxcam_options[j].id;
		i++;
		j++;
	}
	long_options[i].name = NULL;
	long_options[i].has_arg = 0;
	long_options[i].flag = NULL;
	long_options[i].val = 0;

	/* Parse and process options */
	for(i=0;;) {
		int c, idx;
		struct option_val *optval;

		assert(i < MXCAM_MAX_OPTIONS);
		c = getopt_long(argc, argv, "", long_options, &idx);

		/* No more options */
		if (c == -1)
			break;

		switch(c) {
		case '?': /* Invalid option */
			if(c == '?') {
				ret = 1;
				goto main_out;
			}
			break;

		case 0: /* Flag set */
			break;

		default: /* Process option */
			optval = malloc(sizeof(struct option_val));
			optval->opt = get_option_from_id(c);
			assert(optval->opt != NULL);
			optval->val = optarg;
			options_val[i] = optval;
			i++;
		}
	}
	options_val[i] = NULL;

	/* Parse and process SUBCOMMAND */

	/* No SUBCOMMAND specified: display help */
	if(optind >= argc) {
		available_subcommands();
		ret = 1;
		goto main_out;
	}

	/* Retrieve and check SUBCOMMAND */
	subcmd_name = argv[optind];
	subcmd = get_subcmd_from_name(subcmd_name);
	if (subcmd == NULL) {
		printf("Unknown subcommand '%s'\n", subcmd_name);
		ret = 1;
		goto main_out;
	}

	/* Check that all required OPTIONS for this SUBCOMMAND were given */
	for(i=0; subcmd->options[i]; i++) {
		const struct mxcam_option *opt;
		int found=0;
		opt = get_option_from_id(subcmd->options[i]);
		assert(opt != NULL);

		/* Skip optional options */
		if(opt->is_optional == 1)
			continue;

		/* Browse through the OPTIONS given on the command line to find
		 * a match */
		for(j=0; options_val[j]; j++) {
			if(options_val[j]->opt->id != opt->id)
				continue;
			found=1;
			break;
		}

		/* No match */
		if(found==0) {
			printf("Option '--%s' is required for subcommand '%s'\n",
					opt->long_name, subcmd_name);
			ret = 1;
			goto main_out;
		}
	}

	/* Parse and process ARGUMENTS given on the command line */
	i=optind+1;
	for(j=0; subcmd->args[j]; j++) { /* FIXME: unstable? */
		const struct mxcam_arg *arg;
		struct arg_val *argval;
		if(i >= argc)
			break;
		argval = malloc(sizeof(struct arg_val));
		arg = get_arg_from_id(subcmd->args[j]);
		assert(arg != NULL);
		argval->arg = arg;
			argval->val = argv[i];
		args_val[j] = argval;
		i++;
	}
	args_val[j]=NULL;

	/* Check that no mandatory ARGUMENT are missing */
	for(;subcmd->args[j]; j++) {
		const struct mxcam_arg *arg;
		arg = get_arg_from_id(subcmd->args[j]);
		assert(arg != NULL);
		if(arg->is_optional == MANDATORY) {
			printf("Argument <%s> is required for subcommand '%s'\n",
					arg->name, subcmd_name);
			ret = 1;
			goto main_out;
		}

	}

	if((strcmp(subcmd_name, "help")==0) || (strcmp(subcmd_name, "version")==0)) {
		ret = subcmd->func(options_val, args_val);
		return 0;	
	}

	/* Check for verbose global OPTION */
	if (has_option(opt_verbose, options_val)){
		verbose=1;			
	}

#if defined(_WIN32)
	if((strcmp(subcmd_name,"init")==0)||
		(strcmp(subcmd_name,"deinit")==0)){
		ret = subcmd->func(options_val, args_val);
		mxcam_close();
		goto main_out;
	}
#endif

	/* scan & get the device list */
	devlist = get_device(options_val);

	while(devlist){
		if (devlist==NULL)
			break;

		gdevlist = devlist;
		/* check if this command is supported in camera */
		r = check_command_support(subcmd_name, options_val, subcmd, devlist);
		if (MXCAM_OK == r)
		{	
			/* Execute subcommand */
			ret = subcmd->func(options_val, args_val);
			break;
		}else if (MXCAM_ERR_DEVICE_NOT_FOUND == r){
			printf_bold("Please connect a compatible Maxim Camera\n");
		}else if (MXCAM_ERR_INVALID_DEVICE == r){
			//TBD
		}else {
			printf_bold("This command is not supported\n");
			printf_bold("use 'mxcam whoami' to find supported commands\n");
			break;
		}
		if(strcmp(subcmd_name, "help")==0 || strcmp(subcmd_name, "list")==0)
			break;
	
		devlist=devlist->next;
	}

	mxcam_close();
main_out:
	/* Free ressources */
	for(j=0; args_val[j] != NULL; j++)
		free(args_val[j]);
	for(j=0; options_val[j] != NULL; j++)
		free(options_val[j]);
	if (argv != NULL) {
		for (i = 0; i < argc; i++) {
			if (argv[i] != NULL)
				free(argv[i]);
		}
		free(argv);
	}

	return ret;
}

static int open_device(struct option_val **optval,
			struct mxcam_devlist *devlist)
{
	int ret;

	//if(has_option(opt_poll, optval))
	//	return mxcam_poll_one(devlist);
	//else if (poll_new)
	//	ret = mxcam_poll_new();
	if(has_option(opt_bus, optval) || has_option(opt_addr, optval)) {
		int dev_bus, dev_addr;
		if(!has_option(opt_bus, optval)) {
			printf("The bus number (--bus option) must be specified"
					"together with the device address\n");
			return 1;
		}
		if (!has_option(opt_addr, optval)) {
			printf("The device address (--addr option) must be specified"
					"together with the bus number\n");
			return 1;
		}
		dev_bus  = (intptr_t) get_value_from_option(opt_bus,  optval);
		dev_addr = (intptr_t) get_value_from_option(opt_addr, optval);
		PRINTV("Selecting device at bus %i and address %i\n", dev_bus, dev_addr);
		ret = mxcam_open_by_busaddr(dev_bus, dev_addr, devlist);
		if(ret == MXCAM_ERR_INVALID_DEVICE){
			printf("Not able to find device with bus %i and address %i\n",
								dev_bus, dev_addr);
			printf("Please use 'mxcam list' to find all compatible devices\n");
		}
		return ret;
	}
	else {
		int dev_num=1;
		if(has_option(opt_device, optval))
			dev_num = (intptr_t) get_value_from_option(opt_device, optval);
		PRINTV("Selecting device #%i\n", dev_num);
		ret = mxcam_open_by_devnum(dev_num, devlist);
		if(ret == MXCAM_ERR_INVALID_DEVICE){
			printf("Not able to find device %d\n",dev_num);
			printf("Please use 'mxcam list' to find all compatible devices\n");
		}
		return ret;
	}

	/* Impossible */
	abort();
}

/* SUBCOMMANDs implementation */

#if defined(_WIN32)

/* INIT */
static int mxcam_subcmd_init(struct option_val **optval, struct arg_val **argval)
{
	char *pVal;
	short vid=RAPTOR_VID;
	short pid=RAPTOR_PID;

	if(has_option(opt_vid, optval)){
		pVal = get_value_from_option(opt_vid, optval);
		if(pVal){
			vid = (short)strtoul(pVal, NULL, 16);
			PRINTV("vid 0x%04X\n", (vid&0xFFFF));
		}
	}

	if(has_option(opt_pid, optval)){
		pVal = get_value_from_option(opt_pid, optval);
		if(pVal){
			pid = (short)strtoul(pVal, NULL, 16);
			PRINTV("pid 0x%04X\n", (pid&0xFFFF));
		}
	}
	
	DevInstall(vid,pid);
	return 0;
}

/* DEINIT */
static int mxcam_subcmd_deinit(struct option_val **optval, struct arg_val **argval)
{
	int i=0, ret;
	short vid=0;
	short pid=0;

	struct mxcam_devlist *devlist;

	PRINTV("%s (IN)\n",__func__);

	devlist = gdevlist;

	while(devlist != NULL) {
		PRINTV("vid : 0x%04X\n", devlist->vid);
		PRINTV("pid : 0x%04X\n", devlist->pid);

		vid = devlist->vid;
		pid = devlist->pid;
		devlist = devlist->next;
	}

	DevUninstall(vid, pid);
	return 0;
}

#endif

/* LIST */
static int mxcam_subcmd_list(struct option_val **optval, struct arg_val **argval)
{
	int i=0, ret;
	struct mxcam_devlist *devlist;

	PRINTV("%s (IN)\n",__func__);

	ret = 0;
	devlist = gdevlist;;

	while(devlist != NULL) {
		i++;
		printf("device #%i:\n", i);
		if(((devlist->soc == MAX64380) || (devlist->soc == MAX64480)) && 
				devlist->type == DEVTYPE_UVC) {
			uint32_t rev;
			mxcam_open_by_devnum(i, devlist);
			mxcam_qcc_read(0x6, 0xfc, 4, &rev);
			switch(rev) {
			case 0x500:
				printf("\tCore: MAX64380 (A0)\n");
				break;
			case 0x510:
				if (devlist->soc == MAX64480)
					printf("\tCore: MAX64480\n");
				else
					printf("\tCore: MAX64380 (B0)\n");
				break;
			default:
				printf("\tCore: MAX64380 (Unknown)\n");
				break;
			}
		} else {
			printf("\tCore: %s\n", (devlist->soc == MAX64380) ?
				"MAX64380" : 
				((devlist->soc == MAX64480) ? "MAX64480" : "MAX64180"));
		}
		printf("\tState: %s\n", devlist->type == DEVTYPE_BOOT ?
				"Waiting for USB boot" :
				"Booted");
		printf("\tID: %.4x:%.4x\n", devlist->vid, devlist->pid);
		printf("\tBus number: %i\n", devlist->bus);
		printf("\tDevice address: %i\n", devlist->addr);
		devlist = devlist->next;
	}

	return ret;
}

/* HELP */
static void print_usage(struct mxcam_subcmd *cmd)
{
	int i=0;

	if (cmd == NULL) {
		printf("\nType 'mxcam help' for usage\n");
		return;
	}

	printf("Description:\n  %s\n\n", cmd->help);
	printf("Usage:\n  mxcam %s ", cmd->name);
	for(i=0; i<MXCAM_MAX_ARGS && cmd->args[i]; i++) {
		const struct mxcam_arg *arg;
		arg = get_arg_from_id(cmd->args[i]);
		assert(arg != NULL);
		if (arg->is_optional)
			printf("[%s] ", arg->name);
		else
			printf("<%s> ", arg->name);
	}
	for (i=0; cmd->options[i]; i++) {
		const struct mxcam_option *opt;
		opt = get_option_from_id(cmd->options[i]);
		assert(opt != NULL);
		if (opt->is_optional)
			printf("[");
		printf("--%s", opt->long_name);
		if (opt->has_arg == required_argument)
			printf(" <%s>", opt->arg_str);
		else if(opt->has_arg == optional_argument)
			printf(" [%s]", opt->arg_str);
		if (opt->is_optional)
			printf("]");
		printf(" ");

	}
	printf("\n");

	for(i=0; i<MXCAM_MAX_ARGS && cmd->args[i]; i++) {
		const struct mxcam_arg *arg;
		if (i == 0)
			printf("\nValid arguments:\n");
		arg = get_arg_from_id(cmd->args[i]);
		assert(arg != NULL);
		printf("  %-22s: ", arg->name);
		printf("%s\n", arg->help);

	}
	for (i=0; cmd->options[i]; i++) {
		const struct mxcam_option *opt;
		if (i == 0)
			printf("\nValid options:\n");
		opt = get_option_from_id(cmd->options[i]);
		assert(opt != NULL);
		printf("  --%-20s: ", opt->long_name);
		printf("%s\n", opt->help);
	}
};

static int mxcam_subcmd_help(struct option_val **optval, struct arg_val **argval)
{
	const char *sc;
	struct mxcam_subcmd *subcmd;

	/* No subcommand arg given on command line. Display general help */
	if(!has_arg(arg_help_subcmd, argval)) {
		available_subcommands();
		return 0;
	}

	sc = get_value_from_arg(arg_help_subcmd, argval);

	subcmd = get_subcmd_from_name(sc);
	/* Unknown subcommand */
	if(subcmd == NULL) {
		printf("Cannot display help for unknown subcommand '%s'.\n", sc);
		return 1;
	}

	print_usage(subcmd);
	return 0;
}

/* BOOT */
static void mxcam_fw_print_status(FW_STATE st, const char *filename)
{
	if(st == FW_STARTED){
		fprintf(stdout,"Sending %s...",filename);
		fflush(stdout);
	}
	if(st == FW_COMPLETED){
		fprintf(stdout, " : %s\n", "Done");
		fflush(stdout);
	}
}
static int mxcam_subcmd_boot(struct option_val **optval, struct arg_val **argval)
{
	int r;
	const char *image=NULL, *opt_image=NULL;

	PRINTV("%s (IN)\n",__func__);

	image = get_value_from_arg(arg_boot_image, argval);
	if(has_arg(arg_boot_optimage, argval))
		opt_image = get_value_from_arg(arg_boot_optimage, argval);

	r = mxcam_boot_firmware(image, opt_image,
			mxcam_fw_print_status);
	if(r){
		PRINTM("%s\n",mxcam_error_msg(r));
		return 1;
	}
	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* FLASH */
static void wait_for_upgrade_complete(void)
{
	unsigned char wrt_st;
	int r;
	char circle[]={'-','\\','|','/'};
	int cnt = 0;
	while(1) {
		sleep(1);
		r = mxcam_read_nvm_pgm_status(&wrt_st);
		if (r<0) {
			PRINTM("%s\n",mxcam_error_msg(r));
			break;
		}
		if(wrt_st >= MXCAM_ERR_FAIL) {
			PRINTM("%s\n",mxcam_error_msg(r));
			break;
		}
		if (wrt_st < MXCAM_ERR_FAIL) {
			PRINTM("\rFirmware upgrade in progress: %s : %c",
					mxcam_error_msg(wrt_st),circle[cnt++]);
			fflush(stdout);
			if(3 == cnt)
				cnt = 0;
		}
		if(wrt_st == 0) {
			PRINTM("\rCompleted firmware upgrade...:)             "
					"                                           \n");
			fflush(stdout);
			break;
		}
	}
}
static int mxcam_subcmd_flash(struct option_val **optval, struct arg_val **argval)
{
	int r;
	fw_info *fw;

	PRINTV("%s (IN)\n",__func__);

	/* Allocate memory */
	fw = (fw_info *)malloc(sizeof(fw_info));
	if (fw){
		memset(fw,0,sizeof(fw_info));
	} else {
		PRINTM("Unable to allocate memory!\n");
		return 1;
	}

	/* Get options */
	if(has_option(opt_fw, optval)) {
		fw->image = get_value_from_option(opt_fw, optval);
		fw->img_media = SNOR ;
		PRINTV("firmware %s\n", fw->image);
	}else {
		fw->image = NULL;
	}
	if(has_option(opt_bootloader, optval)) {
		fw->bootldr = get_value_from_option(opt_bootloader, optval);
		fw->bootldr_media = EEPROM;
		PRINTV("bootloader %s\n", fw->bootldr);
	} else {
		fw->bootldr = NULL;
	}
	if(has_option(opt_rom, optval)) {
		fw->bootldr = get_value_from_option(opt_rom, optval);
		fw->bootldr_media = EEPROM;
		PRINTV("rom image %s\n", fw->bootldr);
	}
	if((has_option(opt_fw, optval) && has_option(opt_rom, optval))
		||(!has_option(opt_bootloader, optval) &&
		!has_option(opt_fw, optval) && !has_option(opt_rom, optval))
		||(has_option(opt_bootloader, optval) &&
		!has_option(opt_fw, optval) && has_option(opt_rom, optval))){
		PRINTM("\nVaild firmware upgrade combinations are\n"
				"1)bootloader(--bootloader) only\n"
				"2)firmware (--fw) only\n"
				"3)firmware and bootloader\n"
				"4)rom image(--rom) only\n");
		free(fw);
		return 1;
	}
	if(has_option(opt_bootmode, optval)){
		const char *mode = get_value_from_option(opt_bootmode, optval);
		if(strcmp(mode,"snor") == 0)
			fw->mode = MODE_SNOR;
		else
			fw->mode = MODE_USB;
		PRINTV("bootmode %s\n", mode);
	} else {
		fw->mode = MODE_NONE;
	}

	/* If no --silent option, ask for confirmation before upgrade */
	if(!has_option(opt_silent, optval)) {
		char c;
		printf("Do you want to upgrade the camera firmware? [y/N]");
		c = getchar();
		if (c != 'y' && c != 'Y') {
			free(fw);
			return 1;
		}
	}
	/* Start upgrading  */
	if(has_option(opt_rom, optval)) {
		r = mxcam_upgrade_firmware(fw, mxcam_fw_print_status,1);
	} else {
		r = mxcam_upgrade_firmware(fw, mxcam_fw_print_status,0);
	}
	if(r){
		PRINTM("%s\n",mxcam_error_msg(r));
		free(fw);
		return 1;
	}
	wait_for_upgrade_complete();
	free(fw);
	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* BOOTMODE */
static int mxcam_subcmd_bootmode(struct option_val **optval, struct arg_val **argval)
{
	int ret=1;

	PRINTV("%s (IN)\n",__func__);

	/* Set key to specified bootmode */
	if(has_arg(arg_bootmode_usbsnor, argval)) {
		const char *bootmode;
		bootmode = get_value_from_arg(arg_bootmode_usbsnor, argval);
		if(strcmp(bootmode, "snor") == 0)
			ret = mxcam_set_key(MAXIM_INFO, "BOOTMODE", "snor");
		else if(strcmp(bootmode, "usb") == 0)
			ret = mxcam_set_key(MAXIM_INFO, "BOOTMODE", "usb");
		else if(strcmp(bootmode, "uart") == 0)
			ret = mxcam_set_key(MAXIM_INFO, "BOOTMODE", "uart");
		else {
			printf("Valid Bootmodes are usb/snor/uart \n");
			return 1;
		}

		if(!ret)
			ret = mxcam_save_eeprom_config(MAXIM_INFO);

	/* No bootmode specified: display current bootmode */
	} else {
		char *value;
		ret = mxcam_get_value(MAXIM_INFO, "BOOTMODE", &value);
		if(!ret) {
			printf("%s\n", value);
			mxcam_free_get_value_mem(value);
		}
	}

	/* Error handling */
	if(ret){
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* GETKEY */
static void get_all_keys(GET_ALL_KEY_STATE st, int keycnt, void *data1, void *data2)
{
	if (st == GET_ALL_KEY_NO_KEY) {
		PRINTM("Found an empty config area\n");
	} else if (st == GET_ALL_KEY_VALID){
		PRINTM("%s=%s\n",(char *)data1,(char *)data2);
	} else if ( st == GET_ALL_KEY_COMPLETED) {
		PRINTM("\nkey count   : %d \nconfig size : %d/%d bytes\n",
				keycnt,*(unsigned short *)data1,
				*(unsigned short *)data2);
	}
}

static int mxcam_subcmd_getkey(struct option_val **optval, struct arg_val **argval)
{
	int ret=1;
	CONFIG_AREA area;

	PRINTV("%s (IN)\n",__func__);

	/* Check from the options what config area to work on */
	if(has_option(opt_vendor, optval))
		area = VENDOR_INFO;
	else
		area = MAXIM_INFO;

	/* Display specified key */
	if(has_arg(arg_getkey_keyname, argval)) {
		const char *key;
		char *value;

		key = get_value_from_arg(arg_getkey_keyname, argval);
		ret = mxcam_get_value(area, key, &value);
		if(!ret)
			printf("%s\n", value);
		mxcam_free_get_value_mem(value);
	/* No key specified: display them all */
	} else {
		ret = mxcam_get_all_key_values(area, get_all_keys);
		/* FIXME: free ressources ?*/
	}

	/* Error handling */
	if(ret){
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* GETCCR */
static void get_all_ccrs(GET_ALL_KEY_STATE st, int keycnt, void *data)
{
	if (st == GET_ALL_KEY_NO_KEY) {
		PRINTM("Found an empty CCR list\n");
	} else if (st == GET_ALL_KEY_VALID){
		PRINTM("%s\n",(char *)data);
	} else if ( st == GET_ALL_KEY_COMPLETED) {
		PRINTM("\nkey count   : %d \n",keycnt);
	}
}

static void mxcam_print_ccrheader(CONFIG_AREA area)
{
    char str[8];
    
    if (area == MAXIM_INFO)
        strcpy(str, "MAXIM");
    else if (area == VENDOR_INFO)
        strcpy(str, "VENDOR");
    
	printf("-------------------------------\n");
    if (area != 0)
        printf(" %s AREA CCR LIST\n", str);
	printf(" key, default, description\n");
	printf("-------------------------------\n");
}

/* GETCCR */
static int mxcam_subcmd_getccr(struct option_val **optval, struct arg_val **argval)
{
	int ret=1;
    CONFIG_AREA area;

	PRINTV("%s (IN)\n",__func__);

    
    /* Check from the options what config area to work on */
    if(has_option(opt_vendor, optval))
        area = VENDOR_INFO;
    else
        area = MAXIM_INFO;
        
	/* Display specified key */
	if(has_arg(arg_getccr_keyname, argval)) {
		const char *key;
		char *value;
		key = get_value_from_arg(arg_getccr_keyname, argval);
		//ret = mxcam_get_value(area, key, &value);
		ret = mxcam_get_ccrvalue(key, &value);
                if(value != NULL)
                        mxcam_print_ccrheader(0);
		if(!ret)
			printf("%s\n", value);

		mxcam_free_get_value_mem(value);
	/* No key specified: display them all */
	} else {
		mxcam_print_ccrheader(area);
		ret = mxcam_get_all_ccr(area, get_all_ccrs);
		/* FIXME: free ressources ?*/
	}

	/* Error handling */
	if(ret){
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* SETKEY */
static int mxcam_subcmd_setkey(struct option_val **optval, struct arg_val **argval)
{
	int ret=1;
	CONFIG_AREA area;
	const char *keyname, *keyval;
	PRINTV("%s (IN)\n",__func__);

	/* Check from the options what config area to work on */
	if(has_option(opt_vendor, optval))
		area = VENDOR_INFO;
	else
		area = MAXIM_INFO;

	/* Set key to specified value */
	keyname = get_value_from_arg(arg_setkey_keyname, argval);
	keyval  = get_value_from_arg(arg_setkey_value , argval);
	ret = mxcam_set_key(area, keyname, keyval);
	/* Save changes in the eeprom */
	if(!ret)
		ret = mxcam_save_eeprom_config(area);

	/* Error handling */
	if(ret){
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* REMOVEKEY */
static int mxcam_subcmd_removekey(struct option_val **optval, struct arg_val **argval)
{
	const char *keyname;
	int ret=1;
	CONFIG_AREA area;

	PRINTV("%s (IN)\n",__func__);

	/* Check from the options what config area to work on */
	if(has_option(opt_vendor, optval))
		area = VENDOR_INFO;
	else
		area = MAXIM_INFO;

	/* Set key to specified value */
	keyname = get_value_from_arg(arg_removekey_keyname, argval);
	ret = mxcam_remove_key(area, keyname);
	/* Save changes in the eeprom */
	if(!ret)
		ret = mxcam_save_eeprom_config(area);

	/* Error handling */
	if(ret){
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* ERASE */
static int csv_parse(char *record, const char *delim,
		void (*callbk)(char *key,char *value,void *data),void *data1)
{
	char *key,*value,*tmp;
	int i;

#if defined(_WIN32)
	char *NextToken;
#endif

	PRINTV("%s (IN)\n",__func__);

#if !defined(_WIN32)
	key=strtok(record,delim);
#else
	key=strtok_s(record,delim,&NextToken);
#endif

	if(!key) {
		PRINTM("[csv-parser] 1st element is blank\n");
		return -1;
	}
#if !defined(_WIN32)
	value=strtok('\0',delim);
#else
	value=strtok_s('\0',delim,&NextToken);
#endif

	if(!value) {
		PRINTM("[csv-parser] 2nd element is blank\n");
		return -1;
	}
#if !defined(_WIN32)
	tmp=strtok('\0',delim);
#else
	tmp=strtok_s('\0',delim,&NextToken);
#endif
	if(tmp){
		PRINTM("[csv-parser] record has more than 2 elements\n");
		return -1;
	}

	/* checking for " and ' in the string key and value,
	   if present remove them */

	if ( *(key+0) == '\"' || *(key+0) == '\'')
	{
		for(i=0;i<(int)(strlen(key)-2);i++)
			*(key+i)=*(key+i+1);
		*(key+strlen(key)-2)='\0';
	}

	if ( *(value+0) == '\"' || *(value+0) == '\'')
	{
		for(i=0;i<(int)(strlen(value)-2);i++)
			*(value+i)=*(value+i+1);
		*(value+strlen(value)-2)='\0';
	}
	callbk(key,value,data1);
	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* function for parsing a csv file
 * it checks for 2 elements in each column.
 * it throws error if there are more than 2 coloumns in each row.
 * it throws error if there are any blank coloumns in each row .
 * it accepts a filename of the csv file and also a callback function.
 * once the function finds 2 elements in a certain row, it calls the callback function
 * with the 2 elements and data as arguments .
 */
static int csv_file_parser(const char *fname,
		void (*callbk)(char *key,char *value,void *data),void *data1)
{
	char *tmp;
	int recordcnt=0,k,size;
	FILE *in;
#if defined(_WIN32)
	int ret;
#endif

	PRINTV("%s (IN)\n",__func__);

#if !defined(_WIN32)	
	in=fopen(fname,"r");
#else
	ret=fopen_s(&in,fname,"r");
#endif

	if(in==NULL)
	{
		PRINTV("[csv-parser] file %s",fname);
		return -1;
	}
	fseek(in,0, SEEK_END);  // seek to end of file
	size = ftell(in);       // get current file pointer
	fseek(in, 0, SEEK_SET); // seek back to beginning of file
	/* fgets reads one char less than size; so give one byte more to read
	 * any newline character
	 */
	tmp = (char *)malloc(size+1);
	while(fgets(tmp,size+1,in)!=NULL) /* read a record */
	{
		/* remove newline at the end of the line */
		if (tmp[strlen(tmp) - 1] == '\n')
			tmp[strlen(tmp) - 1] = '\0';
		recordcnt++;
		k = csv_parse(tmp,",",callbk,data1); /* whack record into fields */
		if(k != 0)
		{
			PRINTM("[csv-parser] error at record number %d\n",
					recordcnt);
			free(tmp);
			fclose(in);
			return -1;
		}
	}
	free(tmp);
	fclose(in);
	PRINTV("%s (OUT)\n",__func__);
	return 0;
}
/*callback function*/
static void add_key_values_on_eeprom(char *key,char *value,void *data)
{
	int r;
	int area;
	area = *(int *)data;
	r=mxcam_set_key(area,key,value);
	if(r)
		PRINTM("error:%s\n",mxcam_error_msg(r));
}


#define DEFAULT_MAXIM_EEPROM_SIZE 1024
#define DEFAULT_VENDOR_EEPROM_SIZE 256
static int mxcam_subcmd_erase(struct option_val **optval, struct arg_val **argval)
{
	int ret, size=0;
	const char* csvfile = NULL;
	CONFIG_AREA area;

	PRINTV("%s (IN)\n",__func__);

	/* Check from the options what config area to work on */
	if(has_option(opt_vendor, optval))
		area = VENDOR_INFO;
	else
		area = MAXIM_INFO;

	/* Get area size or use the default one */
	if(has_arg(arg_erase_size, argval))
		size = (intptr_t) get_value_from_arg(arg_erase_size, argval);
	else {
		if (area == MAXIM_INFO)
			size = DEFAULT_MAXIM_EEPROM_SIZE;
		else
			size = DEFAULT_VENDOR_EEPROM_SIZE;
	}
	/* Get the csv file path if given and check that it exits */
	if (has_arg(arg_erase_file, argval)) {
		struct stat sb;
		csvfile = get_value_from_arg(arg_erase_file, argval);
		if(stat(csvfile, &sb) == -1) {
			PRINTM("File not found: %s \n", csvfile);
			ret = 1;
			goto erase_out;
		}
	}

	/* If no --silent option, ask for confirmation before erasing */
	if(!has_option(opt_silent, optval)) {
		char c;
		printf("Do you want to erase the %s config area of the camera? [y/N] ",
		      (area == MAXIM_INFO) ? "maxim" : "vendor");
		c = getchar();
		if (c != 'y' && c != 'Y') {
			ret = 0;
			goto erase_out;
		}
	}

	/* Erase */
	ret = mxcam_erase_eeprom_config(area, size);

	/* Write new values from CSV file */
	if (has_arg(arg_erase_file, argval)) {
		int r;
		r = csv_file_parser(csvfile, add_key_values_on_eeprom, &area);
		if (r) {
			PRINTM("Found invalid format in %s\n", csvfile);
			ret = 1;
			goto erase_out;
		}

		/* Save added configs on eeprom */
		ret = mxcam_save_eeprom_config(area);
	}

	/* Error handling */
	if(ret) {
		PRINTM("%s\n",mxcam_error_msg(ret));
		return 1;
	}
erase_out:
	PRINTV("%s (OUT)\n",__func__);
	return ret;
}

/* INFO */

/* 64380 name is organized like this
 * bytes [0-5] build number
 * bytes [6-13] release name
 * bytes [14-31] branch name
 */

#define MAX64380_BUILDNUMBER_POS 0
#define MAX64380_BUILDNUMBER_LEN 6
#define MAX64380_RELEASE_POS (MAX64380_BUILDNUMBER_POS + MAX64380_BUILDNUMBER_LEN)
#define MAX64380_RELEASE_LEN 8
#define MAX64380_BRANCH_POS (MAX64380_RELEASE_POS + MAX64380_RELEASE_LEN)
#define MAX64380_BRANCH_LEN 18

void print_info(image_header_t *hdr, struct mxcam_devlist *devlist, char more)
{
	char release[512]="SDKXXRCXXCUSTXXXX";
	char str[512];
	char buildnumber[MAX64380_BUILDNUMBER_LEN+1];
	char releaseversion[MAX64380_RELEASE_LEN+1];
	char branchname[MAX64380_BRANCH_LEN+1];
	time_t timestamp;
	uint32_t size = 0;

	timestamp = (time_t)ntohl(hdr->ih_time);
	size = ntohl(hdr->ih_size);

    	/* handle 64180 differently from 64380 */
    	if (hdr->ih_os == 100)
    	{
        	memcpy(buildnumber, (char *)(hdr->ih_name+MAX64380_BUILDNUMBER_POS), MAX64380_BUILDNUMBER_LEN);
        	buildnumber[MAX64380_BUILDNUMBER_LEN] = '\0';
        	memcpy(releaseversion, (char *)(hdr->ih_name+MAX64380_RELEASE_POS), MAX64380_RELEASE_LEN);
        	releaseversion[MAX64380_RELEASE_LEN] = '\0';
        	memcpy(branchname, (char *)(hdr->ih_name+MAX64380_BRANCH_POS), MAX64380_BRANCH_LEN);
        	branchname[MAX64380_BRANCH_LEN] = '\0';

        	printf ("Release Version  : %s\n",releaseversion);
		if (more) {
			printf ("Build            : %s\n",buildnumber);
			printf ("Branch           : %s\n",branchname);
		}
    	}	
    	else
    	{
        	memcpy(str,release,sizeof(release));
        	memcpy(str + 3,hdr->ih_name + 28,2);
        	memcpy(str + 7,hdr->ih_name + 30,2);
        	memcpy(str + 13,hdr->ih_name + 24,4);
        	str[sizeof(release)] = '\0';

        	printf ("Release Version  : %s\n",str);
        	str[8] = '\0';
        	memcpy(str,hdr->ih_name,8);
        	printf ("Dist Version     : %s\n",str);
        	memcpy(str,hdr->ih_name + 8,8);
        	printf ("Host Version     : %s\n",str);
        	memcpy(str,hdr->ih_name + 16,8);
        	printf ("Codec Version    : %s\n",str);
    	}

#if !defined(_WIN32)
	printf ("Created          : %s", ctime(&timestamp));
#else
	ctime_s(str,sizeof(str),&timestamp);
	printf ("Created          : %s",str );
#endif

	printf ("Image Size       : %d Bytes = %.2f kB = %.2f MB\n",
		size, (double)size / 1.024e3, (double)size / 1.048576e6 );

	if (more) {
		printf ("Load Address     : 0x%08X\n", ntohl(hdr->ih_load));
		printf ("Entry Point      : 0x%08X\n", ntohl(hdr->ih_ep));
		printf ("Flash CRC        : %u\n", ntohl(hdr->ih_dcrc));

		if(hdr->ih_os == 17)
			printf ("OS               : %s\n","eCos");
		if(hdr->ih_arch == 2)
			printf ("Arch             : %s\n","ARM");
		if(hdr->ih_arch == 100)
			printf ("Arch             : %s\n","QMM");
	}
}

static int mxcam_subcmd_info(struct option_val **optval, struct arg_val **argval)
{
	int r = 0, i = 0;
	unsigned int max_crc, vend_crc;
	int max_ret, vend_ret;
	image_header_t hdr;
	struct mxcam_devlist *devlist;	
	int chip_id;
	char more = 0;

	devlist = gdevlist;

	chip_id = devlist->soc;
	if ((chip_id == MAX64180) && (devlist->type == DEVTYPE_BOOT)){
		printf("In MAX64180 in bootloader mode this command is not supported\n");
		return 1;
	}

	if(has_option(opt_more, optval))
		more = 1;

	PRINTV("%s (IN)\n",__func__);
	if(chip_id == MAX64180){
		r = mxcam_read_flash_image_header(&hdr, SNOR_FW_HEADER);
		printf_bold("Firmware image currently flashed: \n");
		print_info(&hdr, devlist, more);
	} else {
		if (devlist->type != DEVTYPE_BOOT){
			/* The first 4 bytes are the checksum */
			max_ret = mxcam_read_eeprom_config_mem(MAXIM_INFO,
				(char *)&max_crc, sizeof(unsigned int));
			/* The first 4 bytes are the checksum */
			vend_ret = mxcam_read_eeprom_config_mem(VENDOR_INFO,
				(char *)&vend_crc, sizeof(unsigned int));
		}
	
#if __BYTE_ORDER == __BIG_ENDIAN
		max_crc = ntohl(max_crc);
		vend_crc = ntohl(vend_crc);
#endif
		//print fw and bootloader informations
		for (i=((devlist->type==DEVTYPE_BOOT) ? 1 : 0) ; i<2 ; i++){
			if (i==0){
				if(has_option(opt_snor_hdr, optval)) {
					printf_bold("Firmware image currently flashed: \n");
					r = mxcam_read_flash_image_header(&hdr, SNOR_FW_HEADER);
				} else {
					printf_bold("Firmware image currently running: \n");
					r = mxcam_read_flash_image_header(&hdr, RUNNING_FW_HEADER);
				}
			} else {
				printf_bold("Bootloader image: \n");
				r = mxcam_read_flash_image_header(&hdr, BOOTLOADER_HEADER);
			}
			if(r) {
				PRINTM("%s\n",mxcam_error_msg(r));
				return 1;
			}

			print_info(&hdr, devlist, more);
			printf("\n");
		}
	}
	if (more && devlist->type != DEVTYPE_BOOT){
		printf_bold("Camera CCR:\n");
		if(!max_ret)
			printf ("Maxim Config CRC : %u\n", max_crc);
		else
			printf ("Maxim Config CRC : %s\n", "Invalid");
		if(!vend_ret)
			printf ("Vendor Config CRC: %u\n", vend_crc);
		else
			printf ("Vendor Config CRC: %s\n", "Invalid");
	}

	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* RESET */
static int mxcam_subcmd_reset(struct option_val **optval, struct arg_val **argval)
{
	int r = 0;
	PRINTV("%s (IN)\n",__func__);
	r = mxcam_reset();
	if (r) {
		PRINTM("%s\n",mxcam_error_msg(r));
		return 1;
	}
	PRINTV("%s (OUT)\n",__func__);
	return 0;
}

/* SETEPTYPE */
static int mxcam_subcmd_seteptype(struct option_val **optval, struct arg_val **argval)
{
	int r = 0;
	const char *eppoint = get_value_from_arg(arg_seteptype_blukisoc, argval);

	if (strcmp(eppoint, "isoc") == 0)
		r = mxcam_set_configuration(MAXIM_ISOC);
	else if (strcmp(eppoint, "bulk") == 0)
		r = mxcam_set_configuration(MAXIM_BULK);
	else {
		printf("mxcam seteptype argument must be either 'bulk' or 'isoc'\n");
		return 1;
	}

	if(r){
		PRINTM("%s\n",mxcam_error_msg(r));
		return 1;
	}

	return 0;
}

uint16_t i2c_addr;

void mxcam_i2cwrite_key_val(char *key,char *value,void *data){
    uint16_t sadr;
    i2c_payload_t payload;
    int v;
    int r = 0;

    sadr = (uint16_t)strtoul(key, NULL, 16);
    //payload = (uint32_t)strtoul(value, NULL, 16);
    v = (uint32_t)strtoul(value, NULL, 16);
    payload.data.len = 1; // assume one byte transaction only
    payload.data.buf[0] = (unsigned char)v;
    payload.dev_addr = i2c_addr;
    payload.sub_addr = sadr;

    r = mxcam_i2c_write(0, 0, &payload);

    if (r) {
	PRINTM("%s\n",mxcam_error_msg(r));
    }
    printf("i2c write addr 0x%x saddrs 0x%x value 0x%02x\n", i2c_addr, sadr, v);
}

/* i2cwrite */
int mxcam_subcmd_i2cwrite(struct option_val **optval, struct arg_val **argval)
{
	int r = 0;
	int i2c_inst = 0; /* i2c HW block 0 or 1 */
	int i2c_type;
	const char *addr, *saddr, *value, *csvfile;
	uint16_t adr;
	uint16_t sadr;
	uint32_t data;
	i2c_payload_t payload;

	addr = (const char*) get_value_from_arg(arg_i2c_addr, argval);

	adr = (uint16_t)strtoul(addr, NULL, 16);
	i2c_addr = adr;

        if (has_option(opt_i2cfile, optval)) {
                struct stat sb;
                csvfile = get_value_from_option(opt_i2cfile, optval);
                if(stat(csvfile, &sb) == -1) {
                        PRINTM("File not found: %s \n", csvfile);
                        return 1;
                }

		printf("i2c write in address 0x%x from %s file\n",adr,csvfile);
		r = csv_file_parser(csvfile, mxcam_i2cwrite_key_val, NULL);
                if (r) {
                        PRINTM("Found invalid format in %s\n", csvfile);
                        return 1;
                }
        } else {
	    /* for i2cwrite, the value to be written is mandatory */
	    if (has_arg(arg_i2c_value, argval) == 0) {
		printf("%s : Error-Invalid arguments\n",__func__);
		return 1;
	    }
	    /* the subaddr is optional */
	    if (has_arg(arg_i2c_subaddr, argval) == 1) {
		saddr = (const char*) get_value_from_arg(arg_i2c_subaddr, argval);
		sadr = (uint16_t)strtoul(saddr, NULL, 16);
	    }
	    else {
		saddr = NULL;
		sadr = 0;
	    }
	    i2c_inst = (intptr_t) get_value_from_arg(arg_i2c_inst, argval);
	    i2c_type = (intptr_t) get_value_from_arg(arg_i2c_type, argval);

	    value = (const char*) get_value_from_arg(arg_i2c_value, argval);
	    data = (uint32_t)strtoul(value, NULL, 16);

	    printf("i2c write: inst %d type %d dev 0x%x, subaddress 0x%x with value 0x%x\n",
		   i2c_inst, i2c_type, adr, sadr, data);

	    payload.dev_addr = adr;
	    payload.sub_addr = sadr;
	    payload.data.len = 1; // only 1 byte transaction is supported
	    payload.data.buf[0] = (unsigned char)data;
	    r = mxcam_i2c_write(i2c_inst, i2c_type, &payload);

	    if (r) {
		PRINTM("%s\n", mxcam_error_msg(r));
		return 1;
	    }
        }

	return 0;
}

void mxcam_i2cread_key_val(char *key,char *value,void *data) {
    uint16_t sadr;
    i2c_payload_t payload;
    int r = 0;

    sadr = (uint16_t)strtoul(key, NULL, 16);
    payload.dev_addr = i2c_addr;
    payload.sub_addr = sadr;
    payload.data.len = 1;

    r = mxcam_i2c_read(0, 0, &payload);

    if (r) {
	PRINTM("%s\n",mxcam_error_msg(r));
    }
    printf("i2c read addr 0x%x saddrs 0x%x value 0x%02x\n", i2c_addr, sadr, payload.data.buf[0]);
}

/* i2cread */
int mxcam_subcmd_i2cread(struct option_val **optval, struct arg_val **argval)
{
    int r = 0;
    int i2c_inst = 0; /* i2c HW block 0 or 1 */
    int i2c_type = 0;
    const char *addr, *saddr, *csvfile;
    uint16_t adr;
    uint16_t sadr;
    i2c_payload_t payload;

    addr = (const char*) get_value_from_arg(arg_i2c_addr, argval);

    adr = (uint16_t)strtoul(addr, NULL, 16);
    i2c_addr = adr;

    if (has_option(opt_i2cfile, optval)) {
	struct stat sb;
	csvfile = (const char *) get_value_from_option(opt_i2cfile, optval);
	if(stat(csvfile, &sb) == -1) {
	    PRINTM("File not found: %s \n", csvfile);
	    return 1;
	}

	printf("i2c read from address 0x%x from %s file\n",adr,csvfile);
	r = csv_file_parser(csvfile, mxcam_i2cread_key_val, NULL);
	if (r) {
	    PRINTM("Found invalid format in %s\n", csvfile);
	    return 1;
	}
    } else {

	if (has_arg(arg_i2c_subaddr, argval) == 0) {
	    printf("%s : Error-Invalid arguments\n",__func__);
	    return 0;
	}
	i2c_inst = (intptr_t)get_value_from_arg(arg_i2c_inst, argval);
	i2c_type = (intptr_t)get_value_from_arg(arg_i2c_type, argval);
	saddr = (const char *) get_value_from_arg(arg_i2c_subaddr, argval);
	sadr = (uint16_t)strtoul(saddr, NULL, 16);
	printf("i2c read from address 0x%x, subaddress 0x%x\n",adr,sadr);

	payload.dev_addr = adr;
	payload.sub_addr = sadr;
	payload.data.len = 1;
	memset(&payload.data.buf, 0, MXCAM_I2C_PAYLOAD_DATA_LEN);

	r = mxcam_i2c_read(i2c_inst, i2c_type, &payload);

	if (r) {
	    PRINTM("%s\n",mxcam_error_msg(r));
	    return 1;
	}

	printf("i2c register value = 0x%02x\n", payload.data.buf[0]);
    }

    return 0;
}

/* TCW */
static int mxcam_subcmd_tcw(struct option_val **optval, struct arg_val **argval)
{
        int r = 0;
	uint32_t data;

	/* write tcw */
	if (has_arg(arg_tcw_value, argval)) {
		data = (uintptr_t) get_value_from_arg(arg_tcw_value, argval);
		if(!has_option(opt_silent, optval)) {
			char c;
			printf("Updating the timing control word can render "
			 "the board unbootable if an invalid value is written.\n"
			 "Do you want to proceed? [y/N]");
			c = getchar();
			if (c != 'y' && c != 'Y') {
				return 2;
			}
		}
		r = mxcam_tcw_write(data);
		if(r){
			PRINTM("%s\n",mxcam_error_msg(r));
			return 3;
		}
	} else { /* read tcw */
		r = mxcam_tcw_read(&data);
		if(r){
			PRINTM("%s\n",mxcam_error_msg(r));
			return 4;
		}
		printf("%x\n",data);
	}
        return 0;
}

/* ISP */
static int mxcam_subcmd_isp(struct option_val **optval, struct arg_val **argval)
{
	int r = 0;
	uint32_t count = 1, i;
	uint32_t value ;
	uint16_t addr = (uintptr_t) get_value_from_arg(arg_isp_addr, argval);

	if (addr & 0x3) {
		PRINTM("ISP register addr must be mulitples of 4\n");
		return 1;
	}

	if (has_option(opt_isppause, optval))
	{
		uint32_t enable = (uintptr_t) get_value_from_option(opt_isppause, optval);

		r = mxcam_isp_enable(enable);

		if (r) {
			PRINTM("%s\n",mxcam_error_msg(r));
			return 1;
		}
	}

	/* isp write */
	if (has_arg(arg_isp_value, argval)) {
		value = (uintptr_t) get_value_from_arg(arg_isp_value, argval);
		r = mxcam_isp_write(addr, value);
		if (r) {
			PRINTM("%s\n",mxcam_error_msg(r));
			return 1;
		}
		PRINTM("ISP Write: Register 0x%04x Value 0x%08x\n", addr,value);
	} else { /* read isp */
		if (has_option(opt_isprcnt, optval))
			count = (uintptr_t) get_value_from_option(opt_isprcnt, optval);

		for (i=0; i<count; i++) {
			r = mxcam_isp_read(addr + (i * 4), &value);
			if (r) {
				PRINTM("%s\n",mxcam_error_msg(r));
				return 1;
			}
			if ((i % 4) == 0)
				PRINTM("[0x%04x] ", addr + (i * 4));

			PRINTM("0x%08x ", value);
			if ( ((i % 4) == 3) || (i == (count - 1)) )
				PRINTM("\n");
		}
	}

	return 0;
}

/* VERSION */
static int mxcam_subcmd_version(struct option_val **optval, struct arg_val **argval)
{
#if !defined(_WIN32)
	PRINTM("%s\n", MXCAM_VERSION);
#else
	PRINTM("%s\n", "NOT SUPPORTED");
#endif

	return 0;
}

/* USB testmode implementation */
static int mxcam_subcmd_usbtest(struct option_val **optval, struct arg_val **argval)
{
	int ret = 1;
	uint32_t mode;

	PRINTV("%s (IN)\n",__func__);

	/* Set specified USB test mode */
	mode = (uintptr_t) get_value_from_arg(arg_usb_testmode, argval);
	ret = mxcam_usbtest(mode);

	/* Error handling */
	if (ret) {
		PRINTM("%s\n",mxcam_error_msg(ret));
		ret = 1;
	}

	PRINTV("%s (OUT)\n",__func__);
	return ret;
}

/* QCC */
static int mxcam_subcmd_qcc(struct option_val **optval, struct arg_val **argval)
{
	int r = 0;
	uint32_t value ;
	uint16_t bid, addr, length;

	bid = (uintptr_t) get_value_from_arg(arg_qcc_bid, argval);
	addr = (uintptr_t) get_value_from_arg(arg_qcc_addr, argval);
	length = (uintptr_t) get_value_from_arg(arg_qcc_length, argval);


	/* QCC write */
	if (has_arg(arg_qcc_value, argval)) {
		value = (uintptr_t) get_value_from_arg(arg_qcc_value, argval);
		r = mxcam_qcc_write(bid, addr, length, value);
		if (r) {
			PRINTM("%s\n",mxcam_error_msg(r));
			return 1;
		}
		PRINTM("QCC write: bid=0x%x, addr=0x%x, length=0x%x, "
			"value=0x%x\n", bid, addr, length, value);
	/* QCC read */
	} else {
		r = mxcam_qcc_read(bid, addr, length, &value);
		if (r) {
			PRINTM("%s\n",mxcam_error_msg(r));
			return 1;
		}

		PRINTM("0x%x [%d]", value,value);
	}

	return 0;
}

/* WHOAMI */
static int mxcam_subcmd_whoami(struct option_val **optval, struct arg_val **argval)
{
	int r = 0, i = 0;
	struct mxcam_devlist *devlist;
	int chip_id = 0;
	int mode = 0;

	devlist = gdevlist;	

	PRINTV("%s (IN)\n",__func__);
	printf("Camera Core\t\t: ");
	chip_id = devlist->soc;
	if (chip_id == MAX64180)
		printf_bold("MAX64180\n");
	else if (chip_id == MAX64380)
		printf_bold("MAX64380\n");
	else if (chip_id == MAX64480)
		printf_bold("MAX64480\n");

	printf("Camera Mode\t\t: ");
	if (devlist->type == DEVTYPE_BOOT)
	{
		printf_bold("Waiting for USB boot\n");
		mode = 	BOOTLOADER;
	}else if (devlist->type == DEVTYPE_UVC)
	{
		printf_bold("Booted\n");
		mode = FW;
	}else {
		printf("unknown mode\n");
		return 1;
	}

	printf("Supported subcommands\t:\n");
	if((chip_id == MAX64180) || (mode == BOOTLOADER))
	{
		for (i=0 ; mxcam_subcmds[i].name != NULL; i++)
		{
			if (
			((mode == BOOTLOADER) 
			&& ((mxcam_subcmds[i].mode == BOOTLOADER) || (mxcam_subcmds[i].mode == FW_AND_BOOTLOADER)))		   		||
			((mode == FW)
			&& ((mxcam_subcmds[i].mode == FW) || (mxcam_subcmds[i].mode == FW_AND_BOOTLOADER)))
			)
			{
				//check core id, if this command is supported in this version then only print it
				if (((chip_id == MAX64180) && (mxcam_subcmds[i].core_id & CORE_ID_64180)) ||
			   	((chip_id == MAX64380) && (mxcam_subcmds[i].core_id & CORE_ID_64380)) ||
				((chip_id == MAX64480) && (mxcam_subcmds[i].core_id & CORE_ID_64480)))
				{
					printf_bold("  %-22s: ",mxcam_subcmds[i].name);
					printf("%s\n",mxcam_subcmds[i].help);
				}
			}
		}
	} else if (((chip_id == MAX64380) || (chip_id == MAX64480)) || (mode == FW)){
		//this is for versions 64380 and leter fw's
		char cmd_bitmap[256];
		int latest_fw = 0;
		int data[2] = {0, 0};

		r = mxcam_whoru((char *)&data);
		if((data[0] == MAX64380) || (data[0] == MAX64480)){
			//latest fw camera
			latest_fw = 1;
		}else
			latest_fw = 0;

		memset(cmd_bitmap, 0, 256);

		if(latest_fw){
			r = mxcam_get_cmd_bitmap(cmd_bitmap);	
			if (r<0)
			{
				printf( "mxcam_get_supported_cmd_bitmap failed\n");
				return 1;	
			}
		
			for (i=0 ; mxcam_subcmds[i].name != NULL; i++){
				if(MXCAM_OK == mxcam_check_command_support(mxcam_subcmds[i].name, cmd_bitmap))
				{
					printf_bold("  %-22s: ",mxcam_subcmds[i].name);	
					printf("%s\n",mxcam_subcmds[i].help);		
				}
			}
		} else {
			 //for old raptor fw
			for (i=0 ; mxcam_subcmds[i].name != NULL; i++){
				if((mode == FW)
				&& ((mxcam_subcmds[i].mode == FW) || (mxcam_subcmds[i].mode == FW_AND_BOOTLOADER)))
				{
					printf_bold("  %-22s: ",mxcam_subcmds[i].name);	
					printf("%s\n",mxcam_subcmds[i].help);
				}
			}
		}
	}

	return 0;
}

/* MEMTEST */
static int mxcam_subcmd_memtest(struct option_val **optval, struct arg_val **argval){
	struct mxcam_devlist *devlist;
	int chip_id = 0;
	int r;
	const char *image=NULL, *opt_image=NULL;
	uint32_t result = 0;
	char *bootmode = NULL;
	int data_result = 0, address_result = 0;
	uint32_t ddr_size = 0;

typedef enum {
	MEMTEST_FULLMEM,
        MEMTEST_LOADMEM,
	MEMTEST_ALL,
} test_type_t;

	test_type_t type = MEMTEST_FULLMEM;

	devlist = gdevlist;	
	image = get_value_from_arg(arg_boot_image, argval);
	ddr_size = (intptr_t) get_value_from_option(opt_ddr_size, optval);

	if(has_option(opt_fullmem, optval) && has_option(opt_loadmem, optval))
		type = 	MEMTEST_ALL;
	else if(has_option(opt_fullmem, optval))
		type = 	MEMTEST_FULLMEM;
	else if(has_option(opt_loadmem, optval))
		type = MEMTEST_LOADMEM;
				
	PRINTV("%s (IN)\n",__func__);
	printf("Camera Core\t\t: ");
	chip_id = devlist->soc;
	if (chip_id == MAX64180)
		printf("MAX64180\n");
	else if (chip_id == MAX64380)
		printf("MAX64380\n");
	else if (chip_id == MAX64480)
		printf("MAX64480\n");

	printf("DDR size\t\t: %dMB\n",ddr_size);

	if(type == MEMTEST_LOADMEM){
		printf("loadmem test mode is not supported yet\n");
		printf("please use fullmem test only\n");
		return 0;	
	}

	/* if the device is in uvc mode
	   store the bootmode and made boot mode to usb and reset */
	if (devlist->type == DEVTYPE_UVC){
		
		r = mxcam_get_value(MAXIM_INFO, "BOOTMODE", &bootmode);
		if (r) {
			PRINTM("Unable to get BOOTMODE\n");
			return MXCAM_ERR_INVALID_DEVICE;
		}
		
		if(strcmp(bootmode, "usb") != 0){
			r = mxcam_set_key(MAXIM_INFO, "BOOTMODE", "usb");	
			if (r) {
				PRINTM("Unable to set BOOTMODE\n");
				return MXCAM_ERR_INVALID_DEVICE;
			}
			r = mxcam_save_eeprom_config(MAXIM_INFO);
		}else
			bootmode = NULL;	
		//reset
		mxcam_reset();
		mxcam_close();
		while(1){
			sleep(1);

			r = mxcam_scan(&devlist);

			if(devlist == NULL){
				continue;
			}
			if (devlist->soc == MAX64380 && 
				devlist->type == DEVTYPE_BOOT){
				gdevlist = devlist;
				break;
			}
		}
		
		r = open_device(optval, devlist);
		if (r) {
			PRINTM("Unable to open device\n");
			return MXCAM_ERR_INVALID_DEVICE;
		}
	}
	//start doing ddr test on bootloader
	if ((devlist->type == DEVTYPE_BOOT) && 
		(type == MEMTEST_FULLMEM || type == MEMTEST_ALL))
	{
		printf("Testing DDR from Bootloader...\n");
		mxcam_init_ddr(image);
		r = mxcam_memtest(ddr_size);
		if(r<0){
			printf("ERR: mxcam_memtest Failed %d\n",r);
			result = data_result = address_result = 1;
			goto memtest_result;			
		}
		r = mxcam_get_memtest_result(&result);
		if(r<0){
			printf("ERR: mxcam_get_memtest_result Failed %d\n",r);
			result = data_result = address_result = 1;
			goto memtest_result;			
		}
		if((result >> 16) > 0)
			data_result = 1;
		else 
			data_result = 0;
		if((result & 0x0000ffff) > 0)
			address_result = 1;
		else 
			address_result = 0;

		//reset the camera
		mxcam_reset();
	}

	//now only bootloader test is supported
	goto memtest_result; //TBD 

	if(type == MEMTEST_ALL || type == MEMTEST_LOADMEM)
		mxcam_close();
	//detect the usb boot device and boot it with fw
	while(1){
		sleep(1);

		r = mxcam_scan(&devlist);

		if(devlist == NULL){
			continue;
		}
		if (devlist->soc == MAX64380 && 
			devlist->type == DEVTYPE_BOOT){
			gdevlist = devlist;
			break;
		}
	}
	r = open_device(optval, devlist);
	if (r) {
		PRINTM("Unable to open device\n");
		return MXCAM_ERR_INVALID_DEVICE;
	}
	r = mxcam_boot_firmware(image, opt_image,
			mxcam_fw_print_status);
	if(r){
		PRINTM("%s\n",mxcam_error_msg(r));
		return 1;
	}
	//close and reopen a uvc device
	mxcam_close();
	while(1){
		sleep(1);

		r = mxcam_scan(&devlist);
		if(devlist == NULL){
			continue;
		}
		if (devlist->soc == MAX64380 &&
			devlist->type == DEVTYPE_UVC){
			gdevlist = devlist;
			break;
		}
	}
	r = open_device(optval, devlist);
	if (r) {
		PRINTM("Unable to open device\n");
		return MXCAM_ERR_INVALID_DEVICE;
	}

	if (devlist->type == DEVTYPE_UVC){
		printf("Testing DDR from Firmware...\n");
		r = mxcam_memtest(ddr_size);
		if(r<0){
			printf("ERR: mxcam_memtest Failed %d\n",r);
			result = 1;
			goto memtest_result;			
		}
		//wait for a while to get the system loaded
		sleep(1);
		r = mxcam_get_memtest_result(&result);
		if(r<0){
			printf("ERR: mxcam_get_memtest_result Failed %d\n",r);
			result = 1;
		}
	}else {
		printf("unknown mode\n");
		return 1;
	}

memtest_result:
	//publish the results
	printf("\n");
	printf_bold("DDR Test Case \t\t Result\n");
	printf_bold("-------------------------------\n");
	if(data_result == 1)
		printf_bold("Data line test \t\t Failed\n");	
	else 
		printf_bold("Data line test \t\t Passed\n");	
	if(address_result == 1)
		printf_bold("Address line test \t Failed\n");
	else 
		printf_bold("Address line test \t Passed\n");	

#if 0 //TBD
	if(type == MEMTEST_ALL || type == MEMTEST_LOADMEM){
		if(result > 0)
			printf_bold("Memory integrity test \t Failed\n");	
		else 
			printf_bold("Memory integrity test \t Passed\n");
	}

	if(bootmode && (result == 0)){
		r = mxcam_set_key(MAXIM_INFO, "BOOTMODE", bootmode);	
		if (r) {
			PRINTM("Unable to set BOOTMODE\n");
			return MXCAM_ERR_INVALID_DEVICE;
		}	
		r = mxcam_save_eeprom_config(MAXIM_INFO);
	}
	//dont try to reset a camera which might be in a bad shape
	if(result == 0)
		mxcam_reset();
	
	if(result)
		return 1;
#endif 
	if(data_result || address_result)
		return 1;

	return 0;
}
