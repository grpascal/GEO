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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>


/*
 *
 * Type definitions
 *
 */

/* Instructs the function to use periods */
typedef enum
{
    DEVICE_ALSA_AUDIO_DONT_USE_PERIODS = 0,
    DEVICE_ALSA_AUDIO_USE_PERIODS
} t_device_alsa_audio_use_periods_e;


/*
 *
 * Constant defines
 *
 */
/* The number of periods to use in ALSA */
#define ALSA_PERIODS 16

#if (SND_LIB_VERSION > 0x10000)
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#endif

#define TICK_RATE_MS                    10
#define LIVE_UPDATE_RATE 500 /* 500 ms */

// Uncomment this for 'per tick' information about data in buffer
// Note: Don't enable this if using via serial port.
//#define VERBOSE_OUTPUT


//#define DEFAULT_BLOCK_SIZE              (160)
/* 8 KHz Stereo */
//#define DEFAULT_BLOCK_SIZE              (320*2)
/* 16 KHz */
#define DEFAULT_BLOCK_SIZE              (640)
/* 48 KHz */
//#define DEFAULT_BLOCK_SIZE              (1920)
/* 44.1 KHz */
//#define DEFAULT_BLOCK_SIZE              (1764)

// Set this for drivers that never report all the fragments as available for Tx.
// Underunning the Tx will get stuck if the driver does this as it never sees 100% of Tx
// blocks available. Set to '0' or comment out if not needed.
//#define TX_RESERVE_BLOCKS               2


/* Set whether to use periods or not*/
#define DEFAULT_DEVICE_ALSA_AUDIO_PERIODS  (DEVICE_ALSA_AUDIO_USE_PERIODS)

/*
 * Set audio format. Set framesize based on format.
 */
/* 16 bit pcm */
#define DEFAULT_AUDIO_FORMAT            SND_PCM_FORMAT_S16_LE
#define DEFAULT_AUDIO_FRAMESIZE         (2)
/* g711 u-law */
//#define DEFAULT_AUDIO_FORMAT          SND_PCM_FORMAT_MU_LAW
//#define DEFAULT_AUDIO_FRAMESIZE       (1)
/* g711 a-law */
//#define DEFAULT_AUDIO_FORMAT          SND_PCM_FORMAT_A_LAW
//#define DEFAULT_AUDIO_FRAMESIZE       (1)


//#define DEFAULT_SAMPLE_RATE_HZ          (48000)
//#define DEFAULT_SAMPLE_RATE_HZ          (44100)
//#define DEFAULT_SAMPLE_RATE_HZ          (32000)
//#define DEFAULT_SAMPLE_RATE_HZ          (24000)
#define DEFAULT_SAMPLE_RATE_HZ          (16000)
//#define DEFAULT_SAMPLE_RATE_HZ          (8000)

//#define DEFAULT_CHANNEL_COUNT           (1)               /* mono */
#define DEFAULT_CHANNEL_COUNT           (2)               /* stero */


#define DEFAULT_AUDIO_OUTPUT_THRESHOLD_10MS    (5)     /* in 10's of milliseconds */

volatile char reset_stats = 0;
volatile char quit_audio = 0;
volatile long aud_frame = 0;
volatile long aud_nzero = 0;
volatile long aud_frame_live = 0;
volatile long aud_nzero_live = 0;
volatile int aud_nzero_maxseq = 0;
volatile int aud_maxsize = 0;

/*
 *
 * Static variables
 *
 */

static unsigned int framesize = 0;        /* The size of an ALSA frame */
static timer_t timerid;
/* handles to ALSA device */
static snd_pcm_t *paudio_read_handle;
static FILE *f = NULL;


/*
 *
 * Static functions
 *
 */

static int alsa_configure( snd_pcm_t *p_alsa_handle );
static int alsa_read_ready( snd_pcm_t *p_alsa_handle, int *pbytes_to_read);
static int alsa_read( snd_pcm_t *p_alsa_handle, void *p_data_buffer, int data_length);

static void AudioLoop_SigAlarmHandler(int sig)
{
	int tmp;
	int read_len;
	int bytes_to_read;
	char loop_buff[DEFAULT_BLOCK_SIZE];
	static char prev_zero=0;
	static int aud_nzero_curseq=0;
	static int aud_nzero_live_local=0;
	static int aud_frame_live_local=0;
	static int live_counter = LIVE_UPDATE_RATE/TICK_RATE_MS;

	if (live_counter == 0) {
		/* Reset counter */
		live_counter = LIVE_UPDATE_RATE/TICK_RATE_MS;
		aud_nzero_live = aud_nzero_live_local;
		aud_frame_live = aud_frame_live_local;
		aud_nzero_live_local = 0;
		aud_frame_live_local = 0;
	}

	aud_frame++;
	aud_frame_live_local++;

	if(reset_stats) {
		aud_frame = 0;
		aud_nzero = 0;
		aud_nzero_maxseq = 0;
		aud_maxsize = 0;
		aud_nzero_live = 0;
		aud_frame_live = 0;
		reset_stats = 0;
	}

	/* check the data in audio input and output */
	tmp = alsa_read_ready( paudio_read_handle, &bytes_to_read );
	if (tmp < 0)
	{
		/* Unsuccessful, report error */
		printf ("[%d] ERROR: alsa_read_ready() failed\n", __LINE__);
	}

	if (bytes_to_read < DEFAULT_BLOCK_SIZE) {
		aud_nzero_curseq++;
		aud_nzero++;
		aud_nzero_live_local++;
	} else {
		prev_zero = 0;
		aud_nzero_curseq = 0;
	}
	if (aud_nzero_curseq > aud_nzero_maxseq)
		aud_nzero_maxseq = aud_nzero_curseq;

	if ((bytes_to_read/DEFAULT_BLOCK_SIZE)*DEFAULT_BLOCK_SIZE > aud_maxsize)
		aud_maxsize = (bytes_to_read/DEFAULT_BLOCK_SIZE)*DEFAULT_BLOCK_SIZE;

	//Read all whole frames in the input buffer
	while(bytes_to_read >= DEFAULT_BLOCK_SIZE)
	{
		if (bytes_to_read >= DEFAULT_BLOCK_SIZE)
		{
			/* read one block */
			read_len = alsa_read(paudio_read_handle, loop_buff, DEFAULT_BLOCK_SIZE);
			if ((read_len == -1) || (read_len == 0))
			{
				printf ("[%d] Error: alsa_read() failed\n", __LINE__);
			}
			if (f != NULL)
				fwrite((const void *) loop_buff, read_len, 1, f);

		}
		else
		{
			printf("WARNING: --- --- --- too little data to read, adding zeros --- --- ---\n");
			memset(loop_buff, 0, DEFAULT_BLOCK_SIZE);
		}

		bytes_to_read -= DEFAULT_BLOCK_SIZE;
	}
	live_counter--;
}


static int find_ten_ms_data_len
(
 int *ten_ms_data_len
 )
{
	/* Determine a few things about the audio buffers */
	switch(DEFAULT_AUDIO_FORMAT)
	{
		case SND_PCM_FORMAT_MU_LAW:
		case SND_PCM_FORMAT_A_LAW:
			{
				printf("[%d] SND_PCM_FORMAT_MU_LAW SND_PCM_FORMAT_A_LAW\n", __LINE__);

				*ten_ms_data_len = (DEFAULT_SAMPLE_RATE_HZ/100)*sizeof(char)*DEFAULT_CHANNEL_COUNT;
			}
			break;

		case SND_PCM_FORMAT_S16_LE:
			{
				printf("[%d] SND_PCM_FORMAT_S16_NE\n", __LINE__);

				*ten_ms_data_len = (DEFAULT_SAMPLE_RATE_HZ/100)*sizeof(short)*DEFAULT_CHANNEL_COUNT;
			}
			break;

		default:
			{
				printf("[%d] Error, illegal audio format\n", __LINE__);
				return -1;
			}
			break;
	}


	printf("[%d] *ten_ms_data_len = %i\n", __LINE__, *ten_ms_data_len);

	return 0;
}



#define DISABLE_SILENCE_PLAYBACK

/*
 *
 * Global functions
 *
 */
void start_audio(char *alsa_device_capture)
{
    #ifndef DISABLE_SILENCE_PLAYBACK
    char zeroes[DEFAULT_BLOCK_SIZE] = {0};
    #endif
    char loop_buff[DEFAULT_BLOCK_SIZE];
    struct sigaction  act;
    int bytes_to_read;
    int read_len;
    int ten_ms_data_len;
    int audio_output_threshold;
    int audio_output_threshold_blocks;
    struct timeval  current_time;
    int underrun_audio_out;
    int fill_audio_input;

    /* Open output file to save audio if necessary */
    extern char *filename_audio;
    if (filename_audio != NULL) {
	    printf("Writing audio stream to %s\n", filename_audio);
	    f = fopen(filename_audio,"wb");
	    if (f == NULL)
		    perror("fopen");
    }

    snd_pcm_state_t alsa_err;
#ifndef NO_SND_PCM_GET_PARAMS
    snd_pcm_uframes_t period_size = 0;
#endif

    if (find_ten_ms_data_len(&ten_ms_data_len) != 0)
    {
        printf ("[%d] find_ten_ms_data_len() failed\n", __LINE__);
    }

    /* set the audio output threshold, round up to nearset whole block */
    audio_output_threshold_blocks = (((ten_ms_data_len*DEFAULT_AUDIO_OUTPUT_THRESHOLD_10MS)+(DEFAULT_BLOCK_SIZE-1))/DEFAULT_BLOCK_SIZE);
#ifdef TX_RESERVE_BLOCKS
    audio_output_threshold_blocks -= TX_RESERVE_BLOCKS; /* Allow for reserved blocks */
#endif
    audio_output_threshold = audio_output_threshold_blocks*DEFAULT_BLOCK_SIZE;

    /* open the audio device */

    /* open half duplex read */
    printf("[%d] Opening capture device '%s'\n", __LINE__, alsa_device_capture);
    alsa_err = snd_pcm_open(&paudio_read_handle,
                       (const char *) alsa_device_capture,
                       SND_PCM_STREAM_CAPTURE, 0);
    if (alsa_err < 0)
    {
        perror("Error calling snd_pcm_open(CAPTURE) - ");
        return;
    }

    /* configure the audio capture device */
    alsa_err = alsa_configure( paudio_read_handle );
    if ( alsa_err < 0 )
    {
        printf ("[%d] ERROR alsa_configure(read)\n", __LINE__);
    }

    /* open half duplex write */
    printf("[%d] Sample rate = %i\n", __LINE__, DEFAULT_SAMPLE_RATE_HZ);
    printf("[%d] Channels = %i\n", __LINE__, DEFAULT_CHANNEL_COUNT);
#ifndef NO_SND_PCM_GET_PARAMS
    printf("[%d] Period size = %d\n", __LINE__, (unsigned int)period_size);
#endif
    printf("[%d] Block size = %i\n", __LINE__, DEFAULT_BLOCK_SIZE);
    printf("[%d] audio_output_threshold = %i\n", __LINE__, audio_output_threshold);

    /* initialise the audio device */
    /* force read to kick start the audio device.  Many audio devices need this */
    gettimeofday(&current_time, NULL);
    printf("%lu:%lu forcing reads until we get data\n", current_time.tv_sec, current_time.tv_usec);

    do
    {
        read_len = alsa_read( paudio_read_handle, loop_buff, DEFAULT_BLOCK_SIZE);
        if ((read_len == -1) || (read_len == 0))
        {
            printf("[%d] Error: alsa_read() failed\n", __LINE__);
        }

        printf("[%d] read_len = %i\n", __LINE__, read_len);
    }while(read_len <= 0);


    gettimeofday(&current_time, NULL);
    printf("[%d] %lu:%lu Waiting for audio input to build up\n",
        __LINE__, current_time.tv_sec, current_time.tv_usec);

    /* zero the loop buffer */
    memset(loop_buff, 0, DEFAULT_BLOCK_SIZE);

    printf("[%d] Audio input build up done\n", __LINE__);


     /* setup SIGALRM to produce a tick */
    act.sa_handler = (void*)AudioLoop_SigAlarmHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    sigaction(SIGALRM, &act, 0);

    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
	    perror("sigprocmask");
	    exit(1);
    }

    struct sigevent sev;
    struct itimerspec its;

    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
	    perror("timer_create");
	    exit(1);
    }

    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec =  TICK_RATE_MS*1000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1) {
	    perror("timer_settime");
	    exit(1);
    }

    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
	    perror("sigprocmask");
	    exit(1);
    }

    /* loop audio */
    quit_audio = 0;
    aud_frame = 0;
    underrun_audio_out = 0;
    fill_audio_input = 0;
    bytes_to_read = 0;
}


void stop_audio()
{
	struct itimerspec its;

	its.it_value.tv_sec = 0;
	its.it_value.tv_nsec =  0;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;

	if (timer_settime(timerid, 0, &its, NULL) == -1) {
		perror("timer_settime");
		exit(1);
	}

	aud_frame = 0;
	aud_nzero = 0;
	aud_nzero_maxseq = 0;
	aud_maxsize = 0;
	aud_frame_live = 0;
	aud_nzero_live = 0;

	if (f != NULL)
		fclose(f);

	/* close audio device */
	snd_pcm_close(paudio_read_handle);
}


static int alsa_configure( snd_pcm_t *p_alsa_handle    /* The handle to ALSA */ )
{
    snd_pcm_state_t alsa_err;
    snd_pcm_hw_params_t *p_hw_params = NULL;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t tmp;
    unsigned int tmp_uint;

    unsigned int sample_rate = DEFAULT_SAMPLE_RATE_HZ;
    t_device_alsa_audio_use_periods_e alsa_use_periods = DEFAULT_DEVICE_ALSA_AUDIO_PERIODS;

    printf("[%s] Entered, handle %p\n", __FUNCTION__, p_alsa_handle);

    /* Check that the passed handle is legal */
    if((p_alsa_handle == (snd_pcm_t*)-1) || (p_alsa_handle == NULL))
    {
        printf("ERROR: Invalid handle %p\n", p_alsa_handle );
        return -1;
    }

    /* Allocate space for hardware parameters */
    alsa_err = snd_pcm_hw_params_malloc( &p_hw_params );
    if( alsa_err < 0 )
    {
        printf("[%d] ERROR: Cannot allocate hardware parameter structure: (%s)\n",
                __LINE__, snd_strerror( alsa_err ) );
        return -1;
    }

    /* This function loads current settings */
    alsa_err = snd_pcm_hw_params_any( p_alsa_handle, p_hw_params );
    if( alsa_err < 0 )
    {
        printf("[%d] ERROR: Cannot initialize hardware parameter structure: (%s)\n",
                __LINE__, snd_strerror(alsa_err));
        /* Free the hardware params then return error code */
        snd_pcm_hw_params_free( p_hw_params );
        return -1;
    }

    /*
     * ALSA offers choice of interleaved samples (that is left/right for stereo)
     * or 'blocks' of channel data. Interleaved is what we are used to from
     * the OSS way of doing things
     */
    if( ( alsa_err = snd_pcm_hw_params_set_access( p_alsa_handle, p_hw_params,
                    SND_PCM_ACCESS_RW_INTERLEAVED ) ) < 0 )
    {
        printf("[%d] ERROR: Cannot set access type (%s)\n",
                __LINE__, snd_strerror( alsa_err ) );
        /* Free the hardware params then return error code */
        snd_pcm_hw_params_free( p_hw_params );
        return -1;
    }


    /*
     * Set audio format . Set framesize based on this.
     */
    framesize = DEFAULT_AUDIO_FRAMESIZE;
    alsa_err = snd_pcm_hw_params_set_format( p_alsa_handle, p_hw_params, DEFAULT_AUDIO_FORMAT );
    if( alsa_err < 0 )
    {
        printf("[%d] ERROR: Failed to set audio format: %d\n", __LINE__, DEFAULT_AUDIO_FORMAT );
        /* Free the hardware params then return error code */
        snd_pcm_hw_params_free( p_hw_params );
        return -1;
    }

    /* Set sample speed */
    sample_rate = DEFAULT_SAMPLE_RATE_HZ;

#ifdef ALSA_PCM_NEW_HW_PARAMS_API
    if( ( alsa_err = snd_pcm_hw_params_set_rate_near(  p_alsa_handle, p_hw_params,
                    &sample_rate,
                    0 ) ) < 0 )
    {
        printf("[%d] ERROR: Cannot set sample rate (%s)\n", __LINE__, snd_strerror( alsa_err ) );
        snd_pcm_hw_params_free(p_hw_params);
        return -1;
    }
    else
    {
        printf("[%d] Sample rate set to %d Hz\n", __LINE__, sample_rate);
    }
#else
    alsa_err = snd_pcm_hw_params_set_rate_near(  p_alsa_handle, p_hw_params,
                        sample_rate, 0 );
    if( alsa_err < 0 )
    {
        printf("[%d] ERROR: Cannot set sample rate (%s)\n", __LINE__, snd_strerror( alsa_err ) );
        snd_pcm_hw_params_free(p_hw_params);
        return -1;
    }
#endif

    /* set number of channels */
    unsigned int tmp_channels = DEFAULT_CHANNEL_COUNT;
    switch( tmp_channels )
    {
        case 1:
            break;
        case 2:
            framesize <<= 1; // Modify frame size to account for stereo
            break;
        default:
            printf("[%d] ERROR: Invalid config audio channel count %u\n", __LINE__, tmp_channels );
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
            break;
    }

    alsa_err = snd_pcm_hw_params_set_channels_near( p_alsa_handle, p_hw_params, &tmp_channels );
    if( alsa_err < 0)
    {
        printf("[%d] ERROR: Cannot set channel count (%s)\n", __LINE__, snd_strerror ( alsa_err ) );
        /* Free the hardware params then return error code */
        snd_pcm_hw_params_free( p_hw_params );
        return -1;
    }
    else
    {
        printf("[%d] Channels set to %u\n", __LINE__, tmp_channels);
    }


    if( alsa_use_periods == DEVICE_ALSA_AUDIO_USE_PERIODS )
    {
        /*
         * Calculate the period size. Set a period as 10msec of audio data for the
         * given sample rate. This is in 'frames' - there is no need to take
         * bits per sample or number of channels into account, ALSA 'knows' how to
         * adjust for this.
         */
        switch( sample_rate )
        {
            case 48000:
                period_size = 480;
                break;
            case 44100:
                period_size = 441;
                break;
            case 16000:
                period_size = 160;
                break;
            case 8000:
                period_size = 80;
                break;
            default :
                printf( "[%d] ERROR: sample rate not supported (%u)\n", __LINE__, sample_rate );
                /* Free the hardware params then return error code */
                snd_pcm_hw_params_free( p_hw_params );
                return -1;
                break;
        }

        /* Set periods size. Periods used to be called fragments. */
        tmp = period_size;

#ifdef ALSA_PCM_NEW_HW_PARAMS_API
        alsa_err = snd_pcm_hw_params_set_period_size_near( p_alsa_handle, p_hw_params,
                    &period_size, 0);
        if( alsa_err < 0)
        {
            printf("[%d] ERROR: Error setting period size\n", __LINE__);
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
        }
#else
        alsa_err = snd_pcm_hw_params_set_period_size_near( p_alsa_handle, p_hw_params,
                    period_size, 0);
        if( alsa_err < 0)
        {
            printf("[%d] ERROR: Error setting period size\n", __LINE__);
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
        }
#endif


        if( tmp != period_size )
        {
            printf("[%d] WARNING: requested period size not given (%d -> %d)\n",
                __LINE__, (int)tmp, (int)period_size);
        }
        else
        {
            printf("[%d] period_size set to %u\n", __LINE__, (unsigned int)period_size);
        }


        tmp_uint = ALSA_PERIODS;

#ifdef ALSA_PCM_NEW_HW_PARAMS_API
        alsa_err = snd_pcm_hw_params_set_periods_near( p_alsa_handle, p_hw_params, &tmp_uint, 0 );
        if( alsa_err < 0 )
        {
            printf( "[%d] ERROR: Error setting periods.\n", __LINE__ );
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
        }
#else
        alsa_err = snd_pcm_hw_params_set_periods_near( p_alsa_handle, p_hw_params, tmp_uint, 0 );
        if( alsa_err < 0 )
        {
            printf( "[%d] ERROR: Error setting periods.\n", __LINE__ );
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
        }
#endif

        if( tmp_uint != ALSA_PERIODS )
        {
            printf( "[%d] WARNING: requested number of periods not given (%u -> %u)\n",
                __LINE__, ALSA_PERIODS, tmp_uint );
        }
        else
        {
            printf("[%d] Number of periods set to %u\n", __LINE__, ALSA_PERIODS);
        }
    }
    else /* must be DEVICE_ALSA_AUDIO_NOT_USE_PERIODS */
    {
        /*
         * Using parameters to size the buffer based on how many bytes there is in
         * a period multiplied by the number of periods. There may be a better
         * way of doing this....
         */
        alsa_err = snd_pcm_hw_params_set_buffer_size( p_alsa_handle, p_hw_params, DEFAULT_BLOCK_SIZE * ALSA_PERIODS );
        if( alsa_err < 0 )
        {
            printf("\n[%d] ERROR: Error setting buffersize.\n", __LINE__);
            /* Free the hardware params then return error code */
            snd_pcm_hw_params_free( p_hw_params );
            return -1;
        }
    }


    /*
     * Make it so! This call actually sets the parameters
     */
    alsa_err = snd_pcm_hw_params( p_alsa_handle, p_hw_params );
    if( alsa_err < 0 )
    {
        printf("\n[%d] ERROR: Cannot set parameters (%s)\n", __LINE__, snd_strerror (alsa_err));
        /* Free the hardware params then return error code */
        snd_pcm_hw_params_free( p_hw_params );
        return -1;
    }

    /* Free hardware config structure */
    snd_pcm_hw_params_free( p_hw_params );

    printf("[%s] Exit\n", __FUNCTION__);
    return 0;
}


static int alsa_read_ready( snd_pcm_t *p_alsa_handle, int *pbytes_to_read)
{
    int result_code = 0;
    unsigned int data_available;
    int frames_to_read;
    snd_pcm_state_t err;

    /* Check that the passed handle is legal */
    if((p_alsa_handle == (snd_pcm_t*)-1) || (p_alsa_handle == NULL))
    {
        printf("[%d] ERROR: Invalid handle %p\n", __LINE__, p_alsa_handle );
        return -1;
    }

    err = snd_pcm_state(p_alsa_handle);
    switch (err)
    {
        case SND_PCM_STATE_OPEN:
        {
            break;
        }
        case SND_PCM_STATE_SETUP:
        {
            break;
        }
        case SND_PCM_STATE_DRAINING:
        {
            break;
        }
        case SND_PCM_STATE_PAUSED:
        {
            break;
        }
        case SND_PCM_STATE_SUSPENDED:
        {
            break;
        }
#if (SND_LIB_VERSION > 0x10000)
        case SND_PCM_STATE_DISCONNECTED:
        {
            break;
        }
#endif
        case SND_PCM_STATE_XRUN:
        {
            do
            {
                printf ("[%d] Trying snd_pcm_prepare()...\n", __LINE__);
                err = snd_pcm_prepare(p_alsa_handle);
            } while (err < 0);

            err = snd_pcm_start( p_alsa_handle );
            if( err < 0 )
            {
                printf("\n[%d] Cannot start PCM (%s)\n", __LINE__, snd_strerror (err));
                return -1;
            }
            break;
        }
        case SND_PCM_STATE_PREPARED:
        {
            err = snd_pcm_start( p_alsa_handle );
            if( err < 0 )
            {
                printf("\n[%d] Cannot start PCM (%s)\n", __LINE__, snd_strerror (err));
                return -1;
            }
            break;
        }
        case SND_PCM_STATE_RUNNING :
        {
            break;
        }
    }

    /* Get number of frames available for read */
    frames_to_read = snd_pcm_avail_update( p_alsa_handle );

    /*
     * Negative value indicates an error. Usually this is a 'broken pipe' error
     * which indicates an overrun of input. If we detect this then
     * we need to call 'snd_pcm_prepare' to clear the error
     */
    if( frames_to_read < 0 )
    {
        printf( "[%d] WARNING: %s\n", __LINE__, snd_strerror( frames_to_read ) );
        if( frames_to_read == -EPIPE )
        {
            printf("[%d] WARNING: rda: orun\n", __LINE__);
            /* Kick input to clear error */
            snd_pcm_prepare( p_alsa_handle );
        }
        frames_to_read = 0;

        result_code = -1;
    }

    /* Update number of bytes to read back to caller */
    data_available = frames_to_read * framesize;

    /* Return the number of bytes available to read even if there isn't enough for a complete frame */
    *pbytes_to_read = data_available;

    return result_code;
}


static int alsa_read( snd_pcm_t *p_alsa_handle, void *p_data_buffer, int data_length)
{
    int result_code = 0;
    int frames_read = 0;
    snd_pcm_state_t  err;

    /* Check that the passed handle is legal */
    if((p_alsa_handle == (snd_pcm_t*)-1) || (p_alsa_handle == NULL))
    {
        printf("[%d] ERROR: Invalid handle %p\n", __LINE__, p_alsa_handle );
        return -1;
    }

    /* Check the passed buffer to see if it is what is needed */
    if( p_data_buffer == NULL)
    {
        printf("[%d] ERROR: bad data buffer: input data address %p\n", __LINE__, p_data_buffer);
        return -1;
    }

    /* Now perform the ALSA read */

    /*
     * ALSA works in 'frames' so we ask for the
     * 'number of bytes requested'/framesize. We then have to adjust the
     * number of frames actually read back to bytes
     */

    err= snd_pcm_state(p_alsa_handle);
    switch (err)
    {
        case SND_PCM_STATE_OPEN:
        {
            break;
        }
        case SND_PCM_STATE_SETUP:
        {
            break;
        }
        case SND_PCM_STATE_DRAINING:
        {
            break;
        }
        case SND_PCM_STATE_PAUSED:
        {
            break;
        }
        case SND_PCM_STATE_SUSPENDED:
        {
            break;
        }
#if (SND_LIB_VERSION > 0x10000)
        case SND_PCM_STATE_DISCONNECTED:
        {
            break;
        }
#endif
        case SND_PCM_STATE_RUNNING:
         {
             frames_read = snd_pcm_readi( p_alsa_handle, p_data_buffer, (data_length/framesize)
                 /*(p_alsa_context->config.block_size / p_alsa_context->framesize)*/ );
             break;
         }
        case SND_PCM_STATE_XRUN:
        {
            do
            {
                printf ("[%d] Trying snd_pcm_prepare()...\n", __LINE__);
                err = snd_pcm_prepare(p_alsa_handle);
            } while (err < 0);

            err = snd_pcm_start( p_alsa_handle );
            if( err < 0 )
            {
                printf("\n[%d] Error : Cannot start PCM (%s)\n", __LINE__, snd_strerror (err));
                return -1;
            }
            break;
        }
        case SND_PCM_STATE_PREPARED:
        {
             /* Instruct the stream that it has now started */
             frames_read = snd_pcm_readi( p_alsa_handle, p_data_buffer, (data_length/framesize)
                 /*(p_alsa_context->config.block_size / p_alsa_context->framesize)*/ );
             err = snd_pcm_start( p_alsa_handle );
             if( err < 0 )
             {
                 printf("\n[%d] Error :Cannot start PCM (%s)\n", __LINE__, snd_strerror (err));
                 return -1;
             }
        }
    }

    if( frames_read < 0 )
    {
        /* If we get a 'broken pipe' this is an overrun */
        printf("[%d] WARNING: Read error: %s\n", __LINE__, snd_strerror( frames_read ) );
        if(frames_read == -EPIPE)
        {
            /* Kick input to clear error */
            snd_pcm_prepare( p_alsa_handle );
            result_code = -1;
        }
    }
    else
    {
        /* Read was OK so record the size (in bytes) of data read */
        return (frames_read * framesize);
    }

    return result_code;
}


int get_next_audioin_dev(char **name)
{
	static char getting=0;
	static void **hints, **n;

	if (getting == 0) {
		if (snd_device_name_hint(-1, "pcm", &hints) < 0)
			return -1;
		n = hints;
		getting = 1;
	}

	if (*n != NULL) {
		*name = snd_device_name_get_hint(*n, "NAME");
		n++;
		return 0;
	} else {
		snd_device_name_free_hint(hints);
		getting = 0;
	}

	return -1;
}
