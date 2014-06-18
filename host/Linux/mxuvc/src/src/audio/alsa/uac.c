/*******************************************************************************
 *
 * The content of this file or document is CONFIDENTIAL and PROPRIETARY
 * to Maxim Integrated Products.  It is subject to the terms of a
 * License Agreement between Licensee and Maxim Integrated Products.
 * restricting among other things, the use, reproduction, distribution
 * and transfer.  Each of the embodiments, including this information and
 * any derivative work shall retain this copyright notice.
 *
 * Copyright (c) 2012 Maxim Integrated Products.
 * All rights reserved.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "common.h"
#include "mxuvc.h"
#include "uac.h"
#include "qbox.h"

/*
 *
 * Static variables
 *
 */

/* handles to ALSA device */
static struct audio_stream {
	char dev_name[20];
	snd_pcm_t *alsa_device_capture_handle;
	int current_audio_framesize;
	audio_format_t current_audio_format;
	snd_pcm_format_t current_alsa_format;
	mxuvc_audio_cb_t apps_audio_callback_func;
	void *apps_audio_callback_func_user_data;
	volatile int started;
	unsigned int current_audio_period;
} audio_stream[NUM_AUDIO_CHANNELS];

/* sound card number */
static int card_number = -1;

/* probably common for all channels */
static unsigned int current_audio_blocksize_bytes;

static snd_pcm_uframes_t current_audio_period_size;

/* alsa mixer params */
static unsigned int current_audio_sampling_rate;
static unsigned int current_audio_channel_count;
static unsigned int current_audio_duration_ms;
static snd_mixer_t* alsa_mixer_handle = NULL;
static snd_mixer_elem_t* mixer_elem = NULL;
static long alsa_mixer_max_volume;
static long alsa_mixer_min_volume;
/* Range is 0 to 100 for app to set */
static int current_volume;

static volatile int exit_AudioEngine = 0;
static volatile int AudioEngine_started = 0;
static volatile int AudioEngine_aborted = 0;
static pthread_t AudioEngine_thread;

/* ALSA device handler for capture device.*/
/* TODO try to make this local */
char *audio_device_name = NULL;

/* TODO why is this required? */
static unsigned short max_volume = 100;

const char* mixer_mic_table[] = {
    "Capture",
    "Mic"
};

static int get_alsa_card_info (char * dev_name, int *cardnum, int *devcount);

/* Invoke the user callback every AUDIO_DURATION_MS ms worth of data*/
#define AUDIO_DURATION_MS_DEFAULT       10
#define AUDIO_SAMPLING_RATE_DEFAULT     24000
#define CHANNEL_COUNT_DEFAULT           CHANNEL_COUNT_STEREO

#define AUDIO_RAW_AAC_SAMPLE_SIZE 	1024

int audio_channel_init(audio_channel_t ch, char *dev_name)
{
	struct audio_stream *astream = &audio_stream[ch];

	/* open half duplex read */
	strcpy(astream->dev_name, dev_name);

	/*
	 * TODO can't we detect this from camera?
	 * we might want to keep the defaults given by camera
	 */
	astream->current_audio_format = AUD_FORMAT_PCM_RAW;
	astream->current_alsa_format = SND_PCM_FORMAT_S16_LE;

	if(ch == AUD_CH1){
		astream->current_audio_period = AUDIO_PERIOD_DEFAULT;	
	}else{
		astream->current_audio_format = AUD_FORMAT_AAC_RAW;
		astream->current_audio_period = AUDIO_MUX_AAC_PERIOD_DEFAULT;
	}

	return 0;
}

void audio_channel_deinit(audio_channel_t ch)
{
	return;
}

/*
 *
 * mxuvc Audio APIs
 *
 */

/**
 ****************************************************************************
 * @brief Open/Init the Maxim camera's Mic interface using ALSA APIs
 *
 * This function will open the Maxim camera Mic interface,
 * initialise the device and allocate resources ready for processing camera's
 * Mic data.
 *
 * @param[in]  backend - This string represents the backend interface used.
 *	alsa => to use ALSA framework.
 *	libusb-uac => to use libusb framework
 * Note : the selection of alsa/libusb is compile time decision.
 *
 * @param[in]  options - This string consists of below paraters to be set
 *		device - The name of Maxim Camera.
 *		audio_sampling_rate - The sampling rate to open the device
 *		audio_channel_count - The number of channels to open(Mono/Stereo)
 *		audio_duration_ms   - The time period (in millisec) at which the audio
 *			data  will be read from audio device
 * Below is the format to be used to create the options string.
 *		"device = MAX64380;audio_sampling_rate = 16000;
 *				audio_channel_count = 2; audio_duration_ms = 20"
 *
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
int
mxuvc_audio_init (const char *backend, const char *options)
{
	RECORD("\"%s\", \"%s\"", backend, options);
	int one_ms_data_len;
	int ret = 0;
	char *str = NULL, *tmp_str = NULL, *opt, *value;
	char dev_name[20];
	int device_count, channel;

	TRACE("Initializing the audio\n");
	/* Check that the correct video backend was requested */
	if (strncmp (backend, "alsa", 4))
	{
		ERROR (-1, "The audio backend requested (%s) does not match "
				"the implemented one (libusb-uac)", backend);
	}

	/* Set init parameters to their default values */
	current_audio_sampling_rate = AUDIO_SAMPLING_RATE_DEFAULT;
	current_audio_channel_count = CHANNEL_COUNT_DEFAULT;
	current_audio_duration_ms   = AUDIO_DURATION_MS_DEFAULT;

	/* Copy the options string to a new buffer since next_opt() needs
	 * non const strings and options could be a const string
	 * */
	if (options != NULL)
	{
		str = (char *) malloc (strlen (options) + 1);
		/* hold the address in tmp; needed later to free it */
		tmp_str = str;
		strncpy (str, options, strlen (options));
		*(str + strlen (options)) = '\0';
	}

	/* Get backend option from the option string */
	ret = next_opt (str, &opt, &value);
	while (ret == 0)
	{
		if (strncmp (opt, "device", 6) == 0)
		{
			audio_device_name = malloc(strlen(value) + 1);
			strncpy(audio_device_name, value, strlen(value));
			audio_device_name[strlen(value)] = '\0';
		}
		else if (strncmp (opt, "audio_sampling_rate", 19) == 0)
		{
			current_audio_sampling_rate =
				(unsigned int) strtoul (value, NULL, 10);
		}

		else if (strncmp (opt, "audio_channel_count", 19) == 0)
		{
			current_audio_channel_count =
				(unsigned int) strtoul (value, NULL, 10);
		}
		else if (strncmp (opt, "audio_duration_ms", 17) == 0)
		{
			current_audio_duration_ms =
				(unsigned int) strtoul (value, NULL, 10);
		}
		ret = next_opt (NULL, &opt, &value);
	}

	/* Parsing done free the str */
	free (tmp_str);

	/* find sound card information */
	ret = get_alsa_card_info(audio_device_name, &card_number, &device_count);
	
	CHECK_ERROR(ret > 0, -1,
			"Unable to find soundcard '%s'\n", audio_device_name);
	CHECK_ERROR(card_number < 0 || device_count <= 0, -1,
			"Unable to find soundcard '%s'\n", audio_device_name);
		
	TRACE("Detected %s: card %d with %d device/s\n",audio_device_name, card_number,device_count);

	/* Display the values we are going to use */
	TRACE("Using device = %s\n", audio_device_name);
	TRACE("Using audio_sampling_rate = %i\n",  current_audio_sampling_rate);
	TRACE("Using audio_channel_count = %i\n",  current_audio_channel_count);
	TRACE("Using audio_duration_ms = %i\n",    current_audio_duration_ms);

	/* TODO check whether this has to be channel specific */
	if (find_one_ms_data_len (&one_ms_data_len, SND_PCM_FORMAT_S16_LE) != 0)
	{
		ERROR (-1, "[%d] find_one_ms_data_len() failed\n", __LINE__);
	}

	current_audio_blocksize_bytes =
		(one_ms_data_len * current_audio_duration_ms);

	/* initialize audio channels */
	/* TODO get number of devices in the card and initialize all */
	for(channel=AUD_CH1 ; channel<device_count ; channel++){
		sprintf(dev_name, "hw:%d,%d", card_number, channel);
		TRACE("device %d: %s\n",channel, dev_name);
		ret = audio_channel_init(channel, dev_name);
		CHECK_ERROR(ret < 0, -1, "Unable to initialize audio channel\n");
	}

	/* Init alsa mixer */
	if ( alsa_mixer_init(audio_device_name) < 0)
	{
		free_resources();
		ERROR( -1, "[%d] ERROR alsa_mixer_init failed\n", __LINE__);
	}

	/* open half duplex write */
	TRACE2("[%d] Sample rate = %i\n", __LINE__, current_audio_sampling_rate);
	TRACE2("[%d] Channels = %i\n", __LINE__, current_audio_channel_count);
	TRACE2("[%d] Data duration = %d ms\n", __LINE__,
			(unsigned int) current_audio_duration_ms);
	TRACE2("[%d] Period size = %d\n", __LINE__,
			(unsigned int) current_audio_period_size);
	TRACE2("[%d] Block size = %i\n", __LINE__, current_audio_blocksize_bytes);

	return 0;
}

/**
 ****************************************************************************
 * @brief Register the Application's Callback function with the Mxuvc.
 *
 * The Maxim camera must have been opened by calling mxuvc_audio_init()
 *
 * @param[in]   func -  Application's Callback function address.
 * @param[out]  user_data - Pointer to Application Provite data.
 * @return 0.
 *****************************************************************************/
int
mxuvc_audio_register_cb (audio_channel_t ch, mxuvc_audio_cb_t func, void *user_data)
{
	struct audio_stream *astream = &audio_stream[ch];

	RECORD("%p, %p",func, user_data);
	astream->apps_audio_callback_func = func;
	astream->apps_audio_callback_func_user_data = user_data;
	return 0;
}


/**
 ****************************************************************************
 * @brief This function starts the Audio capture.
 *
 * This function creates a thread and
 * @param[in] - None
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
static int _mxuvc_audio_start (audio_channel_t ch)
{
	int ret;
	int alsa_err;
	TRACE("Starting audio on channel %d\n", ch);
	TRACE2("[%d] Audio call back set; created thread to do periodic "
			"read. AudioEngine_started =%d\n", __LINE__,
			AudioEngine_started);
	
	struct audio_stream *astream = &audio_stream[ch];

	/* open the audio device */
	TRACE2("[%d] Opening capture device '%s'\n", __LINE__, astream->dev_name);
	alsa_err = snd_pcm_open(&astream->alsa_device_capture_handle, astream->dev_name,
			SND_PCM_STREAM_CAPTURE, 0);
	if (alsa_err < 0)
	{
		ERROR ( -1, "Error calling snd_pcm_open(CAPTURE) - ");
	}

	alsa_err = alsa_configure (astream);
	if (alsa_err < 0)
	{
		free_resources();
		ERROR (-1, "[%d] ERROR alsa_configure(read)\n", __LINE__);
	}

	/* reset all flags */
	exit_AudioEngine = 0;
	astream->started = 0;
	AudioEngine_aborted = 0;

	if(AudioEngine_started == 0){
		AudioEngine_started = 1;
		astream->started = 1;

		ret = pthread_create (&AudioEngine_thread, NULL, &AudioEngine, NULL);
		if (ret != 0)
		{
			ERROR(-1," Unable to start the audio. Failed to create the "
					"pthread: err = %d\n", ret);
		}
	}

	astream->started = 1;

	return 0;
}
int
mxuvc_audio_start (audio_channel_t ch)
{
	RECORD("");
	return _mxuvc_audio_start(ch);
}

/**
 ****************************************************************************
 * @brief Close the Maxim camera
 *
 * This function will close the Maxim camera.  The camera must have first been
 * opened by a call to #mxuvcOpen
 *
 * @param[in] - NONE
 * @return 0
 *****************************************************************************/
static int _mxuvc_audio_stop (audio_channel_t ch)
{
	int ret;

	TRACE("Stopping audio\n");

	CHECK_ERROR(AudioEngine_started == 0, -1, "Unable to stop the audio: "
			"audio hasn't been started yet.");

	struct audio_stream *astream = &audio_stream[ch];
	CHECK_ERROR(astream->started == 0, -1, "Unable to stop the audio: "
			"audio hasn't been started yet.");

	/* Just return if audio engine aborted due to alsa errors */
	if (AudioEngine_aborted ) {
		/* close alsa handle */
		ret = snd_pcm_close(astream->alsa_device_capture_handle);
		if (ret < 0) {
			ERROR_NORET("Unable to close alsa handle for %s\n",
					astream->dev_name);
		}
		return 0;
	}

	/* close audio device */
	exit_AudioEngine = 1;
	while( exit_AudioEngine )
	{
		sleep (1);
	}

	/* close alsa handle */
	ret = snd_pcm_close(astream->alsa_device_capture_handle);
	if (ret < 0) {
		ERROR_NORET("Unable to close alsa handle for %s\n",
				astream->dev_name);
	}

	/* reset all flags */
	exit_AudioEngine = 0;
	astream->started = 0;
	AudioEngine_started = 0;
	AudioEngine_aborted = 0;
	
	return 0;
}
int
mxuvc_audio_stop (audio_channel_t ch)
{
	RECORD("");
	return _mxuvc_audio_stop(ch);
}


/**
 ****************************************************************************
 * @brief Set the audio format  of the Maxim camera.
 *
 * @param[in] fmt - The audio format to be set.
 * Note: only format SND_PCM_FORMAT_S16_LE is supported now.
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
int
mxuvc_audio_set_format (audio_channel_t ch, audio_format_t fmt)
{
	int configure = 0;
	int alsa_err;
	struct audio_stream *astream = &audio_stream[ch];
	RECORD("%s", audformat2str(fmt));

	TRACE("Setting audio format to %s.\n", audformat2str(fmt));

	switch (fmt)
	{
		case AUD_FORMAT_AAC_RAW:
			astream->current_audio_format = AUD_FORMAT_AAC_RAW;
			astream->current_alsa_format = SND_PCM_FORMAT_S16_LE;
			//TBD 
			//if (astream->started)
			//	configure = 1;
			break;
		case AUD_FORMAT_PCM_RAW:
			astream->current_audio_format = AUD_FORMAT_PCM_RAW;
			astream->current_alsa_format = SND_PCM_FORMAT_S16_LE;
			if (astream->started)
				configure = 1;
			break;
		default:
			ERROR (-1, "Unable to set the audio format: unknown audio "
					"format %i", fmt);
	}

	if (configure)
	{
		/* configure the audio capture device */
		alsa_err = alsa_configure (astream);
		if (alsa_err < 0)
		{
			ERROR (-1, "[%d] ERROR alsa_configure(read)\n", __LINE__);
		}
	}

	return 0;
}

/**
 ****************************************************************************
 * @brief Set the sample rate of the Maxim camera.
 *
 * This function will update the param 'sampleRate' with the current sample rate being used
 *
 * @param[in] samplingFr - The current sample rate of the Maxim camera audio stream
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
int
mxuvc_audio_set_samplerate (audio_channel_t ch, int samplingFr)
{
	int alsa_err;
	int restart=0;
	struct audio_stream *astream = &audio_stream[ch];

	RECORD("%i", samplingFr);

	TRACE("Setting audio sample rate to %i.\n", samplingFr);

	current_audio_sampling_rate = samplingFr;

	if(astream->started) {
		alsa_err = _mxuvc_audio_stop(ch);
		CHECK_ERROR(alsa_err < 0, -1,
				"Could not set the audio sample rate to %i",
				samplingFr);
		restart = 1;

		/* configure the audio capture device */
		alsa_err = alsa_configure (astream);
		CHECK_ERROR (alsa_err < 0, -1,
				"[%d] ERROR alsa_configure failed\n", __LINE__);

		alsa_err = _mxuvc_audio_start(ch);
		CHECK_ERROR (alsa_err < 0, -1,
				"[%d] ERROR function mxuvc_audio_start failed \n",
				__LINE__);
	}

	if ( samplingFr != (int) current_audio_sampling_rate )
	{
			ERROR (-1, "[%d] ERROR Not able to set the requested sampling Rate \n", __LINE__);
	}

	return 0;
}

/* Get the current sample rate */
int mxuvc_audio_get_samplerate(audio_channel_t ch)
{
	RECORD("");
	return current_audio_sampling_rate;
}


/**
 ****************************************************************************
 * @brief This function to chcek the audio capture started and also to check if
 * it is still happening.
 *
 * @param[in]   NONE
 * @return 1 if audio capture already started.
 * @return 0 if audio capture is not started or halted due to some error.
 *****************************************************************************/
int
mxuvc_audio_alive ()
{
	RECORD("");
	return ( AudioEngine_started && (!AudioEngine_aborted)) ;
}

/**
 ****************************************************************************
 * @brief Close the Maxim camera
 *
 * This function will close the Maxim camera Mic interface and frees all allocated resources.
 * @return 0
 *****************************************************************************/
int
mxuvc_audio_deinit ()
{
	RECORD("");
	int channel;

	/* stop the AudioEngine thread, if it is not already. */
	if(AudioEngine_started)
	{
		for(channel=AUD_CH1; channel<NUM_AUDIO_CHANNELS; channel++){
			struct audio_stream *astream = &audio_stream[channel];
			if(astream->started){		
				_mxuvc_audio_stop(channel);
			}
			audio_channel_deinit(channel);
		}
	}

	free_resources();
	TRACE("The audio has been successfully uninitialized\n");
	return 0;
}

/**
 ****************************************************************************
 * @brief this function gets the current Mic Gain/Volume.
 *
 * @param - None
 * @return volume set (range 0 to 100)
 *****************************************************************************/
int mxuvc_audio_get_volume()
{
	RECORD("");
	TRACE2("[%d] Volume is set to %d ", __LINE__, current_volume);
	return  current_volume;
}

/**
 ****************************************************************************
 * @brief This function used to Mute/Unmute the Mic.
 *
 * @param[in] Mute - This variable will have value 0 or 1.
 *		     If  1 , Mic is muted
 *		     If  0 , Mic is Unmuted
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
int mxuvc_audio_set_mic_mute(int bMute)  /* mute=1, unmute=0 */
{
	RECORD("%i", bMute);
	TRACE("Setting mic mute to %i\n", bMute);

	if ((alsa_mixer_handle == NULL) || (mixer_elem == NULL))
	{
		TRACE("Error mixer device not opened\n");
		return -1;
	}

	snd_mixer_handle_events(alsa_mixer_handle);
	/* check if the device has a switch we can enable to mute
	 * the audio */
	if (snd_mixer_selem_has_capture_switch(mixer_elem) == 1)
	{
		int err;
		/* turn the switch off (0) i.e. disable audio and hence turn mute on */
		if(bMute == 1)
		{
			TRACE2("[%d] Muting the Mic", __LINE__);
			err = snd_mixer_selem_set_capture_switch_all(mixer_elem, 0);
		}
		else
		{
			TRACE2("[%d] UnMuting the Mic", __LINE__);
			err = snd_mixer_selem_set_capture_switch_all(mixer_elem, 1);
		}
		if (err < 0)
		{
			ERROR(-1, "Unable to mute/unmute the audio. "
				"Error calling snd_mixer_selem_set_capture_switch_all: %s\n",
				snd_strerror(err));
		}
	}
	else
	{
		ERROR(-1, "Unable to mute/unmute the audio. "
			"Card %d name '%s' has no mute capability\n",
			card_number, snd_mixer_selem_get_name(mixer_elem));
	}

	return 0; //SUCCESS;
}

/**
 ****************************************************************************
 * @brief API to increasse/decrese Volume/Gain of Mic.
 *
 * Sets the gain of the device.  Gain is specificed between 0 and 100.
 * where 0 is - Infinity dB and 100 is max gain (min=0, max=100)
 *
 * @param[in] volume - The volume to be set.
 * @return 0 when the function completes successfully, or -1 to indicates an error
 *****************************************************************************/
int mxuvc_audio_set_volume(int user_volume)
{
	RECORD("%i", user_volume);
	int err;
	int alsa_volume;

	TRACE("Setting audio volume to %i\n", user_volume);

	CHECK_ERROR((alsa_mixer_handle == NULL) || (mixer_elem == NULL) ,-1,
		"Unable to set the audio volume to %i. "
		"Error mixer device not opened\n", user_volume);

	/* check the device even has a volume control */
	CHECK_ERROR(snd_mixer_selem_has_capture_volume(mixer_elem) == 0 , -1,
		"Unable to set the audio volume to %i. Card %d name '%s' "
		"has no volume control\n", user_volume, card_number,
		snd_mixer_selem_get_name(mixer_elem));

	alsa_volume = ( ( (alsa_mixer_max_volume - alsa_mixer_min_volume) * user_volume ) / max_volume);
	TRACE2("[%d] Alsa volume = %lu (range %lu to %lu)\n", __LINE__,
			(long)alsa_volume,
			alsa_mixer_min_volume,
			alsa_mixer_max_volume);

	/* set the volume for all channels */
	err = snd_mixer_selem_set_capture_volume_all(mixer_elem, (long)alsa_volume);
	CHECK_ERROR(err < 0, -1,
		"Unable to set the audio volume to %i. "
		"Error calling snd_mixer_selem_set_capture_volume_all: %s\n",
		user_volume, snd_strerror(err));

	current_volume = user_volume;

	return 0;
}

/* static functions which are not exposed to Application */

/*
 * This function is called first before starting the capture.
 * In this function we trigger alsa to start capture of Mic data.
 */
static int
alsa_start (snd_pcm_t * p_alsa_handle)
{
	snd_pcm_state_t state;
	int err;

	/* Check that the passed handle is legal */
	CHECK_ERROR((p_alsa_handle == (snd_pcm_t *) - 1) || (p_alsa_handle == NULL), -1,
			"[%d] Invalid handle %p\n", __LINE__, p_alsa_handle);

	state = snd_pcm_state (p_alsa_handle);
	TRACE2("[%d] state:%d\n", __LINE__,state);
	switch (state)
	{
		case SND_PCM_STATE_OPEN:
			break;
		case SND_PCM_STATE_SETUP:
			break;
		case SND_PCM_STATE_DRAINING:
			break;
		case SND_PCM_STATE_PAUSED:
			break;
		case SND_PCM_STATE_SUSPENDED:
			break;
#if (SND_LIB_VERSION > 0x10000)
		case SND_PCM_STATE_DISCONNECTED:
			break;
#endif
		case SND_PCM_STATE_XRUN:
			do
			{
				TRACE2("[%d] Trying snd_pcm_prepare()...\n", __LINE__);
				err = snd_pcm_prepare (p_alsa_handle);
			}
			while (err < 0);
			err = snd_pcm_start (p_alsa_handle);
			CHECK_ERROR(err < 0, -1,
				"\n[%d] Cannot start PCM (%s)\n", __LINE__,
				snd_strerror (err));
			break;
		case SND_PCM_STATE_PREPARED:
			err = snd_pcm_start (p_alsa_handle);
			CHECK_ERROR(err < 0, -1,
				"\n[%d] Cannot start PCM (%s)\n", __LINE__,
				snd_strerror (err));
			break;
		case SND_PCM_STATE_RUNNING:
			break;
		default:
			break;
	}

	return 0;
}
/* might require in future so kept commented */
/*int proces_audio_qbox_frames(struct audio_stream *astream,
				unsigned char *buffer,
				uint32_t buf_length)
{
	uint32_t box_size;
	audio_params_t aparam;
	int ts = 0;
	unsigned char *buf = buffer;
	uint32_t length = buf_length;
	int offset = 0;

	//check if its qbox ?
	while(1){
		if(length < 28){
			TRACE("ERR: wrong qbox packet\n");
			break;
		}

		box_size = get_qbox_frame_size(buf+offset);
		if(box_size == 0){
			TRACE("ERR: wrong qbox packet %d\n",length);
			break;
		}else{			
			if(box_size > length)
				break;

			length = length - box_size;
		}

		if(audio_param_parser(&aparam, (buf+offset), box_size) == 0){
			astream->apps_audio_callback_func (aparam.dataptr,(unsigned)aparam.framesize,
					(int) astream->current_audio_format, ts, 
					astream->apps_audio_callback_func_user_data, 
					&aparam);
		}else{
			TRACE("ERR: audio_param_parser failed\n");
			return 1;
		}
		offset = offset + box_size;
	}

	return length;
}
*/
/*
 * This is executed in separate thread.
 * This function is executed in while loop till the audio_stop is called by the
 * App.
 * The sole functionality of this function is to wait for data, then capture the data
 * from Mic and then call the App registered callback function.
 */
static void *
AudioEngine (void *ptr)
{
	int tmp;
	struct timeval tv;
	uint64_t ts64 = 0, tsec = 0, tusec = 0;
	static uint64_t ts = 0;
	unsigned int bytes_to_read;
	int err;
	int read_len = 0;
	unsigned char loop_buff[current_audio_blocksize_bytes];
	int read_fail_count = 0;
	int channel;
	int hdr_length = 0;	
	int qbox_frame_size = 0;
	audio_params_t aparam;
	int channel_id;

	/* FIXME: hack
	 * current audio engine does not support reading from multiple sources
	 * add a mechanism to wait on multiple devices
	 */
	for(channel=AUD_CH1; channel<NUM_AUDIO_CHANNELS ; channel++){
		struct audio_stream *astream = &audio_stream[channel];
		if(astream->started)
			break;
	}

	struct audio_stream *astream = &audio_stream[channel];

	err = alsa_start (astream->alsa_device_capture_handle);

#if MXUVC_AUDIO_ALSA_ENABLE_PTHREAD_PRIORITY
	struct sched_param param;
	int policy = 0;

	policy = MXUVC_AUDIO_ALSA_SET_PTHREAD_SCHED_POLICY;
        param.sched_priority = MXUVC_AUDIO_ALSA_SET_PTHREAD_SCHED_PRIORITY;

        err = pthread_setschedparam(AudioEngine_thread, policy, &param);
	if ( err != 0)
	{
		WARNING("[%d] pthread_setschedparam failed  err_code=%d \n",
				__LINE__, err);
	}

        err = pthread_getschedparam(AudioEngine_thread, &policy, &param);
	if ( err != 0)
	{
		WARNING("[%d] pthread_getschedparam failed  err_code=%d \n",
				__LINE__, err);
	}

	TRACE2("[%d] AudioEngine thread set  priority=%d  policy=%d \n",
			__LINE__,param.sched_priority,policy);

#endif

	if (!err)
	{
		while (!exit_AudioEngine){
			if(astream->current_audio_format == AUD_FORMAT_AAC_RAW)
			{
				snd_pcm_wait (astream->alsa_device_capture_handle, current_audio_duration_ms);

				hdr_length = get_qbox_hdr_size();
				read_len = alsa_read (astream, loop_buff, hdr_length);
				if(read_len == hdr_length)
				{
					video_format_t fmt;
					uint8_t *data_buf;
					if(qbox_parse_header(loop_buff, &channel_id, 
								&fmt, &data_buf, 
								(uint32_t *)&qbox_frame_size, &ts)) {
						TRACE("Wrong mux audio format\n");
						continue;
					}
				}else
					continue;

				if(qbox_frame_size)
				{
					read_len = alsa_read(astream, loop_buff+hdr_length, qbox_frame_size);

					if(qbox_frame_size != read_len)
						continue;

					if(audio_param_parser(&aparam, loop_buff, qbox_frame_size+hdr_length) == 0)
					{
						astream->apps_audio_callback_func (aparam.dataptr,(unsigned)aparam.framesize,
							(int) astream->current_audio_format, ts, 
							astream->apps_audio_callback_func_user_data, 
							&aparam);
					}
				}
			}else{ 
				TRACE2("[%d] Audio Loop started\n",__LINE__);
				/* Wait for audio data */
				snd_pcm_wait (astream->alsa_device_capture_handle, current_audio_duration_ms * 2);
				/* check the data in audio input and output */
				tmp = alsa_read_ready (astream, &bytes_to_read);
				if (tmp < 0)
				{
					/* Unsuccessful, report error */
					ERROR_NORET("[%d] alsa_read_ready() failed\n", __LINE__);
				}

				bytes_to_read = current_audio_blocksize_bytes;
				/* Read all frames in the input buffer */
				while (bytes_to_read >= current_audio_blocksize_bytes)
				{
					if (bytes_to_read >= current_audio_blocksize_bytes)
					{
						/* read one block */
						read_len =
							alsa_read (astream, loop_buff,
									current_audio_blocksize_bytes);
						if ( read_len > 0 )
						{
							/* Reset retry count */
							if (read_fail_count)
								read_fail_count = 0;

							/* Create a timestamp here */
							gettimeofday (&tv, NULL);
							tsec = (uint64_t) tv.tv_sec;
							tusec = (uint64_t) tv.tv_usec;
							ts64 = ( ( (tsec * 1000000 + tusec) * 9) / 100 );
							ts = (uint64_t) (ts64 & 0xffffffff);

							/*Call the App's callback and pass the captured audio data */
							astream->apps_audio_callback_func (loop_buff, (unsigned) read_len,
									(int) astream->current_audio_format, ts,
									astream->apps_audio_callback_func_user_data, &aparam);
						}
						else if (read_len == -2)
						{
							ERROR_NORET("[%d] Capture device %s removed; "
										"Stopping the AudioEngine\n",
										__LINE__, audio_device_name);
								AudioEngine_aborted = 1;
								exit_AudioEngine = 1;
								break;
						}
						else
						{
							++read_fail_count;
							if( read_fail_count >= ALSA_READ_MAX_RETRY_COUNT )
							{
								ERROR_NORET("[%d] alsa_read() failed continuously "
										"%d times; Stopping the AudioEngine\n",
										__LINE__, ALSA_READ_MAX_RETRY_COUNT);
								AudioEngine_aborted = 1;
								exit_AudioEngine = 1;
								break;
							}
						}
					}
					else
					{
						WARNING (" --- --- --- too little data to read, "
								"adding zeros --- --- ---\n");
						memset (loop_buff, 0, current_audio_blocksize_bytes);
					}

					bytes_to_read -= (unsigned ) read_len;
				}
			}
		}
	}
	else
	{
		ERROR_NORET ("[%d]  alsa_start() failed\n", __LINE__);
	}

	exit_AudioEngine = 0;
	TRACE2("[%d] Audio Loop Exit\n",__LINE__);
	return NULL;
}

/*
 * This function is handy in calculating the number of bytes need to be read
 * per millisecond calculated based on sample rate, number of channels and
 * format.
 * Note: currently this function only supports format SND_PCM_FORMAT_S16_LE
 */
static int
find_one_ms_data_len (int *one_ms_data_len, snd_pcm_format_t current_alsa_format)
{
	/* Determine a few things about the audio buffers */
	switch (current_alsa_format)
	{
		case SND_PCM_FORMAT_MU_LAW:
		case SND_PCM_FORMAT_A_LAW:
			TRACE2("[%d] SND_PCM_FORMAT_MU_LAW SND_PCM_FORMAT_A_LAW\n",
					__LINE__);

			*one_ms_data_len =
				(current_audio_sampling_rate / 1000) * sizeof (char) *
				current_audio_channel_count;
			break;
		case SND_PCM_FORMAT_S16_LE:
			TRACE2("[%d] SND_PCM_FORMAT_S16_LE\n", __LINE__);
			*one_ms_data_len =
				(current_audio_sampling_rate / 1000) * sizeof (short) *
				current_audio_channel_count;
			break;
		default:
			ERROR(-1, "[%d] illegal audio format\n", __LINE__);
			break;
	}

	TRACE2("[%d] *one_ms_data_len = %i\n", __LINE__, *one_ms_data_len);
	return 0;
}

/*
 * This function calculated the alsa framesize based on current format and
 * channel count.
 */
static int
alsa_calculate_framesize (audio_format_t fmt, int channel_count)
{
	int framesize = 0;

	switch (fmt)
	{
		case AUD_FORMAT_AAC_RAW:
			//framesize = (sizeof (char) * channel_count);
			framesize = (sizeof (short) * channel_count);
			break;
		case AUD_FORMAT_PCM_RAW:
			framesize = (sizeof (short) * channel_count);
			break;
		default:
			ERROR (-1, "Unable to set the audio format: unknown audio "
					"format %i", fmt);
	}
	return framesize;
}


/*
 * This is main main function which does the configuration of opened alsa
 * interface using the current set parameters,
 * Format, sample rate, channel count, sample duration, periods and
 * period_size.
 */
static int
alsa_configure (struct audio_stream *astream)
{

	int alsa_err;
	snd_pcm_hw_params_t *p_hw_params = NULL;
	snd_pcm_uframes_t tmp;
	unsigned int tmp_uint;
	snd_pcm_t *p_alsa_handle = astream->alsa_device_capture_handle;

	t_device_alsa_audio_use_periods_e alsa_use_periods =
		DEFAULT_DEVICE_ALSA_AUDIO_PERIODS;

	TRACE2("[%s] Entered, handle %p\n", __FUNCTION__, p_alsa_handle);

	/* Check that the passed handle is legal */
	if ((p_alsa_handle == (snd_pcm_t *) - 1) || (p_alsa_handle == NULL))
	{
		ERROR (-1, "Invalid handle %p\n", p_alsa_handle);
	}

	/* Allocate space for hardware parameters */
	alsa_err = snd_pcm_hw_params_malloc (&p_hw_params);
	if (alsa_err < 0)
	{
		ERROR( -1, "[%d]  Cannot allocate hardware parameter structure: (%s)\n", __LINE__, snd_strerror (alsa_err));
	}

	/* This function loads current settings */
	alsa_err = snd_pcm_hw_params_any (p_alsa_handle, p_hw_params);
	if (alsa_err < 0)
	{
		ERROR_NORET ("[%d]  Cannot initialize hardware parameter structure: (%s)\n", __LINE__, snd_strerror (alsa_err));
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}

	/*
	* ALSA offers choice of interleaved samples (that is left/right for stereo)
	* or 'blocks' of channel data. Interleaved is what we are used to from
	* the OSS way of doing things
	*/
	if ((alsa_err = snd_pcm_hw_params_set_access (p_alsa_handle, p_hw_params,
					SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		ERROR_NORET ("[%d] Cannot set access type (%s)\n", __LINE__, snd_strerror (alsa_err));
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}

	/*
	 * Set audio format .
	*/
	alsa_err =
		snd_pcm_hw_params_set_format (p_alsa_handle, p_hw_params,
				astream->current_alsa_format);
	if (alsa_err < 0)
	{
		ERROR_NORET ("[%d] Failed to set audio format: %d\n", __LINE__,
				astream->current_alsa_format);
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}

	/* Set sample rate */
	tmp_uint = current_audio_sampling_rate;
#ifdef ALSA_PCM_NEW_HW_PARAMS_API
	if ((alsa_err = snd_pcm_hw_params_set_rate_near (p_alsa_handle, p_hw_params,
					&current_audio_sampling_rate,
					0)) < 0)
	{
		ERROR_NORET ("[%d]  Cannot set sample rate (%s)\n", __LINE__,
				snd_strerror (alsa_err));
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}
	else
	{
		TRACE2("[%d] Sample rate set to %d Hz\n", __LINE__,
				current_audio_sampling_rate);
	}
#else
	alsa_err = snd_pcm_hw_params_set_rate_near (p_alsa_handle, p_hw_params,
			current_audio_sampling_rate, 0);
	if (alsa_err < 0)
	{
		ERROR_NORET ("[%d] Cannot set sample rate (%s)\n", __LINE__,
				snd_strerror (alsa_err));
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}
#endif
	if (tmp_uint != current_audio_sampling_rate)
	{

		ERROR_NORET ("[%d] Not able to set requested sample rate =%u instead Sample Rate set to= %u \n",
				__LINE__, tmp_uint, current_audio_sampling_rate);
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}

	/* get current supported channel count(mono/stereo) */
	alsa_err = snd_pcm_hw_params_get_channels(p_hw_params, &current_audio_channel_count);
	if (alsa_err < 0)
	{
		ERROR_NORET ("[%d]  Cannot get channel count (%s)\n", __LINE__,
				snd_strerror (alsa_err));
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;	
	}
	printf("Audio channel count %d\n",current_audio_channel_count);
	alsa_err =
		snd_pcm_hw_params_set_channels_near (p_alsa_handle, p_hw_params,
				&current_audio_channel_count);
	if (alsa_err < 0)
	{
		ERROR_NORET ("[%d]  Cannot set channel count (%s)\n", __LINE__,
				snd_strerror (alsa_err));
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}
	else
	{
		TRACE2("[%d] Channels set to %u\n", __LINE__,
				current_audio_channel_count);
	}
	/* calculate and set current framesize based on samplerate,format and
	 * channel count
	*/
	astream->current_audio_framesize = alsa_calculate_framesize (astream->current_audio_format, current_audio_channel_count);
	if (alsa_use_periods == DEVICE_ALSA_AUDIO_USE_PERIODS)
	{
		/*
		 * Calculate the period size. Set a period as
		 * current_audio_duration_ms of audio data for the given sample rate.
		 *  This is in 'frames' - there is no need to take bits per sample or
		 *  number of channels into account, ALSA 'knows' how to adjust for
		 *  this -:).
		 */
		switch (current_audio_sampling_rate)
		{
			case SAMPLE_RATE_48KHZ:
				current_audio_period_size = (48 * current_audio_duration_ms);
				break;
			case SAMPLE_RATE_441KHZ:
				current_audio_period_size =
					((441 * current_audio_duration_ms) / 10);
				break;
			case SAMPLE_RATE_32KHZ:
				current_audio_period_size = (32 * current_audio_duration_ms);
				break;
			case SAMPLE_RATE_24KHZ:
				current_audio_period_size = (24 * current_audio_duration_ms);
				break;
			case SAMPLE_RATE_16KHZ:
				current_audio_period_size = (16 * current_audio_duration_ms);
				break;
			case SAMPLE_RATE_8KHZ:
				current_audio_period_size = (8 * current_audio_duration_ms);
				break;
			default:
				ERROR_NORET ("[%d] sample rate not supported (%u)\n", __LINE__,
						current_audio_sampling_rate);
				/* Free the hardware params then return error code */
				snd_pcm_hw_params_free (p_hw_params);
				return -1;
				break;
		}

		/* AAC mode audio duration and period size calculation */
		if(astream->current_audio_format == AUD_FORMAT_AAC_RAW){
			current_audio_duration_ms = 
				(AUDIO_RAW_AAC_SAMPLE_SIZE*1000)/current_audio_sampling_rate;
			current_audio_period_size = AUDIO_RAW_AAC_SAMPLE_SIZE;
		}

		//need to finetune it TBD	
		tmp = current_audio_period_size=128; //Tuned for lowest possible sam freq(8khz) 
#ifdef ALSA_PCM_NEW_HW_PARAMS_API
		alsa_err =
			snd_pcm_hw_params_set_period_size_near (p_alsa_handle, p_hw_params,
					&current_audio_period_size,
					0);
		if (alsa_err < 0)
		{
			ERROR_NORET ("[%d] Error setting period size\n", __LINE__);
			/* Free the hardware params then return error code */
			snd_pcm_hw_params_free (p_hw_params);
			return -1;
		}
#else
		alsa_err =
			snd_pcm_hw_params_set_period_size_near (p_alsa_handle, p_hw_params,
					current_audio_period_size, 0);
		if (alsa_err < 0)
		{
			ERROR_NORET ("[%d] Error setting period size\n", __LINE__);
			/* Free the hardware params then return error code */
			snd_pcm_hw_params_free (p_hw_params);
			return -1;
		}
#endif
		if (tmp != current_audio_period_size)
		{
			WARNING
				("[%d] requested period size not given (%d -> %d)\n",
				 __LINE__, (int) tmp, (int) current_audio_period_size);
		}
		else
		{
			TRACE2("[%d] period_size set to %u\n", __LINE__,
					(unsigned int) current_audio_period_size);
		}
		
		if(astream->current_audio_format != AUD_FORMAT_AAC_RAW)
		{
			/*
			 * calculate the alsa_period based on audio_duration as sent from
			 * application.
			 */
			tmp_uint = astream->current_audio_period;
#ifdef ALSA_PCM_NEW_HW_PARAMS_API
			alsa_err =
				snd_pcm_hw_params_set_periods_near (p_alsa_handle, p_hw_params,
						&tmp_uint, 0);
			if (alsa_err < 0)
			{
				ERROR_NORET ("[%d] ERROR: Error setting periods.\n", __LINE__);
				/* Free the hardware params then return error code */
				snd_pcm_hw_params_free (p_hw_params);
				return -1;
			}
#else
			alsa_err =
				snd_pcm_hw_params_set_periods_near (p_alsa_handle, p_hw_params,
						tmp_uint, 0);
			if (alsa_err < 0)
			{
				ERROR_NORET ("[%d] Error setting periods.\n", __LINE__);
				/* Free the hardware params then return error code */
				snd_pcm_hw_params_free (p_hw_params);
				return -1;
			}
#endif
			if (tmp_uint != astream->current_audio_period)
			{
				WARNING
					("[%d]  requested number of periods not given (%u -> %u)\n",
					 __LINE__, astream->current_audio_period, tmp_uint);
			}
			else
			{
				TRACE2("[%d] Number of periods set to %u\n", __LINE__,
						astream->current_audio_period);
			}
		}
	}
	else				/* must be DEVICE_ALSA_AUDIO_NOT_USE_PERIODS */
	{
		/*
		 * Using parameters to size the buffer based on how many bytes there is in
		 * a period multiplied by the number of periods. There may be a better
		 * way of doing this....
		 */
		alsa_err =
			snd_pcm_hw_params_set_buffer_size (p_alsa_handle, p_hw_params,
					current_audio_blocksize_bytes *
					AUDIO_PERIOD_DEFAULT);
		if (alsa_err < 0)
		{
			ERROR_NORET ("\n[%d] Error setting buffersize.\n", __LINE__);
			/* Free the hardware params then return error code */
			snd_pcm_hw_params_free (p_hw_params);
			return -1;
		}
	}

	/*
	 * Make it so! This call actually sets the parameters
	*/
	alsa_err = snd_pcm_hw_params (p_alsa_handle, p_hw_params);
	if (alsa_err < 0)
	{
		ERROR_NORET ("\n[%d]  Cannot set parameters (%s)\n", __LINE__,
				snd_strerror (alsa_err));
		/* Free the hardware params then return error code */
		snd_pcm_hw_params_free (p_hw_params);
		return -1;
	}

	/* Free hardware config structure */
	snd_pcm_hw_params_free (p_hw_params);
	TRACE2("[%s] Exit\n", __FUNCTION__);
	return 0;
}

/*
 * This function pokes the alsa interface to find how many bytes are in alsa
 * buffer to be read.
 * this function needs to be called to decide whether to call alsa_read or not.
 */
static int
alsa_read_ready (struct audio_stream *astream, unsigned int *pbytes_to_read)
{
	  int result_code = 0;
	  unsigned int data_available;
	  int frames_to_read;
	  snd_pcm_t * p_alsa_handle = astream->alsa_device_capture_handle;

	  /* Check that the passed handle is legal */
	  if ((p_alsa_handle == (snd_pcm_t *) - 1) || (p_alsa_handle == NULL))
	  {
		  ERROR (-1, "[%d] ERROR: Invalid handle %p\n", __LINE__, p_alsa_handle);
	  }

	  /* Get number of frames available for read */
	  frames_to_read = snd_pcm_avail_update (p_alsa_handle);

	  /*
	   * Negative value indicates an error. Usually this is a 'broken pipe' error
	   * which indicates an overrun of input. If we detect this then
	   * we need to call 'snd_pcm_prepare' to clear the error
	   */
	  if (frames_to_read < 0)
	  {
		  WARNING ("[%d] %s\n", __LINE__, snd_strerror (frames_to_read));
		  if (frames_to_read == -EPIPE)
		  {
			  WARNING ("[%d] rda: orun\n", __LINE__);
			  /* Kick input to clear error */
			  snd_pcm_prepare (p_alsa_handle);
		  }
		  frames_to_read = 0;
		  result_code = -1;
	  }

	  /* Update number of bytes to read back to caller */
	  data_available = frames_to_read * astream->current_audio_framesize;
	  /* Return the number of bytes available to read even if there isn't enough for a complete frame */
	  *pbytes_to_read = data_available;

	  return result_code;
}

/*
 * This function does interleaved read of the requested bytes from alsa buffer.
 *
 */
static int
alsa_read (struct audio_stream *astream, void *p_data_buffer, int data_length)
{
	int result_code = 0;
	int frames_read = 0;
	snd_pcm_t * p_alsa_handle = astream->alsa_device_capture_handle;

	/* Check that the passed handle is legal */
	if ((p_alsa_handle == (snd_pcm_t *) - 1) || (p_alsa_handle == NULL))
	{
		ERROR (-1, "[%d] ERROR: Invalid handle %p\n", __LINE__, p_alsa_handle);
	}

	/* Check the passed buffer to see if it is what is needed */
	if (p_data_buffer == NULL)
	{
		ERROR (-1, "[%d] ERROR: bad data buffer: input data address %p\n",
				__LINE__, p_data_buffer);
	}

	/* Now perform the ALSA read
	* ALSA works in 'frames' so we ask for the
	* 'number of bytes requested'/framesize. We then have to adjust the
	* number of frames actually read back to bytes
	*/

	frames_read =
		snd_pcm_readi (p_alsa_handle, p_data_buffer,
				(data_length / astream->current_audio_framesize));
	if (frames_read < 0)
	{
		WARNING ("[%d] Read error: %s\n", __LINE__,
				snd_strerror (frames_read));

		/* If we get a 'broken pipe' this is an overrun */
		/* Kick input to clear error */
		if (frames_read == -EPIPE)
			snd_pcm_prepare (p_alsa_handle);

		if (frames_read == -ENODEV)
			result_code = -2;
		else
			/* default error code */
			result_code = -1;
	}
	else
	{
		/* Read was OK so record the size (in bytes) of data read */
		return (frames_read * astream->current_audio_framesize);
	}

	return result_code;
}

#if 0
/*
 * This function is used to get the full Alsa device name
 * from the user provided name
 */
static int
get_alsa_device_name (char **name, char * dev_name)
{
	static char getting=0;
	static void **hints, **n;
	int ret_val = -1;

	if (getting == 0)
	{
		if (snd_device_name_hint (-1, "pcm", &hints) < 0)
		{
			ERROR(-1, "[%d] Getting alsa device name failed", __LINE__);
		}
		n = hints;
	}

	while(*n != NULL)
	{
		*name = snd_device_name_get_hint (*n, "NAME");
		n++;
		if (strstr(*name,dev_name))
		{
			if (strstr(*name, "front"))
			{
				ret_val = 0;
				break;
			}
		}
	}

	snd_device_name_free_hint (hints);
	return ret_val;
}
#endif

/*
 * This function inits the alsa mixer interface for the already opened alsa
 * device. the handler returned from this function is used for
 * set_volume , mute and Unmute of the Mic.
 *
 */
static int alsa_mixer_init(char* device_name)
{
	if (alsa_mixer_handle == NULL)
	{
		int err;
		/* open an handle to the mixer interface of alsa */
		err = snd_mixer_open(&alsa_mixer_handle, 0);
		CHECK_ERROR(err < 0, -1,
				"Error calling snd_mixer_open: %s\n", snd_strerror(err));

		card_number = snd_card_get_index(device_name);
		CHECK_ERROR(card_number < 0, -1,
				"Unable to get the card number for device '%s'.",
				device_name);

		char mixer_device[64];
		sprintf(mixer_device, "hw:%d",card_number);

		/* attach the mixer interface to the audio device we are in interested in */
		err = snd_mixer_attach(alsa_mixer_handle, mixer_device);
		if (err < 0)
		{
			ERROR_NORET("Error calling snd_mixer_attach for device %s: %s\n", mixer_device, snd_strerror(err));
			return -1; //ERROR_SYSTEM_CALL
		}

		TRACE2("[%d] Attached mixer to %s\n", __LINE__, mixer_device);

		/* register the mixer simple element (selem) interface */
		err = snd_mixer_selem_register(alsa_mixer_handle, NULL, NULL);
		if (err < 0)
		{
			ERROR_NORET("Error calling snd_mixer_selem_register: %s\n", snd_strerror(err));
			return -1; //ERROR_SYSTEM_CALL
		}


		/* load the mixer elements */
		err = snd_mixer_load(alsa_mixer_handle);
		if (err < 0)
		{
			ERROR_NORET("Error calling snd_mixer_load: %s\n", snd_strerror(err));
			return -1;//ERROR_SYSTEM_CALL
		}

		/* find the selem handle, this allows use to use the
		   mixer simple element interface */
		snd_mixer_selem_id_t *selemId = (snd_mixer_selem_id_t *) alloca(snd_mixer_selem_id_sizeof());
		memset(selemId, 0, snd_mixer_selem_id_sizeof());

		const char** ptable;
		unsigned int ptable_len;
		ptable = mixer_mic_table;
		ptable_len = sizeof(mixer_mic_table)/sizeof(mixer_mic_table[0]);
		char selem_name[64];
		int found_selem = 0;
		snd_mixer_elem_t *elem;
		unsigned int i;

		for(i=0; i<ptable_len; i++)
		{
			/* look through all the elements looking for one we can use */
			for (elem = snd_mixer_first_elem(alsa_mixer_handle); elem; elem = snd_mixer_elem_next(elem))
			{
				snd_mixer_selem_get_id(elem, selemId);
				strcpy(selem_name, snd_mixer_selem_id_get_name(selemId));
				/* check if the element is active */
				if (snd_mixer_selem_is_active(elem) == 0)
				{
					TRACE2("[%d] Ignoring inactive element '%s'\n", __LINE__, selem_name);
					continue;
				}
				if (strcmp(selem_name, ptable[i]) == 0)
				{
					TRACE2("[%d] Using element '%s'\n",  __LINE__, selem_name);
					mixer_elem = elem;
					found_selem = 1;
					break;
				}
			}
			if (found_selem == 1)
			{
				/* we found a match so continue */
				break;
			}
		}

		if (found_selem == 0)
		{
			ERROR_NORET ("Error failed to find a mixer element to control device = %s\n",mixer_device);
			snd_mixer_close(alsa_mixer_handle);
			alsa_mixer_handle = NULL;
			return -1; //ERROR_SYSTEM_CALL
		}

		/* the gain in the api is in the range 0 to 100.  Convert the gain
		 * from the range in the api to the range the sound card expects
		 * */
		err = snd_mixer_selem_get_capture_volume_range(mixer_elem, &alsa_mixer_min_volume, &alsa_mixer_max_volume);
		if (err < 0)
		{
			ERROR(-1, "Error calling snd_mixer_selem_get_capture_volume_range: %s\n",snd_strerror(err));
		}
	}

	return 0; //SUCCESS;
}

/*
 * This function frees all resources allocated.
 * */

static void free_resources()
{
	if (alsa_mixer_handle)
	{
		snd_mixer_close(alsa_mixer_handle);
		alsa_mixer_handle = NULL;
		mixer_elem = NULL;
	}

	if(audio_device_name)
	{
		free(audio_device_name);
		audio_device_name = NULL;
	}
}

/*
 * This function is used to get the full Alsa device informations
 * from the user provided name
 */
static int
get_alsa_card_info (char * dev_name, int *cardnum, int *devcount)
{
	snd_ctl_t *handle;
	int card, err, dev, count = 0;
	int ret = 0;

	card = -1;
	if (snd_card_next(&card) < 0) {
		TRACE("no soundcards found...\n");
		return 1;
	}
	card = snd_card_get_index(dev_name);

	if(card>=0){
		char name[32];
		sprintf(name,"hw:%d",card);

		if((err = snd_ctl_open(&handle, name, 0)) < 0){
			TRACE("error snd_ctl_open\n");
			ret = 1;
		}	
		dev = -1;
		while (1) {
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				TRACE("err: snd_ctl_pcm_next_device\n");
			if (dev < 0){
				break;
			}

			count++;
		}
		snd_ctl_close(handle);
	}	
	*cardnum = card;	
	*devcount = count;

	return ret;	
}
