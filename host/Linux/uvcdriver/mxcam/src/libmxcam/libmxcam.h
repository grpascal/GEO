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

#ifndef __LIBMXCAM_H__
#define __LIBMXCAM_H__

#if !defined(_WIN32)
	#include <stdint.h>
#else
	#include "win-libusb.h"
	#define __func__  __FUNCTION__
	#define sleep(x)	Sleep(x*1000)
#endif


#define MXCAM_I2C_PAYLOAD_DATA_LEN 1
#define MXCAM_I2C_MAX_RETRIES 5

/** \ingroup misc
 * Error and status codes of libmxcam.
 * Most of libmxcam functions return 0 on success.a negative value indicates
 * its a libusb error.other wise one of the these codes on failure
 */
typedef enum {
	/** Success (no error) */
	MXCAM_OK = 0,
	/** Started EEPROM FW Upgrade */
	MXCAM_STAT_EEPROM_FW_UPGRADE_START,//1
 	/** Completed  EEPROM FW Upgrade */
	MXCAM_STAT_EEPROM_FW_UPGRADE_COMPLETE,//2
 	/** Started SNOR FW Upgrade */
	MXCAM_STAT_SNOR_FW_UPGRADE_START,//3
 	/** Completed SNOR FW Upgrade */
	MXCAM_STAT_SNOR_FW_UPGRADE_COMPLETE,//4
 	/** Completed FW Upgrade */
	MXCAM_STAT_FW_UPGRADE_COMPLETE,//5
 	/** EEPROM Erase in progress */
	MXCAM_STAT_EEPROM_ERASE_IN_PROG,//6
 	/** EEPROM config save in progress */
	MXCAM_STAT_EEPROM_SAVE_IN_PROG,//7
 	/** ERR numbers starts here */
	MXCAM_ERR_FAIL = 128,
 	/** FW Image is corrupted */
	MXCAM_ERR_FW_IMAGE_CORRUPTED,//129
 	/** SNOR FW upgrade failed */
	MXCAM_ERR_FW_SNOR_FAILED,//130
 	/** Unsupported Flash memory */
	MXCAM_ERR_FW_UNSUPPORTED_FLASH_MEMORY,//131
 	/** Erase size exceeds MAX_VEND_SIZE */
	MXCAM_ERR_ERASE_SIZE,//132
 	/** Unknown area to erase */
	MXCAM_ERR_ERASE_UNKNOWN_AREA,//133
 	/** Unknown area to save */
	MXCAM_ERR_SAVE_UNKNOWN_AREA,//134
 	/** Not enough memory to save new key on memory */
	MXCAM_ERR_SETKEY_OVER_FLOW_NO_MEM,//135
 	/** Unknown area to set key */
	MXCAM_ERR_SETKEY_UNKNOWN_AREA,//136
 	/** Unknown area to remove key */
	MXCAM_ERR_REMOVE_KEY_UNKNOWN_AREA,//137
 	/** Unknown area to get key */
	MXCAM_ERR_GETVALUE_UNKNOWN_AREA,//138
 	/** Value not found for given key */
	MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND,//139
	/** Failed to read TCW from flash */
	MXCAM_ERR_TCW_FLASH_READ,//140
	/** Failed to write TCW on flash */
	MXCAM_ERR_TCW_FLASH_WRITE,//141
	/** Failed to allocate memory on camera*/
	MXCAM_ERR_MEMORY_ALLOC,//142
	/** Vendor area is not initialized */
	MXCAM_ERR_VEND_AREA_NOT_INIT,//143
	//Don't change the above values
 	/** Unknown area to get config size */
	MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA = 150,
 	/** Invalid parameter(s) */
	MXCAM_ERR_INVALID_PARAM,//151
 	/** Not a valid device */
	MXCAM_ERR_INVALID_DEVICE,//152
 	/** Failed to send image */
	MXCAM_ERR_IMAGE_SEND_FAILED,//153
 	/** File not found */
	MXCAM_ERR_FILE_NOT_FOUND,//154
	/** Not enough memory */
	MXCAM_ERR_NOT_ENOUGH_MEMORY,//155
	/** Not a valid image */
	MXCAM_ERR_NOT_VALID_IMAGE,//156
 	/** vid and pid already registered */
	MXCAM_ERR_VID_PID_ALREADY_REGISTERED,//157
	 /** device not found */
 	MXCAM_ERR_DEVICE_NOT_FOUND,//158
	/** vendor area not initialized*/
	MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY,//159
	/** feature not supported*/
	MXCAM_ERR_FEATURE_NOT_SUPPORTED,//160
	/** i2c read error */
	MXCAM_ERR_I2C_READ = 180,
	/** i2c write error */
	MXCAM_ERR_I2C_WRITE,
} MXCAM_STAT_ERR;

typedef enum {
	FW_UPGRADE_START = 0x02,
	FW_UPGRADE_COMPLETE,//0x3
	START_TRANSFER,//0x4
	TRANSFER_COMPLETE,//0x5
	TX_IMAGE,//0x6
	GET_NVM_PGM_STATUS,//0x7
	GET_SNOR_IMG_HEADER,//0x8
	GET_EEPROM_CONFIG,//0x9
	GET_EEPROM_CONFIG_SIZE,//0xA
	REQ_GET_EEPROM_VALUE,//0xB
	GET_EEPROM_VALUE,//0xC
	SET_EEPROM_KEY_VALUE,//0xD
	REMOVE_EEPROM_KEY,//0xE
	SET_OR_REMOVE_KEY_STATUS,//0xF
	ERASE_EEPROM_CONFIG,//0x10
	SAVE_EEPROM_CONFIG,//0x11
	RESET_BOARD,//0x12
	I2C_WRITE,//0x13
	I2C_READ,//0x14
	I2C_WRITE_STATUS, //0x15
	I2C_READ_STATUS, //0x16
	TCW_WRITE,//0x17
	TCW_WRITE_STATUS,//0x18
	TCW_READ,//0x19
	TCW_READ_STATUS,//0x1A
	ISP_WRITE,//0x1B
	ISP_READ,//0x1C
	ISP_WRITE_STATUS,//0x1D
	ISP_READ_STATUS,//0x1E
	ISP_ENABLE,//0x1F
	ISP_ENABLE_STATUS,//0x20
	REQ_GET_CCRKEY_VALUE,//0x21
	GET_CCRKEY_VALUE,//0x22
	GET_CCR_SIZE, //0x23
	GET_CCR_LIST, //0x24
	GET_IMG_HEADER, //0x25
	GET_BOOTLOADER_HEADER, //0x26
	GET_CMD_BITMAP, //0x27
	CMD_WHO_R_U, //0x28	
	AV_ALARM, //0x29
	A_INTENSITY,//0x2A
	AUDIO_VAD, //0x2B
	REBOOT_BOARD,//0x2C

	//GET_SPKR_VOL, //0x2C
	SET_SPKR_VOL, //0x2D
	
	MEMTEST, //0x2E	
	MEMTEST_RESULT, //0x2F	

	QCC_READ = 0x60,
	QCC_WRITE, //0x61,
	QCC_READ_STATUS, //0x62,
	QCC_WRITE_STATUS, //0x63,

	VENDOR_REQ_LAST = 0x80
} VEND_CMD_LIST;

/** \ingroup upgrade
 * program media used for firmware upgrade
 */
typedef enum {
	/** firmware upgrade media is EEPROM */
	EEPROM=0x1,
 	/** firmware upgrade media is parallel NOR */
	PNOR,
 	/** firmware upgrade media is serial NOR */
	SNOR,
 	/** max upgrade media for validation*/
	LAST_MEDIA,
} PGM_MEDIA;

/** \ingroup configuration
* config area
*/
typedef enum {
	/** maxim config area */
	MAXIM_INFO = 1,
 	/** vendor config area */
	VENDOR_INFO,
 	/** max config area for validation*/
	LAST_INFO
} CONFIG_AREA;

/** \ingroup configuration
* endpoint configuration
*/
typedef enum {
	/** isoc */
	MAXIM_ISOC = 1,
	/** bulk */
	MAXIM_BULK,
}EP_CONFIG;

/** \ingroup configuration
* represents registered callback function state of
* \ref mxcam_get_all_key_values function
*/
typedef enum {
	/** found an valid key and value pair*/
	GET_ALL_KEY_VALID = 1,
 	/** config area parsing is completed */
	GET_ALL_KEY_COMPLETED,
 	/** found a empty config area*/
	GET_ALL_KEY_NO_KEY,
} GET_ALL_KEY_STATE;

/** \ingroup upgrade
* represents BOOTMODE used in \ref mxcam_upgrade_firmware function
*/
typedef enum  {
	/** don't set the bootmode of bootloader */
	MODE_NONE = 0,
 	/** set the bootmode as USB */
	MODE_USB,
 	/** set the bootmode as serial nor */
	MODE_SNOR,
} BOOTMODE;

/** \ingroup boot
*represents registered callback function state of
*\ref mxcam_boot_firmware function
*/
typedef enum  {
	/** started firmware download over usb  */
	FW_STARTED = 0,
 	/** completed firmware download over usb */
	FW_COMPLETED,	
} FW_STATE;

/** \ingroup library
*
*\ref mxcam_register_vidpid
*/
typedef enum  {
	/** MAX64180  */
	MAX64180 = 0,
 	/** MAX64380 */
	MAX64380,	
	MAX64480,
	NUM_SOC,
	SOC_UNKNOWN,
} SOC_TYPE;

/** \ingroup library
*
*\ref mxcam_register_vidpid
*/
typedef enum  {
	/** boot device used by the bootloader
 	* during the first stage of the booting process   */
	DEVTYPE_BOOT = 0,
 	/** enumerated as uvc device */
	DEVTYPE_UVC,
	NUM_DEVTYPE,
	DEVTYPE_UNKNOWN,
} DEVICE_TYPE;

/** \ingroup upgrade
*structure to encapsulate firmware upgrade information that used in
\ref mxcam_upgrade_firmware function
*/
typedef struct fw_info {
	/** application image */
	const char *image;
	/** bootloader image */
	const char *bootldr;
	/** program media of application image */
	PGM_MEDIA img_media;
	/** program media of bootloader image */
	PGM_MEDIA bootldr_media;
	/** set this bootmode after bootloader image program */
	BOOTMODE mode;
} fw_info;

/** \ingroup library
*
* \ref mxcam_read_flash_image_header 
*/
typedef enum  {
	RUNNING_FW_HEADER = 0,
	SNOR_FW_HEADER,
	BOOTLOADER_HEADER,
	UNDEFINED,
} IMG_HDR_TYPE;

/** \ingroup library
*structure to hold information of all registered devices 
*/
struct mxcam_devlist {
	/** vendor id */
	int vid;
	/** product id */
	int pid;
	/** device desc as a string: DEPRECATED */
	const char *desc;
	/** connected bus*/
	int bus;
	/** connected address*/
	int addr;
	/** camera type **/
	DEVICE_TYPE type;
	/** soc **/
	SOC_TYPE soc;
	/** pointer to usb struct **/
	void *dev;
	/** link to next node */
	struct mxcam_devlist *next;
};

/** Image Header Magic Number */
#define IH_MAGIC        0x27051956
/** Image Name Length */
#define IH_NMLEN                32          

/** \ingroup configuration
* 64bytes flash image header information
*/
typedef struct image_header {
	/** Image Header Magic Number 0x27051956 */
	uint32_t        ih_magic;
	/** Image Header CRC Checksum */
	uint32_t        ih_hcrc;
	/** Image Creation Timestamp in  in ctime format*/
	uint32_t        ih_time;
	/** Image Data Size           */            
	uint32_t        ih_size;
	/** Data  Load  Address       */         
	uint32_t        ih_load;
	/** Entry Point Address       */  
	uint32_t        ih_ep;
	/** Image Data CRC Check sum   */
	uint32_t        ih_dcrc;
	/** Operating System          */
	uint8_t         ih_os;
	/** CPU architecture          */              
	uint8_t         ih_arch;
	/** Image Type                */
	uint8_t         ih_type;
	/** Compression Type          */
	uint8_t         ih_comp;
	/** Image Name                */     
	uint8_t         ih_name[IH_NMLEN]; 
} image_header_t;

typedef struct {
	char len; // dual purpose for status and buf len
	unsigned char buf[MXCAM_I2C_PAYLOAD_DATA_LEN];
} i2c_data_t;
typedef struct {
	unsigned short dev_addr;
	unsigned short sub_addr;
	i2c_data_t     data;
} i2c_payload_t;

/* API */
int mxcam_scan(struct mxcam_devlist **devlist);
int mxcam_scan_oldcam(struct mxcam_devlist **devlist);
int mxcam_open_by_devnum(int dev_num, struct mxcam_devlist *devlist);
int mxcam_open_by_busaddr(int bus, int addr, struct mxcam_devlist *devlist);
int mxcam_poll_one(struct mxcam_devlist *devlist);
int mxcam_poll_new(void);
int mxcam_boot_firmware(const char *image, const char *opt_image,
	void (*callbk)(FW_STATE st, const char *filename));
int mxcam_upgrade_firmware(fw_info *fw,
	void (*callbk)(FW_STATE st, const char *filename),int is_rom_img);
int mxcam_read_nvm_pgm_status(unsigned char *status);
int mxcam_erase_eeprom_config(CONFIG_AREA area,unsigned short size);
int mxcam_set_key(CONFIG_AREA area, const char *keyname, const char *value);
int mxcam_remove_key(CONFIG_AREA area, const char *keyname);
int mxcam_save_eeprom_config(CONFIG_AREA area);
int mxcam_get_config_size(CONFIG_AREA area,unsigned short *size_out);
int mxcam_read_eeprom_config_mem(CONFIG_AREA area,char *buf,unsigned short len);
int mxcam_read_flash_image_header(image_header_t *header, IMG_HDR_TYPE hdr_type);
int mxcam_get_value(CONFIG_AREA area, const char *keyname, char **value_out);
int mxcam_get_ccrvalue(const char *keyname, char **value_out);
int mxcam_get_all_ccr(CONFIG_AREA area, void (*callbk)(GET_ALL_KEY_STATE st, int keycnt, void *data));
int mxcam_free_get_value_mem(char *value_mem);
int mxcam_get_all_key_values(CONFIG_AREA area,
	void (*callbk)(GET_ALL_KEY_STATE st,int keycnt,void *data1,void *data2));
int mxcam_reset(void);
void mxcam_close (void);
const char* mxcam_error_msg(const int err);
int mxcam_set_configuration(EP_CONFIG config);
int mxcam_i2c_write(uint16_t inst, uint16_t type, i2c_payload_t *payload);
int mxcam_i2c_read(uint16_t inst, uint16_t type, i2c_payload_t *payload);
int mxcam_tcw_write(uint32_t value);
int mxcam_tcw_read(uint32_t *value);
int mxcam_isp_write(uint16_t addr, uint32_t value);
int mxcam_isp_read(uint16_t addr, uint32_t *value);
int mxcam_isp_enable(uint32_t enable);
int mxcam_usbtest(uint32_t testmode);
int mxcam_qcc_write(uint16_t bid, uint16_t addr, uint16_t length, uint32_t value);
int mxcam_qcc_read(uint16_t bid, uint16_t addr, uint16_t length, uint32_t *value);
int mxcam_whoami(void);
int mxcam_get_cmd_bitmap(char *buffer);
int mxcam_whoru(char *buffer);

int mxcam_memtest(uint32_t ddr_size);
int mxcam_get_memtest_result(uint32_t *value);
int mxcam_init_ddr(const char *image);

/* Deprecated API: kept for backward compatibility  */
int mxcam_register_vidpid(int vid, int pid, char *desc,SOC_TYPE soc,DEVICE_TYPE dev);

#endif //__LIBMXCAM_H__
