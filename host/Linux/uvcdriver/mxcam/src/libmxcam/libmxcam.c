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

/**
 * \mainpage libmxcam API Reference
 * \section intro Introduction
 * libmxcam library  allows you to manage the maxim camera over USB with vendor
 * specific commands.<br>This documentation is aimed at application developers
 * wishing to manage maxim camera over usb.
 * \section features Library features
 * - boot the camera over usb
 * - upgrade the camera firmware
 * - get the camera information
 * - reboot the camera
 * - camera configuration records management
 * \section errorhandling Error handling
 * libmxcam functions typically return  \ref MXCAM_OK on success or a non zero value
 * for error code.<br> a negative value indicates it is a libusb error.<br>
 * The error codes defined in \ref MXCAM_STAT_ERR as enum constants.<br>
 * \ref MXCAM_STAT_ERR codes contains error as well as status codes,
 * which are listed on the \ref misc "miscellaneous" documentation page.
 *<br>\ref mxcam_error_msg "mxcam_error_msg" can be used to convert error
 * and status code to human readable string
 * \section Dependent Dependent library
 *  - libusb 1.0
 */
/** @defgroup library Library initialization/deinitialization */
/** @defgroup boot Boot firmware */
/** @defgroup upgrade Upgrade firmware */
/** @defgroup camerainfo Camera information */
/** @defgroup configuration Camera configuration record management  */
/** @defgroup misc Miscellaneous */
/** \file
 * libmxcam implementation
 */
#ifndef API_EXPORTED

#if !defined(_WIN32)
	#include <sys/types.h>
	#include <libusb-1.0/libusb.h>
	#include <unistd.h>
	#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "libmxcam.h"

#define FWPACKETSIZE 4088	/*3072  2048 4096 */
#define EP0TIMEOUT   (0) 	/* unlimited timeout */

#define LIBMXDEBUG
#undef LIBMXDEBUG

#if !defined(_WIN32)
	#ifdef LIBMXDEBUG
	#define MXPRINT(args...) printf("[libmxcam] " args)
	#else
	#define MXPRINT(args...)
	#endif
#else
	#ifdef LIBMXDEBUG
	#define MXPRINT(...) { printf("[libmxcam] " __VA_ARGS__); }
	#else
	#define MXPRINT(...)
	#endif
#endif

#define DONT_CHK_DEV_ID
#undef DONT_CHK_DEV_ID

static struct libusb_device_handle *devhandle=NULL;
static struct libusb_context *dcontext=NULL;
static struct mxcam_devlist *devlist_cache=NULL;
static struct mxcam_devlist *cur_mxdev=NULL;
static struct libusb_device **devs=NULL;

/*
 * is uvc device or not ?
 */
static int is_uvcdevice(libusb_device_handle *dev)
{
	return cur_mxdev->type == DEVTYPE_UVC;
}

/*
 * is mboot device or not ?
 */
static int is_bootdevice(libusb_device_handle *dev)
{
	return cur_mxdev->type == DEVTYPE_BOOT;
}

static int is_max64180(libusb_device_handle *dev)
{
	return cur_mxdev->soc == MAX64180;
}

static int is_max64380(libusb_device_handle *dev)
{
	return cur_mxdev->soc == MAX64380;
}

static int is_max64480(libusb_device_handle *dev)
{
	return cur_mxdev->soc == MAX64480;
}

static void *grab_file(const char *filename, int blksize, unsigned int *size,
		unsigned int *totblks)
{
	unsigned int max = blksize;
	int ret, err_save;
	FILE *fd;

	char *buffer = malloc(max);
	if (!buffer)
		return NULL;

#if !defined(_WIN32)
	fd = fopen(filename, "rb");
#else
	ret = fopen_s(&fd,filename, "rb");
#endif

	if (fd == NULL)
		return NULL;
	*size = 0;
	*totblks = 1;

	while ( (ret = (int)fread((char*)(buffer + (*size)), sizeof(char),max-(*size),fd)) > 0 ){
		*size += ret;
		if (*size == max) {
			void *p;
			p = realloc(buffer, max *= 2);
			if (!p)
				goto out_error;
			buffer = p;
			memset((buffer + *size), 0, max - *size);
		}
	}
	if (ret < 0)
		goto out_error;

	fclose(fd);

	*totblks = (*size + blksize - 1) / blksize;
	return buffer;

out_error:
	err_save = errno;
	free(buffer);
	fclose(fd);
	errno = err_save;
	*totblks = 0;
	return NULL;
}
/*
 *   all data in network byte order (aka natural aka bigendian)
 */
static unsigned int get_loadaddr(image_header_t *img)
{
	return ntohl(img->ih_load);
}

static int isvalidimage(image_header_t* img)
{
	return (ntohl(img->ih_magic) != IH_MAGIC) ? 1 : 0 ;
}

static int usb_send_file(struct libusb_device_handle *dhandle,
		const char *filename, unsigned char brequest,
		int fwpactsize, int fwupgrd,int isbootld, unsigned int *fsz)
{
	int r;
	unsigned int fsize = 0, wblkcount = 0;
	int retryc = 0;
	unsigned char *trabuffer;
	unsigned int count;
	unsigned int imageaddr=0;
	struct stat stfile;
	image_header_t img_hd;

	if(stat(filename,&stfile))
		return MXCAM_ERR_FILE_NOT_FOUND;
	trabuffer =
		(unsigned char *)grab_file(filename, fwpactsize, &fsize,
				&wblkcount);
	if (!trabuffer)
		return MXCAM_ERR_NOT_ENOUGH_MEMORY;

	/* Check that the image is bigger than the header size */
	if(fsize < sizeof(image_header_t)) {
		free(trabuffer);
		return MXCAM_ERR_NOT_VALID_IMAGE;
	}
	memcpy(&img_hd, trabuffer, sizeof(image_header_t));


	/* Should not check bootloader header */
	if(!isbootld) {
		if (isvalidimage(&img_hd)) {
			free(trabuffer);
			return MXCAM_ERR_NOT_VALID_IMAGE;
		}
	}
	imageaddr=get_loadaddr(&img_hd);

	MXPRINT("File %s Size %d \n", filename, fsize);
	MXPRINT("Blk count %d\n", wblkcount);
	MXPRINT("Image load addr %0x\n",imageaddr);

	if( fwupgrd == 0 ){
		/*Send image download address in <wValue wIndex> */
		r = libusb_control_transfer(dhandle,
			/* bmRequestType */
			LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE,
			/* bRequest      */ brequest-1 ,
			/* wValue        */ (uint16_t) (imageaddr >> 16),
			/* wIndex        */ (uint16_t) (imageaddr & 0x0000ffff),
			/* Data          */ NULL,
			/* wLength       */ 0,
					    0);//No time out
		if (r < 0) {
			free(trabuffer);
			return MXCAM_ERR_IMAGE_SEND_FAILED;
		}
	}
	for (count = 0; count < wblkcount; count++) {
retry:
		r = libusb_control_transfer(dhandle,
				/* bmRequestType*/
				LIBUSB_ENDPOINT_OUT |LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE,
				/* bRequest     */brequest,
				/* wValue       */(uint16_t)(count >> 16),
				/* wIndex       */(uint16_t)(count & 0x0000ffff),
				/* Data         */
				trabuffer + (count * fwpactsize),
				/* wLength       */ fwpactsize,
				0); // No timeout
		if (r != fwpactsize) {
			MXPRINT("Retry:libusb_control_transfer cnt %d\n",count);
			if (retryc <= 3) {
				retryc++;
				goto retry;
			}
			free(trabuffer);
			return MXCAM_ERR_IMAGE_SEND_FAILED;
		}
	}
	free(trabuffer);
	*fsz = fsize;
	return MXCAM_OK;
}

static int tx_libusb_ctrl_cmd(VEND_CMD_LIST req, uint16_t wValue)
{
	int r;
	char data[4] = {0};
	uint16_t wLength=4;

	r = libusb_control_transfer(devhandle,
                        /* bmRequestType */
			LIBUSB_ENDPOINT_OUT| LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE,
			req, 			/* bRequest      */
			wValue,			/* wValue */
			0, 			/* wIndex */
			(unsigned char *)data,  /* Data          */
			wLength,
			EP0TIMEOUT);
	if (r < 0) {
		return r;
	}
	return MXCAM_OK;
}
#endif /*API_EXPORTED*/

/* API's Implementation */

/**
* \ingroup misc
* \brief mxcam_error_msg:
* 	To turn an error or status code into a human readable string
* \param err  : error or status value
*
* \retval  covertedstring
*
* \remark
*	 This function can be used to turn an error code into a human
* readable string.
*/
const char* mxcam_error_msg(const int err)
{
	switch (err) {
		case MXCAM_OK:
			return "No error - operation complete";
		// Staus
		case MXCAM_STAT_EEPROM_FW_UPGRADE_START:
			return "Started EEPROM FW Uprgade";
		case MXCAM_STAT_EEPROM_FW_UPGRADE_COMPLETE:
			return "Completed  EEPROM FW Uprgade";
		case MXCAM_STAT_SNOR_FW_UPGRADE_START:
			return "Started SNOR FW Uprgade";
		case MXCAM_STAT_SNOR_FW_UPGRADE_COMPLETE:
			return "Completed SNOR FW Uprgade";
		case MXCAM_STAT_FW_UPGRADE_COMPLETE:
			return "Completed FW Upgrade";
		case MXCAM_STAT_EEPROM_ERASE_IN_PROG:
			return "EEPROM Erase in progress";
		case MXCAM_STAT_EEPROM_SAVE_IN_PROG:
			return "EEPROM config save in progress";
		//Errors
		case MXCAM_ERR_FW_IMAGE_CORRUPTED:
			return "FW Image is corrupted";
		case MXCAM_ERR_FW_SNOR_FAILED:
			return "SNOR FW upgrade failed";
		case MXCAM_ERR_FW_UNSUPPORTED_FLASH_MEMORY:
			return "Unsupported Flash memory";
		case MXCAM_ERR_ERASE_SIZE:
			return "Erase size exceeds MAX_VEND_SIZE";
		case MXCAM_ERR_ERASE_UNKNOWN_AREA:
			return "Unknown area to erase";
		case MXCAM_ERR_SAVE_UNKNOWN_AREA:
			return "Unknown area to save";
		case MXCAM_ERR_SETKEY_OVER_FLOW_NO_MEM:
			return "Not enough memory to save new key on memory";
		case MXCAM_ERR_SETKEY_UNKNOWN_AREA:
			return "Unknown area to set key";
		case MXCAM_ERR_REMOVE_KEY_UNKNOWN_AREA:
			return "Unknown area to remove key";
		case MXCAM_ERR_GETVALUE_UNKNOWN_AREA:
			return "Unknown area to get key";
		case MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND:
			return "Value not found for given key";
		case MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA:
			return "Unknown area to get config size";
		case MXCAM_ERR_TCW_FLASH_READ:
			return "Failed to read TCW from camera";
		case MXCAM_ERR_TCW_FLASH_WRITE:
			return "Failed to write TCW to camera";
		case MXCAM_ERR_MEMORY_ALLOC:
			return "Failed to allocate memory on camera";
		case MXCAM_ERR_VEND_AREA_NOT_INIT:
			return "Vendor area is not initialized";
		case MXCAM_ERR_INVALID_PARAM:
			return "Invalid parameter(s)";
		case MXCAM_ERR_INVALID_DEVICE:
			return "Not a valid device";
		case MXCAM_ERR_IMAGE_SEND_FAILED:
			return "Failed to send image";
		case MXCAM_ERR_FILE_NOT_FOUND:
			return "File not found";
		case MXCAM_ERR_NOT_ENOUGH_MEMORY:
			return "Not enough memory";
		case MXCAM_ERR_NOT_VALID_IMAGE:
			return "Not a valid image";
		case MXCAM_ERR_VID_PID_ALREADY_REGISTERED:
			return "Already registered vid and Pid";
		case MXCAM_ERR_DEVICE_NOT_FOUND:
			return "Device not found";
		case MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY:
			return "Vendor area not initialized";
		case MXCAM_ERR_FEATURE_NOT_SUPPORTED:
			return "Feature not supported on this device";
		case MXCAM_ERR_I2C_READ:
			return "I2C read failed";
		case MXCAM_ERR_I2C_WRITE:
			return "I2C write failed";
		default:
			return "libusb error";
	}
}

/**
* \ingroup upgrade
* \brief mxcam_upgrade_firmware:
* 	upgrade the camera firmware
*
* \param *fw  :pointer to a structure that encapsulate fw upgrade information
* \param *callbk  :register the callbk function pointer here, _callbk_ will be
* invoked before and after the file transfer over usb
* \param is_rom_img  : true if the image is a rom image
*
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - fw is NULL
* - fw->image anf fw->bootldr are NULL
* - fw->img_media >= LAST_MEDIA
* - fw->bootldr_media >= LAST_MEDIA
* - callbk is NULL
* \retval MXCAM_ERR_IMAGE_SEND_FAILED  - if send fails
* \retval MXCAM_ERR_FW_IMAGE_CORRUPTED - found an corrupted image at camera
* \retval MXCAM_ERR_FW_UNSUPPORTED_FLASH_MEMORY - found unsupported flash memory
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	upgrade the camera with the  image information given in fw_info
*structure.<br> using this function you can upgrade  application image
*and/or bootloader  image on given program media provied in fw_info.
*<br> as a status, callbk will invoked before and after the
*file transfer over usb with FW_STATE.
*/

int mxcam_upgrade_firmware(fw_info *fw,
		void (*callbk)(FW_STATE st, const char *filename), int is_rom_img)
{
	int r;
	image_header_t header;
	FILE *fin;
	char *hdr;
	unsigned int fsize=0;
	char *cur_bootmode;

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !fw || (!fw->image && !fw->bootldr)
		  ||fw->img_media >= LAST_MEDIA ||
		  fw->bootldr_media >= LAST_MEDIA || !callbk){
		return MXCAM_ERR_INVALID_PARAM;
	}

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;
	r = tx_libusb_ctrl_cmd(FW_UPGRADE_START, FWPACKETSIZE);
	if(r)
		return r;
	/* Change the bootmode to 'usb' to recover if in case upgrade fails */
	r = mxcam_get_value(MAXIM_INFO, "BOOTMODE", &cur_bootmode);
	/* If bootmode is set and bootmode != usb, set it to usb temporary */
	if (!r && strcmp(cur_bootmode, "usb") != 0) {
		r = mxcam_set_key(MAXIM_INFO, "BOOTMODE", "usb");
		if (!r) {
			// save new key value on eeprom
			r = mxcam_save_eeprom_config(MAXIM_INFO);
		}
		if(r){
			MXPRINT("Failed setting bootmode key: '%s'\n",mxcam_error_msg(r));
			return r;
		}
	}
	if (fw->image) {
		callbk(FW_STARTED,fw->image);
		/* Tx START_TRANSFER command */
		r = tx_libusb_ctrl_cmd(START_TRANSFER, \
					(uint16_t)(fw->img_media));
		if(r)
			return r;
		/* Start sending the image */
		r = usb_send_file(devhandle,fw->image,TX_IMAGE,
				  FWPACKETSIZE,1,0,&fsize);
		if(r)
			return r;

		r = tx_libusb_ctrl_cmd(TRANSFER_COMPLETE, 0);
		if(r)
			return r;
		callbk(FW_COMPLETED,fw->image);

	}
	if (fw->bootldr) {
		callbk(FW_STARTED,fw->bootldr);
		/* Tx START_TRANSFER command */
		r = tx_libusb_ctrl_cmd(START_TRANSFER,
				       (uint16_t)(fw->bootldr_media));
		if(r)
			return r;
		/* Tx mboot header */
		header.ih_magic = ntohl(IH_MAGIC);

#if !defined(_WIN32)
		fin = fopen(fw->bootldr, "r" );
#else
		r = fopen_s(&fin, fw->bootldr, "r" );
#endif
		if (fin == (FILE *)NULL){
			return MXCAM_ERR_FILE_NOT_FOUND;
		}
		fseek(fin, 0L, SEEK_END);
		header.ih_size  = ftell(fin);
		header.ih_size  = ntohl( header.ih_size );
		fclose(fin);
		hdr = (char *)&header;
		r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			LIBUSB_ENDPOINT_OUT| LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE,
			/* bRequest      */ TX_IMAGE,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */
			(unsigned char *)hdr,
			/* wLength     */ (uint16_t)sizeof(image_header_t),
			EP0TIMEOUT);
		if ( r < 0 )
			return r;
		/* Start sending the image */
		r = usb_send_file(devhandle,fw->bootldr,TX_IMAGE,
				      FWPACKETSIZE,1,1,&fsize);
		if (r)
			return r;
		r = tx_libusb_ctrl_cmd(TRANSFER_COMPLETE,0);
		if(r)
			return r;
		callbk(FW_COMPLETED,fw->bootldr);
	}
	r = tx_libusb_ctrl_cmd(FW_UPGRADE_COMPLETE, (uint16_t)(fw->mode));
	if(r)
		return r;
	if(!is_rom_img) {
		if (cur_bootmode != NULL && strcmp(cur_bootmode, "usb") != 0) {
			/* Change the BOOTMODE back to its original value */
			r = mxcam_set_key(MAXIM_INFO, "BOOTMODE", cur_bootmode);
			if (!r) {
				/* save new key value on eeprom */
				r = mxcam_save_eeprom_config(MAXIM_INFO);
			}
			if(r){
				MXPRINT("Failed resetting bootmode key to its initial"
						"value: '%s'\n", mxcam_error_msg(r));
				return r;
			}
		}
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}
/**
* \ingroup boot
* \brief mxcam_boot_firmware:
* 	boot the camera firmware over usb
*
* \param *image  : eCos/linux kernel image name
* \param *opt_image  :initrd image in case of linux, NULL for eCos boot
* \param *callbk  :register the callbk function pointer here, _callbk_ will be
* invoked before and after the file transfer over usb
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - image is NULL
* - callbk is NULL
* \retval MXCAM_ERR_IMAGE_SEND_FAILED  - if send fails
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	boot the camera with image and/or opt_image.
*<br> as a status, function callbk will invoked before and after the
*file transfer over usb with FW_STATE.In case of eCos boot,opt_image will be NULL.
*In case of linux, opt_image will be initrd image.
*/

int mxcam_boot_firmware(const char *image, const char *opt_image,
	       void (*callbk)(FW_STATE st, const char *filename))
{
	int r;
	unsigned int fsize = 0;

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !image || !callbk)
		return MXCAM_ERR_INVALID_PARAM;

	if(!is_bootdevice(devhandle))
		return MXCAM_ERR_INVALID_DEVICE;

	callbk(FW_STARTED,image);
	r = usb_send_file(devhandle,image, 0xde,FWPACKETSIZE,0,0,&fsize);
        if (r)
		return r;
	callbk(FW_COMPLETED,image);

        fsize = 0;
	if (opt_image) {
		callbk(FW_STARTED,opt_image);
		r = usb_send_file(devhandle,opt_image,0xed,
				      FWPACKETSIZE,0,0,&fsize);
		if (r)
			return r;
		callbk(FW_COMPLETED,opt_image);
        }
	if(is_max64180(devhandle)) {
	    /* Send a Download complete command with Initrd Image size
		 * in <wValue wIndex>.
		 * Image size would be zero if no Initrd image*/
	        r = libusb_control_transfer(devhandle,
                        /* bmRequestType */
                        (LIBUSB_ENDPOINT_OUT) | LIBUSB_REQUEST_TYPE_VENDOR |
                        LIBUSB_RECIPIENT_INTERFACE,
                        /* bRequest      */ 0xad,
                        /* wValue        */ (uint16_t) (fsize >> 16),
                        /* wIndex        */ (uint16_t) (fsize & 0x0000ffff),
                        /* Data          */ NULL,
                        /* wLength       */ 0,
			EP0TIMEOUT);

        	if (r < 0)
				return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

//send partial image just to init the ddr
int mxcam_init_ddr(const char *image)
{
	int r;
	unsigned int fsize = 0, wblkcount = 0;
	int retryc = 0;
	unsigned char *trabuffer;
	unsigned int count;
	unsigned int imageaddr=0;
	struct stat stfile;
	image_header_t img_hd;
	int fwupgrd = 0;
	int fwpactsize = FWPACKETSIZE;
	unsigned char brequest = 0xde;

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !image )
		return MXCAM_ERR_INVALID_PARAM;

	if(!is_bootdevice(devhandle))
		return MXCAM_ERR_INVALID_DEVICE;

	/**********************/
	if(stat(image,&stfile))
		return MXCAM_ERR_FILE_NOT_FOUND;
	trabuffer = (unsigned char *)grab_file(image, fwpactsize, &fsize, &wblkcount);

	if (!trabuffer)
		return MXCAM_ERR_NOT_ENOUGH_MEMORY;

	/* Check that the image is bigger than the header size */
	if(fsize < sizeof(image_header_t)) {
		free(trabuffer);
		return MXCAM_ERR_NOT_VALID_IMAGE;
	}
	memcpy(&img_hd, trabuffer, sizeof(image_header_t));

	imageaddr=get_loadaddr(&img_hd);

	MXPRINT("File %s Size %d \n", image, fsize);
	MXPRINT("Blk count %d\n", wblkcount);
	MXPRINT("Image load addr %0x\n",imageaddr);

	if( fwupgrd == 0 ){
		/*Send image download address in <wValue wIndex> */
		r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE,
			/* bRequest      */ brequest - 1,
			/* wValue        */ (uint16_t) (imageaddr >> 16),
			/* wIndex        */ (uint16_t) (imageaddr & 0x0000ffff),
			/* Data          */ NULL,
			/* wLength       */ 0,
					    0);//No time out
		if (r < 0) {
			free(trabuffer);
			return MXCAM_ERR_IMAGE_SEND_FAILED;
		}
	}
	//send only 1/4 th size of the total image
	//thats sufficient to init the ddr
	for (count = 0; count < wblkcount/4; count++) {
retry:
		r = libusb_control_transfer(devhandle,
				/* bmRequestType*/
				LIBUSB_ENDPOINT_OUT |LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE,
				/* bRequest     */brequest,
				/* wValue       */(uint16_t)(count >> 16),
				/* wIndex       */(uint16_t)(count & 0x0000ffff),
				/* Data         */
				trabuffer + (count * fwpactsize),
				/* wLength       */ fwpactsize,
				0); // No timeout
		if (r != fwpactsize) {
			MXPRINT("Retry:libusb_control_transfer cnt %d\n",count);
			if (retryc <= 3) {
				retryc++;
				goto retry;
			}
			free(trabuffer);
			return MXCAM_ERR_IMAGE_SEND_FAILED;
		}
	}
	free(trabuffer);

	/*********************/
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup camerainfo
* \brief mxcam_read_eeprom_config_mem:
* 	read the config memory area stored on camera persistent storage memory
*
* \param area :area to read config data
* \param *buf :config data will be written on buf on success
* \param len  :length of config data needs to copied from camera memory
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - buf is NULL
* - len is 0
* - area >= LAST_INFO
* \retval MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY - if the vendor area not
* initialzed properly on the camera.
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	read camera configuration record from camera persistent storage memory
*/
int mxcam_read_eeprom_config_mem(CONFIG_AREA area,char *buf,unsigned short len)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !buf || len == 0 || area >= LAST_INFO)
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
       			/* bmRequestType */
                        (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                         LIBUSB_RECIPIENT_INTERFACE),
                        /* bRequest      */ GET_EEPROM_CONFIG,
                        /* wValue        */ (uint16_t)area,
                        /* MSB 4 bytes   */
                        /* wIndex        */ 0,
                        /* Data          */ (unsigned char*)buf,
                        /* wLength       */ len,
                        /* imeout*/ 	     EP0TIMEOUT
                        );
	if (r < 0) {
		MXPRINT("Failed GET_EEPROM_CONFIG %d\n", r);
		if ( r == LIBUSB_ERROR_PIPE ){
			return MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY;
		}
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return 0;
}

/**
* \ingroup camerainfo
* \brief mxcam_get_config_size:
* 	get camera configuration record size for the config area
* \param area 	   :area to get config record size
* \param *size_out :returns identified size from the camera

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - *size_out is NULL
* \retval MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA - if area >= LAST_INFO
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	get camera configuration record  size from camera
* persistent storage memory for a specific \ref CONFIG_AREA
*/
int mxcam_get_config_size(CONFIG_AREA area,unsigned short *size_out)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if( area >= LAST_INFO )
		return MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA;

	if(devhandle == NULL || !size_out)
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_EEPROM_CONFIG_SIZE,
			/* wValue        */ (uint16_t)area,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char*)size_out,
			/* wLength       */ sizeof(unsigned short),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed GET_EEPROM_CONFIG_SIZE %d\n", r);
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}
#ifndef API_EXPORTED
static int isempty_config(char *cfg_mem,int size)
{
	int count=0;
	/*
	* check if config is empty or not ?
	*/
	for (; count < size ; count++,cfg_mem++ ){
		if( *cfg_mem == '\0' ){
			return 0;
		}
	}
	return 1;
}
#endif

/**
* \ingroup configuration
* \brief mxcam_get_all_key_values:
* 	get all configuration records from camera for a sepecific \ref CONFIG_AREA
* \param area 	  :area to get all configuration records
* \param *callback  :register the callbk function pointer here, _callbk_ will be
* invoked when \ref mxcam_get_all_key_values
* gets a valid record,end of config area or found an empty config area
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* \retval MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA - if area >= LAST_INFO
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	get all configuration records from camera for
* a specific \ref CONFIG_AREA .The function _callbk_ will be invoked when
*\ref mxcam_get_all_key_values  gets a valid record,end of config area or
*found an empty config area
*/

int mxcam_get_all_key_values(CONFIG_AREA area,
	void (*callback)(GET_ALL_KEY_STATE st,int keycnt,void *key,void *value))
{
	int r;
	unsigned short usedbytes;
	unsigned short size;
	char *cfg_mem,*string,*key,*value;
#if defined(_WIN32)
	char *NextToken;
#endif
	int count=0;
	MXPRINT("%s (IN)\n",__func__);
	r = mxcam_get_config_size(area,&size);
	if (r)
		return r;
	cfg_mem = (char *)malloc(size);
	r = mxcam_read_eeprom_config_mem(area,cfg_mem,size);
	if (r)
		return r;
	//skip 4 bytes for CRC
	string = cfg_mem + 4;
	if(isempty_config(string,size)){
		if(callback)
			callback(GET_ALL_KEY_NO_KEY,0,NULL,NULL);
		free(cfg_mem);
		return MXCAM_OK;
	}
	while ( string[0] != '\0' && string[1] != '\0' ) {
		char delimter = '=';

#if !defined(_WIN32)
		key=strtok(string,&delimter);
#else
		key=strtok_s(string,&delimter,&NextToken);
#endif
		value = key + strlen(key) + 1;
		string += strlen(key) + strlen(value) + 2;
		++count;
		if(callback)
			callback(GET_ALL_KEY_VALID,count,key,value);
	}
	if(callback){
		usedbytes = (unsigned short)(string - cfg_mem - 4) ;
		callback(GET_ALL_KEY_COMPLETED,count,&usedbytes,&size);
	}
	free(cfg_mem);
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_get_ccr_size:
* 	get the size of the complete ccr list
*
* \param area 	  :area to get ccr list	size.
* \param size_out :returns identified size from the camera
*
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - size_out is not a valid pointer
* \retval MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA - if area >= LAST_INFO
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	this will read just the size of ccr list
* from camera and return it in size_out.
*/
int mxcam_get_ccr_size(CONFIG_AREA area,unsigned short *size_out)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if( area >= LAST_INFO )
		return MXCAM_ERR_GET_CFG_SIZE_UNKNOWN_AREA;

	if(devhandle == NULL || !size_out)
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_CCR_SIZE,
			/* wValue        */ (uint16_t)area,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char*)size_out,
			/* wLength       */ sizeof(unsigned short),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed GET_EEPROM_CONFIG_SIZE %d\n", r);
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;

}

/**
* \ingroup configuration
* \brief mxcam_read_ccr_mem:
* 	get the complete ccr list from camera
* \param area 	  :area to get ccr list	size.
* \param buf	  :buffer to read the ccr list
* \param len	  :size of the data to be read from camera
*
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - buf is NULL
* - len is zero
* - area >= LAST_INFO
* \retval MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY- if libusb returns LIBUSB_ERROR_PIPE
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	this will read the specified length(parameter len) size of data from camera
* and returns' it in parameter buf.
*/
int mxcam_read_ccr_mem(CONFIG_AREA area,char *buf,unsigned short len)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !buf || len == 0 || area >= LAST_INFO)
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
       			/* bmRequestType */
                        (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                         LIBUSB_RECIPIENT_INTERFACE),
                        /* bRequest      */ GET_CCR_LIST,
                        /* wValue        */ (uint16_t)area,
                        /* MSB 4 bytes   */
                        /* wIndex        */ 0,
                        /* Data          */ (unsigned char*)buf,
                        /* wLength       */ len,
                        /* imeout*/ 	     EP0TIMEOUT
                        );
	if (r < 0) {
		MXPRINT("Failed GET_CCR_LIST%d\n", r);
		if ( r == LIBUSB_ERROR_PIPE ){
			return MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY;
		}
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return 0;
}

/**
* \ingroup configuration
* \brief mxcam_get_all_ccr:
* 	get the complete ccr from camera
* \param *callbk  :register the callbk function pointer here, _callbk_ will be
* invoked before and after the file transfer over usb
*
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - buf is NULL
* - len is zero
* - area >= LAST_INFO
* \retval MXCAM_ERR_UNINITIALIZED_VENDOR_MEMORY- if libusb returns LIBUSB_ERROR_PIPE
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
*	this gets the size of the complete ccr using \ref mxcam_get_ccr_size
* and reads the coplete ccr using \ref mxcam_read_ccr_mem and finally for every ccr
* record it invokes the _callbk_ function
*/
int mxcam_get_all_ccr(CONFIG_AREA area, void (*callbk)(GET_ALL_KEY_STATE st, int keycnt, void *data))
{
	int r;
	unsigned short size;
	char *cfg_mem,*string,*key;
#if defined(_WIN32)
	char *NextToken;
#endif
	int count=0;
	MXPRINT("%s (IN)\n",__func__);
	r = mxcam_get_ccr_size(area ,&size);
	if (r)
		return r;

	if (size == 0){
		callbk(GET_ALL_KEY_NO_KEY,0,NULL);
		return MXCAM_OK;
	}
	cfg_mem = (char *)malloc(size);
	r = mxcam_read_ccr_mem(area, cfg_mem, size);
	if (r)
		return r;
	string = cfg_mem;

	while ( size != 0 ) {
#if !defined(_WIN32)
		key=strtok(string,"\n");
#else
		key=strtok_s(string,"\n",&NextToken);
#endif
		if(callbk)
			callbk(GET_ALL_KEY_VALID,0,key);
		string += strlen(key) + strlen("\n");
		size = size - (int)(strlen(key)+strlen("\n"));
		++count;
	}
	if(callbk){
		callbk(GET_ALL_KEY_COMPLETED, count, NULL);
	}
	free(cfg_mem);
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup camerainfo
* \brief mxcam_read_flash_image_header:
* 	get 64 bytes flash image header
* \param *header  :pointer to image_header_t structure
* \IMG_HDR_TYPE hdr_type : possible values of this filed are  
* \			   For running fw image header : 0,
* \			   if fw image hdr need to be read from snor : 1, for bootloader header: 2
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - header is NULL
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	get 64 bytes flash image header \ref image_header_t. This image header
*information used to get camera information. ih_name in image_header_t is used
* stored camera firmware version information
*/
int mxcam_read_flash_image_header(image_header_t *header, IMG_HDR_TYPE hdr_type)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !header)
		return MXCAM_ERR_INVALID_PARAM;

	if (hdr_type == SNOR_FW_HEADER)
		r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_SNOR_IMG_HEADER,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *) header,
			/* wLength       */ (uint16_t) sizeof(image_header_t),
			/* timeout*/   EP0TIMEOUT
			);
	else if (hdr_type == RUNNING_FW_HEADER)
		r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_IMG_HEADER,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *) header,
			/* wLength       */ (uint16_t) sizeof(image_header_t),
			/* timeout*/   EP0TIMEOUT
			);
	else if (hdr_type == BOOTLOADER_HEADER)
		r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_BOOTLOADER_HEADER,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *) header,
			/* wLength       */ (uint16_t) sizeof(image_header_t),
			/* timeout*/   EP0TIMEOUT
			);			
	else 
		return MXCAM_ERR_INVALID_PARAM;

	if (r < 0) {
		MXPRINT("Failed GET_SNOR_IMG_HEADER: %d\n", r);
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup upgrade
* \brief mxcam_read_nvm_pgm_status:
* 	get non volatile memory progamming status
* \param *status  :unsigned char pointer to retrieve programming status information
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - status is NULL
* \retval Negativevalue - upon libusb error
* \retval MXCAM_ERR_FW_SNOR_FAILED - when firmware image upgrade fails
* \retval MXCAM_ERR_FAIL - when an unknown error returns from skypecam
* \retval MXCAM_ERR_FW_IMAGE_CORRUPTED - if the image sent to skypecam is corrupted
* \retval MXCAM_OK  - upon success
*
* \remark
 * 	get non volatile memory programming status see \ref MXCAM_STAT_ERR
*/
int mxcam_read_nvm_pgm_status(unsigned char *status)
{
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !status )
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_NVM_PGM_STATUS,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ status,
			/* wLength       */ sizeof(unsigned char),
			/*  timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed GET_NVM_PGM_STATUS  %d\n", r);
		return r;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return *status;
}

/**
* \ingroup configuration
* \brief mxcam_erase_eeprom_config:
* 	erase a specific \ref CONFIG_AREA on camera persistent storage memory
* \param area 	  :area is erase the config area
* \param size 	  :size of vendor size area for maxim area size is fixed as 1K

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - area >= LAST_INFO
* - area == VENDOR_INFO && size <= 12
* \retval MXCAM_ERR_ERASE_SIZE -if  erase size exceeds MAX_VEND_SIZE
* \retval MXCAM_ERR_ERASE_UNKNOWN_AREA - unknown erase \ref CONFIG_AREA
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	erase a specific \ref CONFIG_AREA on camera persistent storage memory.
* see also \ref mxcam_get_config_size
*/
int mxcam_erase_eeprom_config(CONFIG_AREA area,unsigned short size)
{
	int r;
	unsigned char status[64];
	unsigned char cmd_sta;
	unsigned char data[] = "ERASE";

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || area >= LAST_INFO ||
		   (area == VENDOR_INFO && size <= 12 ))
		return MXCAM_ERR_INVALID_PARAM;

	if(is_bootdevice(devhandle))
                return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ ERASE_EEPROM_CONFIG,
			/* wValue        */ (uint16_t)area,
			/* wIndex        */ size,
			/* Data          */ (unsigned char*) &data,
			/* wLength       */ (uint16_t)strlen("ERASE"),
			/*  timeout*/    EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed ERASE_EEPROM_CONFIG  %d\n", r);
		return r;
	}

	while (1){
		sleep(1);
		r = libusb_control_transfer(devhandle,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ GET_NVM_PGM_STATUS,
				/* wValue        */ 0,
				/* wIndex        */ 0,
				/* Data          */ status,
				/* wLength       */ sizeof(status),
				/* timeout*/   EP0TIMEOUT
				);
		if (r < 0) {
			MXPRINT("Failed GET_PROGRAM_STATUS  %d\n", r);
			return r;
		}
		cmd_sta = *(unsigned char *)status;
		if (cmd_sta >= MXCAM_ERR_FAIL){
			return cmd_sta;
		}
		if (cmd_sta == 0){
			break;
		}
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_save_eeprom_config:
* 	save a specific \ref CONFIG_AREA on camera persistent storage memory
* \param area 	  :area is erase the config area
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - area >= LAST_INFO
* \retval MXCAM_ERR_SAVE_UNKNOWN_AREA - unknown save \ref CONFIG_AREA
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	The KEY=VALUE pairs are manipulated in camera's volatile memory,use this
* API to stored manipulated KEY=VALUE pairs on camera's non volatile memory.
*/
int mxcam_save_eeprom_config(CONFIG_AREA area)
{
	int r;
	char szCmd[] = "SAVE";
	unsigned char status[64];
	unsigned char cmd_sta;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL  || area >= LAST_INFO )
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ SAVE_EEPROM_CONFIG,
			/* wValue        */ (uint16_t)area,
			/* wIndex        */ 0,
			/* Data          */ (unsigned char*)&szCmd,
			/* wLength       */ (uint16_t)strlen(szCmd),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed SAVE_EEPROM_CONFIG  %d\n", r);
		return r;
	}

	while (1){
		sleep(1);
		r = libusb_control_transfer(devhandle,
				/* bmRequestType */
				(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
				LIBUSB_RECIPIENT_INTERFACE),
				/* bRequest      */ GET_NVM_PGM_STATUS,
				/* wValue        */ 0,
				/* wIndex        */ 0,
				/* Data          */ status,
				/* wLength       */ sizeof(status),
				/* timeout*/   EP0TIMEOUT
				);
		if (r < 0) {
			MXPRINT("Failed GET_PROGRAM_STATUS  %d\n", r);
			return r;
		}
		cmd_sta = *(unsigned char *)status;
		if (cmd_sta >= MXCAM_ERR_FAIL){
			return cmd_sta;
		}
		if (cmd_sta == 0){
			break;
		}
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;

}

/**
* \ingroup configuration
* \brief mxcam_set_key:
* 	Add/modify a KEY=VALUE pair on camera's volatile memory
* \param area 	  :area is erase the config area
* \param *keyname :KEY
* \param *value   :VALUE

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - area >= LAST_INFO
* - keyname is NULL
* - value is NULL
* \retval MXCAM_ERR_SETKEY_OVER_FLOW_NO_MEM - Not enough memory to save new
* key on memory
* \retval MXCAM_ERR_SETKEY_UNKNOWN_AREA -  Unkown area to set key
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	Add/modify a KEY=VALUE pair on camera's volatile memory
*/
int mxcam_set_key(CONFIG_AREA area, const char* keyname, const char* value)
{
	int r,size;
	unsigned char status[64];
	unsigned char cmd_sta;
	char *packet;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || area >= LAST_INFO || !keyname || !value)
		return MXCAM_ERR_INVALID_PARAM;

	if(is_bootdevice(devhandle))
                return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

    // form the packet
	size = (int)strlen(keyname) + (int)strlen(value) + 2;
	packet = malloc(size);

#if !defined(_WIN32)
	strcpy(packet,keyname);
	strcpy(&packet[strlen(keyname) + 1],value);
#else
	strcpy_s(packet,size, keyname);
	size -= (int)(strlen(keyname) + 1);
	strcpy_s(&packet[strlen(keyname) + 1], size, value);
#endif

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ SET_EEPROM_KEY_VALUE	,
			/* wValue        */ (uint16_t)area,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)packet,
			/* wLength       */ (int)strlen(keyname) + (int)strlen(value) + 2,
			/* timeout*/   EP0TIMEOUT
			);
	free(packet);
	if (r < 0) {
		MXPRINT("Failed SET_EEPROM_KEY_VALUE %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ SET_OR_REMOVE_KEY_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ status,
			/* wLength       */ sizeof(status),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed SET_OR_REMOVE_KEY_STATUS %d\n", r);
		return r;
	}

	cmd_sta = *(unsigned char *)status;
	if (cmd_sta >= MXCAM_ERR_FAIL){
		return cmd_sta;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_remove_key:
* 	remove a KEY=VALUE pair on camera's volatile memory
* \param area 	  :area is erase the config area
* \param *keyname :KEY

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - area >= LAST_INFO
* - keyname is NULL
* \retval MXCAM_ERR_REMOVE_KEY_UNKNOWN_AREA -  Unknown area to remove key
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	remove a KEY=VALUE pair on camera's volatile memory
*/
int mxcam_remove_key(CONFIG_AREA area, const char* keyname)
{
	int r;
	unsigned char status[64];
	unsigned char cmd_sta;
	char *data;

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || area >= LAST_INFO || !keyname)
		return MXCAM_ERR_INVALID_PARAM;

	data = malloc(sizeof(keyname)+1);
	if (data == NULL)
		return MXCAM_ERR_NOT_ENOUGH_MEMORY;

#if !defined(_WIN32)
	strcpy(data, keyname);
#else
	strcpy_s(data,(sizeof(keyname)+1), keyname);
#endif

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ REMOVE_EEPROM_KEY,
			/* wValue        */ (uint16_t)area,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char*) data,
			/* wLength       */ (int)strlen(keyname) + 1,
			/*  timeout*/   EP0TIMEOUT
			);
	free(data);
	if (r < 0) {
		MXPRINT("Failed REMOVE_EEPROM_KEY %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ SET_OR_REMOVE_KEY_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ status,
			/* wLength       */ sizeof(status),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed SET_OR_REMOVE_KEY_STATUS %d\n", r);
		return r;
	}

	cmd_sta = *(unsigned char *)status;
	if (cmd_sta >= MXCAM_ERR_FAIL){
		return cmd_sta;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_get_value:
* 	get VALUE for given KEY from camera's volatile memory
* \param area 	     :area is erase the config area
* \param *keyname    :KEY
* \param **value_out :On success VALUE would be copied in *value_out

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - area >= LAST_INFO
* - keyname is NULL
* - value_out is NULL
* \retval MXCAM_ERR_GETVALUE_UNKNOWN_AREA -  Unknown area to get value
* \retval MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND -  Value not found for given KEY
* \retval MXCAM_ERR_NOT_ENOUGH_MEMORY - malloc failed
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	get VALUE for given KEY from camera's volatile memory
* see also \ref mxcam_free_get_value_mem
*/
int mxcam_get_value(CONFIG_AREA area, const char* keyname, char** value_out)
{
	int r;
	unsigned char cmd_sta;
	unsigned char *value;
	char *data;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !keyname || !value_out)
		return MXCAM_ERR_INVALID_PARAM;

	if( area >= LAST_INFO )
		return MXCAM_ERR_GETVALUE_UNKNOWN_AREA;

	value = malloc(FWPACKETSIZE);
	data = malloc(strlen(keyname)+1);
	if (value == NULL || data == NULL)
		return MXCAM_ERR_NOT_ENOUGH_MEMORY;

#if !defined(_WIN32)
	strcpy(data, keyname);
#else
	strcpy_s(data, (sizeof(keyname)+1), keyname);
#endif

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ REQ_GET_EEPROM_VALUE,
			/* wValue        */ (uint16_t)area,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)data,
			/* wLength       */ (int)strlen(keyname) + 1,
			/* timeout*/   EP0TIMEOUT
			);
	free(data);
	if (r < 0) {
		MXPRINT("Failed REQ_GET_EEPROM_VALUE %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_EEPROM_VALUE,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ value,
			/* wLength       */ FWPACKETSIZE * sizeof(uint8_t),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed GET_EEPROM_VALUE %d\n", r);
		return r;
	}
	cmd_sta = *(unsigned char *)value;
	if (cmd_sta == MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND){
		free(value);
		*value_out = NULL;
		return MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND;
	}
	// skip the status byte;
	value += 1;
	*value_out =(char *) value;
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_get_ccrvalue:
* 	get VALUE for given CCR KEY from camera's CCR record
* \param *keyname    :KEY
* \param **value_out :On success VALUE would be copied in *value_out

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - keyname is NULL
* - value_out is NULL
* \retval MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND -  Value not found for given KEY
* \retval MXCAM_ERR_NOT_ENOUGH_MEMORY - malloc failed
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	get VALUE for the provided CCR KEY from camera's CCR Record
*/
int mxcam_get_ccrvalue(const char* keyname, char** value_out)
{
	int r;
	unsigned char cmd_sta;
	unsigned char *value;
	char *data;

	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !keyname || !value_out)
		return MXCAM_ERR_INVALID_PARAM;

	value = malloc(FWPACKETSIZE);
	data = malloc(sizeof(keyname)+1);

	if (value == NULL || data == NULL)
		return MXCAM_ERR_NOT_ENOUGH_MEMORY;

#if !defined(_WIN32)
	strcpy(data, keyname);
#else
	strcpy_s(data,(sizeof(keyname)+1), keyname);
#endif


	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ REQ_GET_CCRKEY_VALUE,
			/* wValue        */ (uint16_t)MAXIM_INFO,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)data,
			/* wLength       */ (int)strlen(keyname) + 1,
			/* timeout*/   EP0TIMEOUT
			);
	free(data);
	if (r < 0) {
		MXPRINT("Failed REQ_GET_EEPROM_VALUE %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_CCRKEY_VALUE,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ value,
			/* wLength       */ FWPACKETSIZE * sizeof(uint8_t),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed GET_EEPROM_VALUE %d\n", r);
		return r;
	}
	cmd_sta = *(unsigned char *)value;
	if (cmd_sta == MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND){
		free(value);
		*value_out = NULL;
		return MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND;
	}
	// skip the status byte;
	value += 1;
	*value_out =(char *) value;
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_set_configutaion:
*       set VALUE for configurtion like bulk or isoc to the config descritor
* \param config	     :config value to be set in config descriptor
* \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL
* \retval MXCAM_ERR_INVALID_DEVICE -  Device no booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/
int mxcam_set_configuration(EP_CONFIG config)
{
	int r = 0,i;
	int num = 0;
	struct libusb_config_descriptor *handle = NULL;
	EP_CONFIG conf=0;

	MXPRINT("%s (IN)\n",__func__);

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;

	r = libusb_get_configuration(devhandle, (int*)&conf);
	if(r < 0) {
		MXPRINT("Failed GET_CONFIGURATION  %d\n", r);
		return r;
	}

	if ( conf == config ){
		MXPRINT("present configuration is %d\n",conf);
		return 0;
	}


	if(devhandle == NULL )
		return MXCAM_ERR_INVALID_PARAM;

	r =  libusb_get_active_config_descriptor( libusb_get_device(devhandle),&handle);
	if (r || (handle == NULL)){
		MXPRINT("Failed REQ_INTERFACE_NUMBER  %d\n", r);
		libusb_free_config_descriptor(handle);
		return r;
	 }

	num = handle->bNumInterfaces;


	for(i=0;i<num;i++){
		r = libusb_detach_kernel_driver(devhandle, i);
	}

	r = libusb_set_configuration(devhandle,config);
	if(r < 0) {
		MXPRINT("Failed SET_CONFIGURATION  %d\n", r);
		libusb_free_config_descriptor(handle);
		return r;
	}

	for(i=0;i<num;i++){
		r = libusb_attach_kernel_driver(devhandle , i);
	}

	libusb_free_config_descriptor(handle);

	MXPRINT("%s (OUT)\n",__func__);
	return 0;
}

/**
* \ingroup configuration
* \brief mxcam_i2c_write:
* 	will do a i2c write operation in camera
* \param *addr		: i2c device address
* \param *subaddr	: i2c device register
* \param *value		: value to be written
* \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/

int mxcam_i2c_write(uint16_t inst, uint16_t type, i2c_payload_t *payload)
{
    i2c_data_t i2c_stat;
    int r, count = 0;

    MXPRINT("%s (IN)\n",__func__);

    if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

    if(!(is_uvcdevice(devhandle)))
	return MXCAM_ERR_INVALID_DEVICE;

    do {
        r = libusb_control_transfer(devhandle,
                                    /* bmRequestType */
                                    (LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                                     LIBUSB_RECIPIENT_INTERFACE),
                                    /* bRequest      */ I2C_WRITE,
                                    /* wValue        */ (uint16_t)inst,
                                    /* MSB 4 bytes   */
                                    /* wIndex        */ (uint16_t)type,
                                    /* Data          */ (unsigned char *)payload,
                                    /* wLength       */ sizeof(i2c_payload_t),
                                    /* timeout*/   EP0TIMEOUT
                                    );
        if (r < 0) {
            MXPRINT("Failed I2C_WRITE %d\n", r);
            return r;
        }

        r = libusb_control_transfer(devhandle,
                                    /* bmRequestType */
                                    (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                                     LIBUSB_RECIPIENT_INTERFACE),
                                    /* bRequest      */ I2C_WRITE_STATUS,
                                    /* wValue        */ 0,
                                    /* wIndex        */ 0,
                                    /* Data          */ (unsigned char *)&i2c_stat,
                                    /* wLength       */ sizeof(i2c_data_t),
                                    /* timeout*/   EP0TIMEOUT
                                    );
        if (r < 0) {
            MXPRINT("Failed I2C_WRITE_STATUS %d\n", r);
            return r;
        }
    } while ((i2c_stat.len < 0) && (count <= MXCAM_I2C_MAX_RETRIES));

    if (i2c_stat.len < 0) {
	MXPRINT("i2c write error\n");
	return MXCAM_ERR_I2C_WRITE;
    }

    MXPRINT("%s (OUT)\n",__func__);
    return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_i2c_read:
*       will do a i2c read operation in camera
* \param *addr          : i2c device address
* \param *subaddr       : i2c device register
* \param *value         : return value
* \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/
int mxcam_i2c_read(uint16_t inst, uint16_t type, i2c_payload_t *payload)
{
    int r, count = 0;
    i2c_data_t data;

    MXPRINT("%s (IN)\n",__func__);

    if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

    if(!(is_uvcdevice(devhandle)))
	return MXCAM_ERR_INVALID_DEVICE;

    do {
        r = libusb_control_transfer(devhandle,
                                    /* bmRequestType */
                                    (LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                                     LIBUSB_RECIPIENT_INTERFACE),
                                    /* bRequest      */ I2C_READ,
                                    /* wValue        */ (uint16_t)inst,
                                    /* MSB 4 bytes   */
                                    /* wIndex        */ (uint16_t)type,
                                    /* Data          */ (unsigned char *)payload,
                                    /* wLength       */ (uint16_t)sizeof(i2c_payload_t),
                                    /* timeout*/   EP0TIMEOUT
                                    );
        if (r < 0) {
            MXPRINT("Failed I2C_READ %d\n", r);
            return r;
        }

        r = libusb_control_transfer(devhandle,
                                    /* bmRequestType */
                                    (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                                     LIBUSB_RECIPIENT_INTERFACE),
                                    /* bRequest      */ I2C_READ_STATUS,
                                    /* wValue        */ 0,
                                    /* wIndex        */ 0,
                                    /* Data          */ (unsigned char *)&data,
                                    /* wLength       */ sizeof(i2c_data_t),
                                    /* timeout*/   EP0TIMEOUT
                                    );
        if (r < 0) {
            MXPRINT("Failed I2C_READ_STATUS %d\n", r);
            return r;
        }
        count++;
    } while ((data.len < 0) && (count <= MXCAM_I2C_MAX_RETRIES));

    if (data.len < 0) {
	MXPRINT("i2c read error\n");
	return MXCAM_ERR_I2C_READ;
    }

    memcpy(payload->data.buf, data.buf, data.len);

    MXPRINT("%s (OUT)\n",__func__);
    return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_tcw_write:
* 	write a SPI device Timing Control Word on camera
* \param value		: value to be written
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/
int mxcam_tcw_write(uint32_t value)
{
	unsigned char status[64];
	unsigned char cmd_sta = MXCAM_ERR_FAIL;
	int r;

	MXPRINT("%s (IN)\n",__func__);

	if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ TCW_WRITE,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)&value,
			/* wLength       */ sizeof(value),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed TCW_WRITE, %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ TCW_WRITE_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ status,
			/* wLength       */ sizeof(status),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed TCW_WRITE_STATUS %d\n", r);
		return r;
	}
	cmd_sta = *(unsigned char *)status;
	if (cmd_sta >= MXCAM_ERR_FAIL){
		return cmd_sta;
	}
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_tcw_read:
*       read SPI Device's Timimg Control Word
* \param *value         : return value
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/
int mxcam_tcw_read(uint32_t *value)
{
        int r;
	unsigned char status[64];
	unsigned char cmd_sta;
	char data[] = "TCW";

	MXPRINT("%s (IN)\n",__func__);

	if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ TCW_READ,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *) data,
			/* wLength       */ (uint16_t)strlen(data),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed TCW_READ %d\n", r);
		return r;
	}
	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ TCW_READ_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)&status,
			/* wLength       */ sizeof(status),
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed TCW_READ_STATUS %d\n", r);
		return r;
	}
	cmd_sta = *(unsigned char *)status;
	if (cmd_sta >= MXCAM_ERR_FAIL){
		return cmd_sta;
	}
	//skip the status byte
	memcpy(value, (status+1), sizeof(uint32_t));
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_isp_write:
*   will do a isp write operation in camera
* \param  addr      : isp register address
* \param  value     : value to be written
* \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/

int mxcam_isp_write(uint16_t addr, uint32_t value)
{
    unsigned char status[64];
    unsigned char cmd_sta;
    int r;

    MXPRINT("%s (IN)\n",__func__);

    if(is_max64180(devhandle))
	return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

    if(!(is_uvcdevice(devhandle)))
        return MXCAM_ERR_INVALID_DEVICE;

    r = libusb_control_transfer(devhandle,
            /* bmRequestType */
            (LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
             LIBUSB_RECIPIENT_INTERFACE),
            /* bRequest      */ ISP_WRITE,
            /* wValue        */ (uint16_t)addr,
            /* wIndex        */ 0,
            /* Data          */ (unsigned char *)&value,
            /* wLength       */ sizeof(value),
            /* timeout*/   EP0TIMEOUT
            );

    if (r < 0) {
        MXPRINT("Failed ISP_WRITE %d\n", r);
        return r;
    }

    r = libusb_control_transfer(devhandle,
            /* bmRequestType */
            (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
             LIBUSB_RECIPIENT_INTERFACE),
            /* bRequest      */ ISP_WRITE_STATUS,
            /* wValue        */ 0,
            /* wIndex        */ 0,
            /* Data          */ status,
            /* wLength       */ sizeof(status),
            /* timeout*/   EP0TIMEOUT
            );

    if (r < 0) {
        MXPRINT("Failed ISP_WRITE_STATUS %d\n", r);
        return r;
    }

    cmd_sta = *(unsigned char *)status;
    if (cmd_sta >= MXCAM_ERR_FAIL){
        return cmd_sta;
    }

    MXPRINT("%s (OUT)\n",__func__);

    return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_isp_read:
*       will do a isp read operation in camera
* \param  addr          : isp device address
* \param *value         : return value
* \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL
* \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
*/
int mxcam_isp_read(uint16_t addr, uint32_t *value)
{
    int r;
    char data[] = "SAVE";

    MXPRINT("%s (IN)\n",__func__);

    if(is_max64180(devhandle))
	return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

    if(!(is_uvcdevice(devhandle)))
        return MXCAM_ERR_INVALID_DEVICE;

    r = libusb_control_transfer(devhandle,
                    /* bmRequestType */
                    (LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                        LIBUSB_RECIPIENT_INTERFACE),
                    /* bRequest      */ ISP_READ,
                    /* wValue        */ (uint16_t)addr,
                    /* wIndex        */ 0,
                    /* Data          */ (unsigned char *) data,
                    /* wLength       */ (uint16_t)strlen(data),
                    /* timeout*/   EP0TIMEOUT
                    );

    if (r < 0) {
        MXPRINT("Failed ISP_READ %d\n", r);
        return r;
    }

    r = libusb_control_transfer(devhandle,
                    /* bmRequestType */
                    (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                        LIBUSB_RECIPIENT_INTERFACE),
                    /* bRequest      */ ISP_READ_STATUS,
                    /* wValue        */ 0,
                    /* wIndex        */ 0,
                    /* Data          */ (unsigned char *)value,
                    /* wLength       */ sizeof(uint32_t),
                    /* timeout*/   EP0TIMEOUT
                    );

    if (r < 0) {
        MXPRINT("Failed ISP_READ_STATUS %d\n", r);
        return r;
    }

    MXPRINT("%s (OUT)\n",__func__);

    return MXCAM_OK;
}

int mxcam_isp_enable(uint32_t enable)
{
    int r;
    unsigned char status[64];
    char data[] = "SAVE";

    MXPRINT("%s (IN)\n",__func__);

#ifndef DONT_CHK_DEV_ID
    if(!(is_uvcdevice(devhandle)))
        return MXCAM_ERR_INVALID_DEVICE;
#endif

    r = libusb_control_transfer(devhandle,
                    /* bmRequestType */
                    (LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
                        LIBUSB_RECIPIENT_INTERFACE),
                    /* bRequest      */ ISP_ENABLE,
                    /* wValue        */ (uint16_t) enable,
                    /* wIndex        */ 0,
                    /* Data          */ (unsigned char *)data,
                    /* wLength       */ (uint16_t)strlen(data),
                    /* timeout*/   EP0TIMEOUT
                    );

    if (r < 0) {
        MXPRINT("Failed ISP_ENABLE %d\n", r);
        return r;
    }

    r = libusb_control_transfer(devhandle,
                    /* bmRequestType */
                    (LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
                        LIBUSB_RECIPIENT_INTERFACE),
                    /* bRequest      */ ISP_ENABLE_STATUS,
                    /* wValue        */ 0,
                    /* wIndex        */ 0,
                    /* Data          */ status,
                    /* wLength       */ sizeof(status),
                    /* timeout*/   EP0TIMEOUT
                    );

    if (r < 0) {
        MXPRINT("Failed ISP_ENABLE_STATUS %d\n", r);
        return r;
    }

    MXPRINT("%s (OUT)\n",__func__);
    return MXCAM_OK;
}

/**
* \ingroup configuration
* \brief mxcam_usbtest:
* 	Set specified USB test mode
* \param testmode   :test mode to set
* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - testmode is out of range
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*/
int mxcam_usbtest(uint32_t testmode)
{
#define USB_FEATURE_TESTMODE 2
	int r;
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || (testmode > 5))
		return MXCAM_ERR_INVALID_PARAM;

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */ 0,
			/* bRequest      */ LIBUSB_REQUEST_SET_FEATURE,
			/* wValue        */ USB_FEATURE_TESTMODE,
			/* wIndex        */ (testmode << 8),
			/* Data          */ NULL,
			/* wLength       */ 0,
			/* timeout*/   EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed USB testmode %d\n", r);
		return r;
	}

	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
 * \ingroup configuration
 * \brief mxcam_qcc_write:
 *   will do a qcc write operation in camera
 * \param  bid       : qcc block id
 * \param  addr      : qcc register address
 * \param  length    : length of the register
 * \param  value     : value to be written
 * \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL or length is
 * 	not 1, 2 or 4
 * \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
 * \retval Negativevalue - upon libusb error
 * \retval MXCAM_OK  - upon success
 *
 */
int mxcam_qcc_write(uint16_t bid, uint16_t addr, uint16_t length, uint32_t value)
{
	unsigned char status[64];
	unsigned char cmd_sta;
	int r;


	MXPRINT("%s (IN)\n",__func__);

	if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;

	switch (length) {
	case 1:
		value &= 0xFF;
		break;
	case 2:
		value &= 0xFFFF;
		break;
	case 4:
		break;
	default:
		return MXCAM_ERR_INVALID_PARAM;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ QCC_WRITE,
			/* wValue        */ bid,
			/* MSB 4 bytes   */
			/* wIndex        */ addr,
			/* Data          */ (unsigned char *)&value,
			/* wLength       */ length,
			/* timeout       */ EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed QCC_WRITE %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ QCC_WRITE_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ status,
			/* wLength       */ sizeof(status),
			/* timeout       */ EP0TIMEOUT
			);

	if (r < 0) {
		MXPRINT("Failed QCC_WRITE_STATUS %d\n", r);
		return r;
	}

	cmd_sta = *(unsigned char *)status;
	if (cmd_sta >= MXCAM_ERR_FAIL){
		return cmd_sta;
	}

	MXPRINT("%s (OUT)\n",__func__);

	return MXCAM_OK;
}

/**
 * \ingroup configuration
 * \brief mxcam_isp_read:
 *       will do a isp read operation in camera
 * \param  bid       : qcc block id
 * \param  addr      : qcc register address
 * \param  length    : length of the register
 * \param *value     : return value
 * \retval MXCAM_ERR_INVALID_PARAM - devhandle is NULL or length is
 * 	not 1, 2 or 4
 * \retval MXCAM_ERR_INVALID_DEVICE -  Device not booted completely
 * \retval Negativevalue - upon libusb error
 * \retval MXCAM_OK  - upon success
 *
 */
int mxcam_qcc_read(uint16_t bid, uint16_t addr, uint16_t length, uint32_t *value)
{
	int r;
	int mask;

	MXPRINT("%s (IN)\n",__func__);

	if(is_max64180(devhandle))
		return MXCAM_ERR_FEATURE_NOT_SUPPORTED;

	if(!(is_uvcdevice(devhandle)))
		return MXCAM_ERR_INVALID_DEVICE;

	switch (length) {
	case 1:
		mask = 0xFF;
		break;
	case 2:
		mask = 0xFFFF;
		break;
	case 4:
		mask = 0xFFFFFFFF;
		break;
	default:
		return MXCAM_ERR_INVALID_PARAM;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ QCC_READ,
			/* wValue        */ bid,
			/* MSB 4 bytes   */
			/* wIndex        */ addr,
			/* Data          */ (unsigned char *)&length,
			/* wLength       */ 1,
			/* timeout       */ EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed QCC_READ %d\n", r);
		return r;
	}

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ QCC_READ_STATUS,
			/* wValue        */ 0,
			/* wIndex        */ 0,
			/* Data          */ (unsigned char*)value,
			/* wLength       */ length,
			/* timeout       */ EP0TIMEOUT
			);
	if (r < 0) {
		MXPRINT("Failed QCC_READ_STATUS %d\n", r);
		return r;
	}

	*value &= mask;

	MXPRINT("%s (OUT)\n",__func__);

	return MXCAM_OK;
}


/**
* \ingroup configuration
* \brief mxcam_free_get_value_mem:
* 	free the resource allocated by \ref mxcam_get_value
* \param *value_mem   :pointer to a memory,allocated by \ref mxcam_get_value

* \retval MXCAM_ERR_INVALID_PARAM - if any one of the following condition meet
* - devhandle is NULL
* - value_mem  is NULL
* \retval MXCAM_ERR_GETVALUE_UNKNOWN_AREA -  Unknown area to get value
* \retval MXCAM_ERR_GETVLAUE_KEY_NOT_FOUND -  Value not found for given KEY
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	free the resource allocated by \ref mxcam_get_value
*/
int mxcam_free_get_value_mem(char* value_mem)
{
	MXPRINT("%s (IN)\n",__func__);
	if(devhandle == NULL || !value_mem )
		return MXCAM_ERR_INVALID_PARAM;

	// add status byte first before the free
	value_mem -= 1 ;
	free(value_mem);
	MXPRINT("%s (OUT)\n",__func__);
	return MXCAM_OK;
}

/**
* \ingroup misc
* \brief mxcam_reset:
* 	reboot the camera
*
* \retval Negativevalue - upon libusb error
* \retval MXCAM_OK  - upon success
*
* \remark
* 	reboot the camera
*/
int mxcam_reset(void)
{
	int ret;
	ret = tx_libusb_ctrl_cmd (RESET_BOARD, 0);
	/* 
	 * device gets disconnected and libusb returns no device
	 * consider it as success
	 */
	if (ret == LIBUSB_ERROR_NO_DEVICE)
		ret = MXCAM_OK;
	return ret;
}

/**
* \page examplecode example code snippet to understand libmxcam API usage
*
\code
static int open_device()
{
	int ret=1;

	if (poll)
		ret = mxcam_poll_one();
	else if (poll_new)
		ret = mxcam_poll_new();
	else if (bus != 0 && dev_addr != 0)
		ret = mxcam_open_by_busaddr(bus, dev_addr);
	else
		ret = mxcam_open_by_devnum(dev_num);

	return ret;
}

static void reboot_maxim_camera(void)
{
	int r = 0;
	r = open_device();
	if (r){
		printf("Failed to open_device (%s)\n",mxcam_error_msg(r));

	}
	r = mxcam_reset();
	if (r) {
		printf("Failed %s (%s)\n",__func__,mxcam_error_msg(r));
	}
	mxcam_close();
}
\endcode
*/

/**
* \ingroup library
* \brief mxcam_register_vidpid:
* 	register a device with its vid/pid
* \param vid   :vendor id
* \param pid   :product id
* \param *desc :device description
* \param chip  :camera soc type

* \retval MXCAM_ERR_VID_PID_ALREADY_REGISTERED - vid and pid already registered
* \retval MXCAM_OK  - upon success
*
* \remark
* 	DEPRECATED.
* 	This was needed for the scan, open and poll functions to work.
*/
int mxcam_register_vidpid(int vid, int pid, char *desc, SOC_TYPE soc, DEVICE_TYPE dev)
{
	/* For backward compatibility */
	return MXCAM_OK;
}

/**
* \ingroup library
* \brief mxcam_scan:
* 	Scan for plugged Maxim cameras.
* \param **devlist   :list of registered devices
* \retval Negativevalue - upon libusb error
* \retval Positivevalue - number of device(s) found
* \retval MXCAM_OK  - upon success
* \remark
*/
#define CC_VIDEO 0x0e
#define VENDOR_SPECIFIC 0xff
int mxcam_scan(struct mxcam_devlist **devlist)
{
	struct libusb_device *dev;
	size_t i = 0;
	int r = 0, found;
	struct mxcam_devlist *usbdev, *usbdev_prev;

	if (devlist)
		*devlist = NULL;
	usbdev = NULL;
	usbdev_prev = NULL;

	r = libusb_init(&dcontext);
	if (r < 0) {
		return r;
	}

	if(devs != NULL && devs[0] != NULL)
		libusb_free_device_list(devs, 1);

	/* If cache exists, free it */
	if(devlist_cache != NULL) {
		struct mxcam_devlist *d = devlist_cache;
		while(devlist_cache != NULL) {
			d = devlist_cache;
			devlist_cache = devlist_cache->next;
			free(d);
		}
	}
	if (libusb_get_device_list(dcontext, &devs) < 0)
		return -1;

	found = 0;
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *conf_desc;
		const struct libusb_interface *dev_interface;
		const struct libusb_interface_descriptor *altsetting;
		int data[2] = {-1, -1};
		DEVICE_TYPE type = DEVTYPE_UNKNOWN;
		SOC_TYPE soc = SOC_UNKNOWN;

		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
			continue;

		r = libusb_get_config_descriptor_by_value(dev, 1, &conf_desc);
		if(r < 0)
			continue;

		/* Detecting the type and state of camera */
#if !defined(_WIN32)
		dev_interface = conf_desc->interface;
#else
		dev_interface = conf_desc->dev_interface;
#endif

		altsetting = dev_interface->altsetting;
		/* We are only interested in devices whose first USB class is
		 *  - a Vendor specific class
		 *  - a UVC class
		 * */
		if (altsetting->bInterfaceClass != VENDOR_SPECIFIC
				&& altsetting->bInterfaceClass != CC_VIDEO) {
			libusb_free_config_descriptor(conf_desc);
			continue;
		}

		/* Open the device to communicate with it */
		r = libusb_open(dev, &devhandle);
		if (r < 0) {
			libusb_free_config_descriptor(conf_desc);
			continue;
		}

		/* Send Vendor specific command to determine if the
		 * USB device is a Maxim camera*/
		r = mxcam_whoru((char *)&data);
		if (r<0) {
			/* Did not respond: this is not a Maxim camera
			 * or it is an old generation Maxim camera that
			 * support CMD_WHO_R_U. We don't support them
			 * in this function (see mxcam_scan_oldcam())*/
			libusb_free_config_descriptor(conf_desc);
			libusb_close(devhandle);
			continue;
		}

		//printf("(%x:%x) chip:mode = %i:%i\n",
		//		desc.idVendor, desc.idProduct,
		//		data[0], data[1]);

		/* Get the type of Camera */
		switch(data[0]) {
		case MAX64380:
		case MAX64480:
			soc = data[0];
			break;
		case MAX64180:
		default:
			/* Camera type not supported.
			 * Skip to the next USB device */
			libusb_free_config_descriptor(conf_desc);
			libusb_close(devhandle);
			continue;
		}

		/* Get the mode in which the camera is */
		switch(data[1])  {
		case 0:
			type = DEVTYPE_UVC;
			break;
		case 1:
			type = DEVTYPE_BOOT;
			break;
		default:
			/* Camera mode mode not supported.
			 * Skip to the next USB device */
			libusb_free_config_descriptor(conf_desc);
			libusb_close(devhandle);
			continue;
		}

		libusb_close(devhandle);

		found++;
		usbdev = malloc(sizeof(struct mxcam_devlist));
		usbdev->vid = desc.idVendor;
		usbdev->pid = desc.idProduct;
		usbdev->desc = "Camera";
		usbdev->bus = libusb_get_bus_number(dev);
		usbdev->addr = libusb_get_device_address(dev);
		usbdev->type = type;
		usbdev->soc = soc;
		usbdev->dev = dev;
		usbdev->next = NULL;
		if (usbdev_prev)
			usbdev_prev->next = usbdev;
		if (found == 1 && devlist)
			*devlist = usbdev;
		usbdev_prev = usbdev;
		libusb_free_config_descriptor(conf_desc);
	}

	devlist_cache = *devlist;
	devhandle = NULL;
	return found;
}

/**
* \ingroup library
* \brief mxcam_scan_old:
* 	Scan for plugged old generation Maxim cameras.
* \param **devlist   :list of registered devices
* \retval Negativevalue - upon libusb error
* \retval Positivevalue - number of device(s) found
* \retval MXCAM_OK  - upon success
* \remark
*/
int mxcam_scan_oldcam(struct mxcam_devlist **devlist)
{
	struct libusb_device *dev;
	size_t i = 0;
	int tmp=1;
	int r = 0, found;
	struct mxcam_devlist *usbdev, *usbdev_prev;

	if (devlist)
		*devlist = NULL;
	usbdev = NULL;
	usbdev_prev = NULL;

	r = libusb_init(&dcontext);
	if (r < 0) {
		return r;
	}

	if(devs != NULL && devs[0] != NULL)
		libusb_free_device_list(devs, 1);

	/* If cache exists, free it */
	if(devlist_cache != NULL) {
		struct mxcam_devlist *d = devlist_cache;
		while(devlist_cache != NULL) {
			d = devlist_cache;
			devlist_cache = devlist_cache->next;
			free(d);
		}
	}
	if (libusb_get_device_list(dcontext, &devs) < 0)
		return -1;

	found = 0;
	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		struct libusb_config_descriptor *conf_desc;
		const struct libusb_interface *dev_interface;
		const struct libusb_interface_descriptor *altsetting;
		unsigned int buf;
		int data[2] = {-1, -1};
		DEVICE_TYPE type = DEVTYPE_UNKNOWN;
		SOC_TYPE soc = SOC_UNKNOWN;

		r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
			continue;

		r = libusb_get_config_descriptor_by_value(dev, 1, &conf_desc);
		if(r < 0)
			continue;

		/* Detecting the type and state of camera */
#if !defined(_WIN32)
		dev_interface = conf_desc->interface;
#else
		dev_interface = conf_desc->dev_interface;
#endif
		altsetting = dev_interface->altsetting;

		/* Skip cameras that supports CMD_WHO_R_U */
		if (altsetting->bInterfaceClass == VENDOR_SPECIFIC ||
				altsetting->bInterfaceClass == CC_VIDEO) {

			/* Open the USB device */
			r = libusb_open(dev, &devhandle);
			if (r < 0) {
				libusb_free_config_descriptor(conf_desc);
				continue;
			}

			/* Send CMD_WHO_R_U request */
			r = mxcam_whoru((char *)&data);
			if (r >= 0) {
				/* Got an answer: check whether this is a
				 * legal answer */
				if(data[0] >= 0 && data[0] < NUM_SOC &&
					data[1] >= 0 && data[1] < NUM_DEVTYPE) {
					/* This is a legal answer: this a camera
					 * supporting CMD_WHO_R_U.
					 * We skip it for this function. */
					libusb_free_config_descriptor(conf_desc);
					libusb_close(devhandle);
					continue;
				}
			}
		}

		/* Vendor specific class for 'bootloader' state */
		if (altsetting->bInterfaceClass == VENDOR_SPECIFIC) {
			/* Check if it is a Maxim bootloader by trying
			 * to boot a fake firmware*/
			r = libusb_control_transfer(devhandle,
					/* bmRequestType */
					LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
					LIBUSB_RECIPIENT_INTERFACE,
					/* bRequest      */ 0xed ,
					/* wValue        */ 0,
					/* wIndex        */ 1,
					/* Data          */ (unsigned char *) &tmp,
					/* wLength       */ 4,
					10);
			if ((r == -9) || (r == -99)) {
				libusb_free_config_descriptor(conf_desc);
				libusb_close(devhandle);
				continue;
			}

			type = DEVTYPE_BOOT;

			/* Detect wheter it is 64380 old bootloader or
			 * 64180 bootloader. */
			r = libusb_control_transfer(devhandle,
					/* bmRequestType */
					(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
					 LIBUSB_RECIPIENT_INTERFACE),
					/* bRequest      */ GET_EEPROM_CONFIG,
					/* wValue        */ (uint16_t)MAXIM_INFO,
					/* MSB 4 bytes   */
					/* wIndex        */ 0,
					/* Data          */ (unsigned char*)&buf,
					/* wLength       */ sizeof(unsigned int),
					/* Timeout*/ 	    10
					);

			if (r < 0){
				soc = MAX64380;
			} else
				soc = MAX64180;

			libusb_close(devhandle);

		/* Video class for 'booted' state */
		} else if (altsetting->bInterfaceClass == CC_VIDEO) {
			type = DEVTYPE_UVC;

			/* Detect whether it is 64380 old firmware ot 64180. */
			/* Make a QCC read to detect if it is a 64180 or a
			 * 64380: only 64380's firmware supports it */
			tmp = 4;
			r = libusb_control_transfer(devhandle,
					/* bmRequestType */
					(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
					 LIBUSB_RECIPIENT_INTERFACE),
					/* bRequest      */ QCC_READ,
					/* wValue        */ 0x6,
					/* MSB 4 bytes   */
					/* wIndex        */ 0xfc,
					/* Data          */ (unsigned char *)&tmp,
					/* wLength       */ 1,
					/* timeout       */ 100
					);
			r = libusb_control_transfer(devhandle,
					/* bmRequestType */
					(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
					 LIBUSB_RECIPIENT_INTERFACE),
					/* bRequest      */ QCC_READ_STATUS,
					/* wValue        */ 0,
					/* wIndex        */ 0,
					/* Data          */ (unsigned char*)&tmp,
					/* wLength       */ 4,
					/* timeout       */ 100
					);

			if(tmp == 0x500 || tmp == 0x510){
				soc = MAX64380;
			} else
				soc = MAX64180;

			libusb_close(devhandle);
		} else {
			libusb_free_config_descriptor(conf_desc);
			continue;
		}

		found++;
		usbdev = malloc(sizeof(struct mxcam_devlist));
		usbdev->vid = desc.idVendor;
		usbdev->pid = desc.idProduct;
		usbdev->desc = "Camera";
		usbdev->bus = libusb_get_bus_number(dev);
		usbdev->addr = libusb_get_device_address(dev);
		usbdev->type = type;
		usbdev->soc = soc;
		usbdev->dev = dev;
		usbdev->next = NULL;
		if (usbdev_prev)
			usbdev_prev->next = usbdev;
		if (found == 1 && devlist)
			*devlist = usbdev;
		usbdev_prev = usbdev;
		libusb_free_config_descriptor(conf_desc);
	}

	devlist_cache = *devlist;
	devhandle = NULL;
	return found;
}

static int device_count = 0;

/**
* \ingroup library
* \brief mxcam_open_by_devnum:
*	open a device based on device number
* \param dev_num   :device number
* \param devlist   : scanned device list

* \retval MXCAM_ERR_INVALID_DEVICE - Failed to open device
* \retval Negativevalue - upon libusb error
* \retval MXCAM_ERR_DEVICE_NOT_FOUND - Device not found
* \retval MXCAM_OK  - upon success
* \remark
* use \ref mxcam_scan to get valid device number
*/
int mxcam_open_by_devnum(int dev_num, struct mxcam_devlist *devlist)
{
	int r;

	if(devlist != NULL) {
		device_count++;
		//if there is no next device and device is still not found return error
		if((devlist->next == NULL) && (dev_num > device_count))
			return MXCAM_ERR_INVALID_DEVICE;	

		if(dev_num != device_count)
			return MXCAM_ERR_INVALID_PARAM;
	
		r = libusb_open(devlist->dev, &devhandle);
		if (r < 0) {
			return MXCAM_ERR_INVALID_DEVICE;
		}
		cur_mxdev = devlist;
		return MXCAM_OK;
	}
	return MXCAM_ERR_INVALID_DEVICE;
}

/**
* \ingroup library
* \brief mxcam_open_by_busaddr:
*	Open a device based on its bus number and device address
* \param bus   :device number
* \param addr   :device address

* \retval MXCAM_ERR_INVALID_DEVICE - Failed to open device
* \retval Negativevalue - upon libusb error
* \retval MXCAM_ERR_DEVICE_NOT_FOUND - Device not found
* \retval MXCAM_OK on success
* \remark
* use \ref mxcam_scan to get valid bus number and address
*/
int mxcam_open_by_busaddr(int bus, int addr, struct mxcam_devlist *devlist)
{
	int r;

	if(devlist != NULL) {
		if(bus != devlist->bus || addr != devlist->addr) {
			if(devlist->next == NULL)
				return MXCAM_ERR_INVALID_DEVICE;
	
			return MXCAM_ERR_INVALID_PARAM;
		}

		r = libusb_open(devlist->dev, &devhandle);
		if (r < 0) {
			return MXCAM_ERR_INVALID_DEVICE;
		}
		cur_mxdev = devlist;
		return MXCAM_OK;
	}
	return MXCAM_ERR_DEVICE_NOT_FOUND;;
}

/**
* \ingroup library
* \brief mxcam_poll_one:
*	Check if a registered device is plugged and open it or open the first
* found if there are multiples registered devices connected. If no registered
* device is plugged yet wait until one is plugged
*
* \param devlist   :scanned device list

* \retval MXCAM_ERR_INVALID_DEVICE - Failed to open device
* \retval Negativevalue - upon libusb error
* \retval number of devices device found
*
*/
int mxcam_poll_one(struct mxcam_devlist *devlist)
{
	int ret;

	while(1) {
		ret = mxcam_open_by_devnum(1, devlist);
		if (ret == MXCAM_OK ||ret == MXCAM_ERR_INVALID_DEVICE ||ret < 0)
			break;
		sleep(1);
	};

	return ret;
}
#if 0
/**
* \ingroup library
* \brief mxcam_poll_new:
* Wait until a new registered device is plugged and open it
* \retval MXCAM_ERR_INVALID_DEVICE - Failed to open device
* \retval Negativevalue - upon libusb error
*/
int mxcam_poll_new(void)
{
	int ndev, found;
	struct mxcam_devlist *d, *dorig, *dnew;
	ndev = mxcam_scan(&dorig);

	/* Wait until a new device is connected */
	while (mxcam_scan(&dnew) <= ndev)
		sleep(1);

	/* A new device was connected; find which one and open it */
	while(dnew != NULL) {
		d = dorig;
		found = 0;
		while(d != NULL) {
			if (d->bus == dnew->bus && d->addr == dnew->addr) {
				found = 1;
				break;
			}
			d = d->next;
		}
		if (found == 0)
			return mxcam_open_by_busaddr(dnew->bus, dnew->addr);
		dnew = dnew->next;
	}

	return MXCAM_ERR_FAIL;
}
#endif
/**
* \ingroup library
* \brief mxcam_whoami:
* find connected camera core id
* \retval valid chip id 
*/
int mxcam_whoami(void)
{
	int chip_id = 0;

	if (is_max64380(devhandle))
		chip_id = 64380;
	else if (is_max64480(devhandle))
		chip_id = 64480;
	else if (is_max64180(devhandle))
		chip_id = 64180;

	return chip_id;
}

/**
* \ingroup library
* \brief mxcam_get_cmd_bitmap:
* get supported command bitmap from camera fw 
* \retval on success, the number of bytes actually transferred  
* \retval Negative value - upon libusb error 
*/
int mxcam_get_cmd_bitmap(char *buffer)
{
	int r;
	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ GET_CMD_BITMAP,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)buffer,
			/* wLength       */ 256,
			/* timeout*/   1000
			);
	if (r < 0) {
		MXPRINT("Failed REQ_GET_EEPROM_VALUE %d\n", r);
		return r;
	}		

	return r;
}

/**
* \ingroup library
* \brief mxcam_whoru:
* get cmera id & mode from camera fw/bootloader 
* \retval on success, first 4 bytes are chip id & next 4 bytes are camera mode  
* \retval Negative value - upon libusb error 
*/
int mxcam_whoru(char *buffer)
{
	int r = 0;
	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ CMD_WHO_R_U,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)buffer,
			/* wLength       */ 8,
			/* timeout*/  100
			);

	if (r < 0) {
		MXPRINT("Failed REQ_GET_EEPROM_VALUE %d\n", r);
		return r;
	}		

	return r;	
}

/**
* \ingroup library
* \brief mxcam_close:
* close libmxcam library
*/
void mxcam_close(void)
{
	if(devs != NULL && devs[0] != NULL)
			libusb_free_device_list(devs, 1);
	if (devhandle != NULL) {
		libusb_release_interface(devhandle, 0);
		libusb_close(devhandle);
	}
	if(dcontext != NULL)
		libusb_exit(dcontext);
	if(devlist_cache != NULL) {
		struct mxcam_devlist *d = devlist_cache;
		while(devlist_cache != NULL) {
			d = devlist_cache;
			devlist_cache = devlist_cache->next;
			free(d);
		}
	}
	devs=NULL;
	devhandle=NULL;
	dcontext=NULL;
	device_count = 0;
}

int mxcam_memtest(uint32_t ddr_size)
{
	int r;
	unsigned char data[4];

	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ MEMTEST,
			/* wValue        */ (uint16_t)ddr_size,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ data,
			/* wLength       */ 4,
			/* timeout*/  1000*30 //30sec timeout 
			);
	if (r < 0) {
		MXPRINT("Failed to start memtest %d\n", r);
		return r;
	}		

	return r;
}

int mxcam_get_memtest_result(uint32_t *value)
{
	int r;
	r = libusb_control_transfer(devhandle,
			/* bmRequestType */
			(LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			 LIBUSB_RECIPIENT_INTERFACE),
			/* bRequest      */ MEMTEST_RESULT,
			/* wValue        */ 0,
			/* MSB 4 bytes   */
			/* wIndex        */ 0,
			/* Data          */ (unsigned char *)value,
			/* wLength       */ 4,
			/* timeout*/  1000*20  //20sec timeout
			);
	if (r < 0) {
		MXPRINT("Failed to get memtest result %d\n", r);
		return r;
	}		

	return r;
}
