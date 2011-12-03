/*
 * linux/sound/arm/ep93xx-i2s.c -- ALSA PCM interface for the edb93xx i2s audio
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/soundcard.h>
#include <linux/dma-mapping.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/initval.h>
#include <sound/ac97_codec.h>

#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/io.h>                                                                                                                             
#include <asm/dma.h>
#include <asm/arch/regs_ac97.h>

#include "ep93xx-ac97.h"

#define	DRIVER_VERSION	"19/3/2007"
#define	DRIVER_DESC	"EP93xx AC97 Audio driver"
static int ac_link_enabled = 0;
static int codec_supported_mixers;

//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

#define WL16 	0
#define WL24	1

#define AUDIO_NAME              		"ep93xx-i2s"
#define AUDIO_SAMPLE_RATE_DEFAULT       44100
#define AUDIO_DEFAULT_VOLUME            0
#define AUDIO_MAX_VOLUME	 	        181
#define AUDIO_DEFAULT_DMACHANNELS       3
#define PLAYBACK_DEFAULT_DMACHANNELS    3
#define CAPTURE_DEFAULT_DMACHANNELS     3

#define CHANNEL_FRONT					(1<<0)
#define CHANNEL_REAR                   	(1<<1)
#define CHANNEL_CENTER_LFE              (1<<2)

static int 								index = SNDRV_DEFAULT_IDX1;          /* Index 0-MAX */
static char 							*id = SNDRV_DEFAULT_STR1;            /* ID for this card */

static void snd_ep93xx_dma_tx_callback( ep93xx_dma_int_t DMAInt, ep93xx_dma_dev_t device, unsigned int user_data);
static void snd_ep93xx_dma_rx_callback( ep93xx_dma_int_t DMAInt, ep93xx_dma_dev_t device, unsigned int user_data);

static const struct snd_pcm_hardware ep93xx_ac97_pcm_hardware = {
    .info				= ( SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_PAUSE  ),
    .formats			= ( SNDRV_PCM_FMTBIT_U8     | SNDRV_PCM_FMTBIT_S8     |
			    			SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE |
			    			SNDRV_PCM_FMTBIT_U16_BE | SNDRV_PCM_FMTBIT_S16_BE |
			    			SNDRV_PCM_FMTBIT_U32_LE | SNDRV_PCM_FMTBIT_S32_LE |
			    			SNDRV_PCM_FMTBIT_U32_BE | SNDRV_PCM_FMTBIT_S32_BE ),
    .rates				= ( SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 |
			   	 			SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
			    			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			    			SNDRV_PCM_RATE_48000 ),
    .rate_min			= 8000,
    .rate_max			= 48000,
    .channels_min		= 1,/*2,*/
    .channels_max		= 2,
					
    .period_bytes_min	= 1 * 1024,
    .period_bytes_max	= 32 * 1024,
    .periods_min		= 1,
    .periods_max		= 32,
    .buffer_bytes_max	= 32 * 1024,
    .fifo_size			= 0,
};

static int ep93xx_calc_closest_freq
(
    ulong   ulPLLFreq, 
    ulong   ulRequestedMClkFreq,
    ulong * pulActualMClkFreq,
    ulong * pulI2SDiv
);

static audio_stream_t output_stream;
static audio_stream_t input_stream;
								
static audio_state_t audio_state =
{
    .output_stream      	= &output_stream,
    .output_dma[0]        	= DMATx_AAC1,
    .output_id[0]          	= "Ac97 out",
	
    .input_stream       	= &input_stream,
    .input_dma[0]          	= DMARx_AAC1,
    .input_id[0]           	="Ac97 in",
	
    .sem                    = __SEMAPHORE_INIT(audio_state.sem,1),
    .codec_set_by_playback  = 0,
    .codec_set_by_capture   = 0,
    .DAC_bit_width		 	= 16,
    .bCompactMode		 	= 0,
};			
		


/*
 * peek
 *
 * Reads an AC97 codec register.  Returns -1 if there was an error.
 */
static int peek(unsigned int uiAddress)
{
	unsigned int uiAC97RGIS;
	
	if( !ac_link_enabled )
	{
		printk("ep93xx ac97 peek: attempt to peek before enabling ac-link.\n");
		return -1;
	}
	
	/*
	 * Check to make sure that the address is aligned on a word boundary 
	 * and is 7E or less.
	 */
	if( ((uiAddress & 0x1)!=0) || (uiAddress > 0x007e))
	{
		return -1;
	}

	/*
	 * How it is supposed to work is:
	 *  - The ac97 controller sends out a read addr in slot 1.
	 *  - In the next frame, the codec will echo that address back in slot 1
	 *    and send the data in slot 2.  SLOT2RXVALID will be set to 1.
	 *
	 * Read until SLOT2RXVALID goes to 1.  Reading the data in AC97S2DATA
	 * clears SLOT2RXVALID.
	 */

	/*
	 * First, delay one frame in case of back to back peeks/pokes.
	 */
	mdelay( 1 );

	/*
	 * Write the address to AC97S1DATA, delay 1 frame, read the flags.
	 */
	outl( uiAddress, AC97S1DATA);
	udelay( 21 * 4 );
	uiAC97RGIS = inl( AC97RGIS );

	/*
	 * Return error if we timed out.
	 */
	if( ((uiAC97RGIS & AC97RGIS_SLOT1TXCOMPLETE) == 0 ) &&
		((uiAC97RGIS & AC97RGIS_SLOT2RXVALID) == 0 ) )
	{
		printk( "ep93xx-ac97 - peek failed reading reg 0x%02x.\n", uiAddress ); 
		return -1;
	}
	
	return ( inl(AC97S2DATA) & 0x000fffff);
}

/*
 * poke
 *
 * Writes an AC97 codec Register.  Return -1 if error.
 */
static int poke(unsigned int uiAddress, unsigned int uiValue)
{
	unsigned int uiAC97RGIS;

	if( !ac_link_enabled )
	{
		printk("ep93xx ac97 poke: attempt to poke before enabling ac-link.\n");
		return -1;
	}
	
	/*
	 * Check to make sure that the address is align on a word boundary and
	 * is 7E or less.  And that the value is a 16 bit value.
	 */
	if( ((uiAddress & 0x1)!=0) || (uiAddress > 0x007e))
	{
		printk("ep93xx ac97 poke: address error.\n");
		return -1;
	}
 	
	/*stop the audio loop from the input to the output directly*/

	if((uiAddress==AC97_0E_MIC_VOL)||(uiAddress==AC97_10_LINE_IN_VOL))
	{
		uiValue = (uiValue | 0x8000);
	
	}
	
	/*
	 * First, delay one frame in case of back to back peeks/pokes.
	 */
	mdelay( 1 );

	/*
	 * Write the data to AC97S2DATA, then the address to AC97S1DATA.
	 */
	outl( uiValue, AC97S2DATA );
	outl( uiAddress, AC97S1DATA );
	
	/*
	 * Wait for the tx to complete, get status.
	 */
	udelay( 30 );/*21*/
	uiAC97RGIS = inl(AC97RGIS);

	/*
	 * Return error if we timed out.
	 */
	if( !(inl(AC97RGIS) & AC97RGIS_SLOT1TXCOMPLETE) )
	{
		printk( "ep93xx-ac97: poke failed writing reg 0x%02x  value 0x%02x.\n", uiAddress, uiValue ); 
		return -1;
	}

	return 0;
}


/*
 * When we get to the multichannel case the pre-fill and enable code
 * will go to the dma driver's start routine.
 */
static void ep93xx_audio_enable( int input_or_output_stream )
{
	unsigned int uiTemp;

	DPRINTK("ep93xx_audio_enable :%x\n",input_or_output_stream);

	/*
	 * Enable the rx or tx channel depending on the value of 
	 * input_or_output_stream
	 */
	if( input_or_output_stream )
	{
		uiTemp = inl(AC97TXCR1);
		outl( (uiTemp | AC97TXCR_TEN), AC97TXCR1 );
	}
	else
	{
		uiTemp = inl(AC97RXCR1);
		outl( (uiTemp | AC97RXCR_REN), AC97RXCR1 );
	}
	
	
	//DDEBUG("ep93xx_audio_enable - EXIT\n");
}

static void ep93xx_audio_disable( int input_or_output_stream )
{
	unsigned int uiTemp;

	DPRINTK("ep93xx_audio_disable\n");

	/*
	 * Disable the rx or tx channel depending on the value of 
	 * input_or_output_stream
	 */
	if( input_or_output_stream )
	{
		uiTemp = inl(AC97TXCR1);
		outl( (uiTemp & ~AC97TXCR_TEN), AC97TXCR1 );
	}
	else
	{
		uiTemp = inl(AC97RXCR1);
		outl( (uiTemp & ~AC97RXCR_REN), AC97RXCR1 );
	}

	//DDEBUG("ep93xx_audio_disable - EXIT\n");
}



/*=======================================================================================*/
/*
 * ep93xx_setup_src
 *
 * Once the ac-link is up and all is good, we want to set the codec to a 
 * usable mode.
 */
static void ep93xx_setup_src(void)
{
	int iTemp;

	/*
	 * Set the VRA bit to enable the SRC.
	 */
	iTemp = peek( AC97_2A_EXT_AUDIO_POWER );
	poke( AC97_2A_EXT_AUDIO_POWER,  (iTemp | 0x1) );
	
	/*
	 * Set the DSRC/ASRC bits to enable the variable rate SRC.
	 */
	iTemp = peek( AC97_60_MISC_CRYSTAL_CONTROL  );
	poke( AC97_60_MISC_CRYSTAL_CONTROL, (iTemp  | 0x0300) );
}

/*
 * ep93xx_set_samplerate
 *
 *   lFrequency       - Sample Rate in Hz
 *   bCapture       - 0 to set Tx sample rate; 1 to set Rx sample rate
 */
static void ep93xx_set_samplerate( long lSampleRate, int bCapture )
{
	unsigned short usDivider, usPhase;

	DPRINTK( "ep93xx_set_samplerate - Fs = %d\n", (int)lSampleRate );

	if( (lSampleRate <  7200) || (lSampleRate > 48000)  )
	{
		printk( "ep93xx_set_samplerate - invalid Fs = %d\n", 
				 (int)lSampleRate );
		return;
	}

	/*
	 * Calculate divider and phase increment.
	 *
	 * divider = round( 0x1770000 / lSampleRate )
	 *  Note that usually rounding is done by adding 0.5 to a floating 
	 *  value and then truncating.  To do this without using floating
	 *  point, I multiply the fraction by two, do the division, then add one, 
	 *  then divide the whole by 2 and then truncate.
	 *  Same effect, no floating point math.
	 *
	 * Ph incr = trunc( (0x1000000 / usDivider) + 1 )
	 */

	usDivider = (unsigned short)( ((2 * 0x1770000 / lSampleRate) +  1) / 2 );

	usPhase = (0x1000000 / usDivider) + 1;

	/*
	 * Write them in the registers.  Spec says divider must be
	 * written after phase incr.
	 */
	if(!bCapture)
	{
		poke( AC97_2C_PCM_FRONT_DAC_RATE, usDivider);
		poke( AC97_64_DAC_SRC_PHASE_INCR, usPhase);
	}
	else
	{
		
		poke( AC97_32_PCM_LR_ADC_RATE,  usDivider);
		poke( AC97_66_ADC_SRC_PHASE_INCR, usPhase);
	}
	
	DPRINTK( "ep93xx_set_samplerate - phase = %d,  divider = %d\n",
				(unsigned int)usPhase, (unsigned int)usDivider );

	/*
	 * We sorta should report the actual samplerate back to the calling
	 * application.  But some applications freak out if they don't get
	 * exactly what they asked for.  So we fudge and tell them what
	 * they want to hear.
	 */
	//audio_samplerate = lSampleRate;

	DPRINTK( "ep93xx_set_samplerate - EXIT\n" );
}

/*
 * ep93xx_set_hw_format
 *
 * Sets up whether the controller is expecting 20 bit data in 32 bit words
 * or 16 bit data compacted to have a stereo sample in each 32 bit word.
 */
static void ep93xx_set_hw_format( long format,long channel )
{
	int bCompactMode;
	
	switch( format )
	{
		/*
		 * Here's all the <=16 bit formats.  We can squeeze both L and R
		 * into one 32 bit sample so use compact mode.
		 */
		case SNDRV_PCM_FORMAT_U8:		   
		case SNDRV_PCM_FORMAT_S8:		   
		case SNDRV_PCM_FORMAT_S16_LE:
		case SNDRV_PCM_FORMAT_U16_LE:
			bCompactMode = 1;
			break;

		/*
		 * Add any other >16 bit formats here...
		 */
		case SNDRV_PCM_FORMAT_S32_LE:
		default:
			bCompactMode = 0;
			break;
	}
	
	if( bCompactMode )
	{
		DPRINTK("ep93xx_set_hw_format - Setting serial mode to 16 bit compact.\n");
	
		/*
		 * Turn on Compact Mode so we can fit each stereo sample into
		 * a 32 bit word.  Twice as efficent for DMA and FIFOs.
		 */
		if(channel==2){
			outl( 0x00008018, AC97RXCR1 );
			outl( 0x00008018, AC97TXCR1 );
		}
		else {
		        outl( 0x00008018, AC97RXCR1 );
                        outl( 0x00008018, AC97TXCR1 );
                }


		audio_state.DAC_bit_width = 16;
		audio_state.bCompactMode = 1;
	}
	else
	{
		DPRINTK("ep93xx_set_hw_format - Setting serial mode to 20 bit non-CM.\n");
	
		/*
		 * Turn off Compact Mode so we can do > 16 bits per channel 
		 */
		if(channel==2){
			outl( 0x00004018, AC97RXCR1 );
			outl( 0x00004018, AC97TXCR1 );
		}
		else{
                        outl( 0x00004018, AC97RXCR1 );
                        outl( 0x00004018, AC97TXCR1 );
		}

		audio_state.DAC_bit_width = 20;
		audio_state.bCompactMode = 0;
	}

}

/*
 * ep93xx_stop_loop
 *
 * Once the ac-link is up and all is good, we want to set the codec to a
 * usable mode.
 */
static void ep93xx_stop_loop(void)
{
        int iTemp;
                                                                                                                             
        /*
         * Set the AC97_0E_MIC_VOL MUTE bit to enable the LOOP.
         */
        iTemp = peek( AC97_0E_MIC_VOL );
        poke( AC97_0E_MIC_VOL,  (iTemp | 0x8000) );
                                                                                                                             
        /*
         * Set the AC97_10_LINE_IN_VOL MUTE bit to enable the LOOP.
         */
        iTemp = peek( AC97_10_LINE_IN_VOL  );
        poke( AC97_10_LINE_IN_VOL, (iTemp  | 0x8000) );
}

/*
 * ep93xx_init_ac97_controller
 *
 * This routine sets up the Ac'97 Controller.
 */
static void ep93xx_init_ac97_controller(void)
{
	unsigned int uiDEVCFG, uiTemp;

	DPRINTK("ep93xx_init_ac97_controller - enter\n");

	/*
	 * Configure the multiplexed Ac'97 pins to be Ac97 not I2s.
	 * Configure the EGPIO4 and EGPIO6 to be GPIOS, not to be  
	 * SDOUT's for the second and third I2S controller channels.
	 */
	uiDEVCFG = inl(SYSCON_DEVCFG);
	
	uiDEVCFG &= ~(SYSCON_DEVCFG_I2SonAC97 | 
				  SYSCON_DEVCFG_A1onG |
				  SYSCON_DEVCFG_A2onG);
		
	SysconSetLocked(SYSCON_DEVCFG, uiDEVCFG);

	/*
	 * Disable the AC97 controller internal loopback.  
	 * Disable Override codec ready.
	 */
	outl( 0, AC97GCR );

	/*
	 * Enable the AC97 Link.
	 */
	uiTemp = inl(AC97GCR);
	outl( (uiTemp | AC97GSR_IFE), AC97GCR );

	/*
	 * Set the TIMEDRESET bit.  Will cause a > 1uSec reset of the ac-link.
	 * This bit is self resetting.
	 */
	outl( AC97RESET_TIMEDRESET, AC97RESET );
	
	/*
	 *  Delay briefly, but let's not hog the processor.
	 */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout( 5 ); /* 50 mSec */

	/*
	 * Read the AC97 status register to see if we've seen a CODECREADY
	 * signal from the AC97 codec.
	 */
	if( !(inl(AC97RGIS) & AC97RGIS_CODECREADY))
	{
		printk( "ep93xx-ac97 - FAIL: CODECREADY still low!\n");
		return;
	}

	/*
	 *  Delay for a second, not hogging the processor
	 */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout( HZ ); /* 1 Sec */
	
	/*
	 * Now the Ac-link is up.  We can read and write codec registers.
	 */
	ac_link_enabled = 1;

	/*
	 * Set up the rx and tx channels
	 * Set the CM bit, data size=16 bits, enable tx slots 3 & 4.
	 */
	ep93xx_set_hw_format( EP93XX_DEFAULT_FORMAT,EP93XX_DEFAULT_NUM_CHANNELS );

	DPRINTK( "ep93xx-ac97 -- AC97RXCR1:  %08x\n", inl(AC97RXCR1) ); 
	DPRINTK( "ep93xx-ac97 -- AC97TXCR1:  %08x\n", inl(AC97TXCR1) ); 

	DPRINTK("ep93xx_init_ac97_controller - EXIT - success\n");

}

#ifdef alsa_ac97_debug
static void ep93xx_dump_ac97_regs(void)
{
	int i;
	unsigned int reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7;
	
	DPRINTK( "---------------------------------------------\n");
	DPRINTK( "   :   0    2    4    6    8    A    C    E\n" ); 
	
	for( i=0 ; i < 0x80 ; i+=0x10 )
	{
		reg0 = 0xffff & (unsigned int)peek( i );
		reg1 = 0xffff & (unsigned int)peek( i + 0x2 );
		reg2 = 0xffff & (unsigned int)peek( i + 0x4 );
		reg3 = 0xffff & (unsigned int)peek( i + 0x6 );
		reg4 = 0xffff & (unsigned int)peek( i + 0x8 );
		reg5 = 0xffff & (unsigned int)peek( i + 0xa );
		reg6 = 0xffff & (unsigned int)peek( i + 0xc );
		reg7 = 0xffff & (unsigned int)peek( i + 0xe );

		DPRINTK( " %02x : %04x %04x %04x %04x %04x %04x %04x %04x\n", 
				 i, reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7);
	}

	DPRINTK( "---------------------------------------------\n");
}
#endif


#define supported_mixer(FOO) \
        ( (FOO >= 0) && \
        (FOO < SOUND_MIXER_NRDEVICES) && \
        codec_supported_mixers & (1<<FOO) )
                                                                                                                             
/*
 * Available record sources.
 * LINE1 refers to AUX in.
 * IGAIN refers to input gain which means stereo mix.
 */
#define AC97_RECORD_MASK \
        (SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_IGAIN | SOUND_MASK_VIDEO |\
        SOUND_MASK_LINE1 | SOUND_MASK_LINE | SOUND_MASK_PHONEIN)
                                                                                                                             
#define AC97_STEREO_MASK \
        (SOUND_MASK_VOLUME | SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_CD | \
        SOUND_MASK_ALTPCM | SOUND_MASK_IGAIN | SOUND_MASK_LINE1 | SOUND_MASK_VIDEO)
                                                                                                                             
#define AC97_SUPPORTED_MASK \
        (AC97_STEREO_MASK | SOUND_MASK_BASS | SOUND_MASK_TREBLE | \
        SOUND_MASK_SPEAKER | SOUND_MASK_MIC | \
        SOUND_MASK_PHONEIN | SOUND_MASK_PHONEOUT)




/* this table has default mixer values for all OSS mixers. */
typedef struct  {
	int mixer;
	unsigned int value;
} mixer_defaults_t;

/*
 * Default mixer settings that are set up during boot.
 *
 * These values are 16 bit numbers in which the upper byte is right volume
 * and the lower byte is left volume or mono volume for mono controls.
 *
 * OSS Range for each of left and right volumes is 0 to 100 (0x00 to 0x64).
 * 
 */
static mixer_defaults_t mixer_defaults[SOUND_MIXER_NRDEVICES] = 
{
	/* Outputs */
	{SOUND_MIXER_VOLUME,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */
	{SOUND_MIXER_ALTPCM,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */
	{SOUND_MIXER_PHONEOUT,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */

	/* PCM playback gain */
	{SOUND_MIXER_PCM,		0x4b4b},   /* 0 dB */  /* -34.5dB to +12dB */

	/* Record gain */
	{SOUND_MIXER_IGAIN,		0x0000},   /* 0 dB */  /* -34.5dB to +12dB */

	/* Inputs */
	{SOUND_MIXER_MIC,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_LINE,		0x4b4b},   /* 0 dB */  /* -34.5dB to +12dB */

	/* Inputs that are not connected. */
	{SOUND_MIXER_SPEAKER,	0x0000},   /* mute */  /* -45dB   to   0dB */
	{SOUND_MIXER_PHONEIN,	0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_CD,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_VIDEO,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_LINE1,		0x0000},   /* mute */  /* -34.5dB to +12dB */

	{-1,0} /* last entry */
};

/* table to scale scale from OSS mixer value to AC97 mixer register value */	
typedef struct {
	unsigned int offset;
	int scale;
} ac97_mixer_hw_t; 

static ac97_mixer_hw_t ac97_hw[SOUND_MIXER_NRDEVICES] = 
{
	[SOUND_MIXER_VOLUME]		=  	{AC97_02_MASTER_VOL,	64},
	[SOUND_MIXER_BASS]			=	{0, 0},
	[SOUND_MIXER_TREBLE]		=	{0, 0},
	[SOUND_MIXER_SYNTH]			=  	{0,	0},
	[SOUND_MIXER_PCM]			=  	{AC97_18_PCM_OUT_VOL,	32},
	[SOUND_MIXER_SPEAKER]		=  	{AC97_0A_PC_BEEP_VOL,	32},
	[SOUND_MIXER_LINE]			=  	{AC97_10_LINE_IN_VOL,	32},
	[SOUND_MIXER_MIC]			=  	{AC97_0E_MIC_VOL,		32},
	[SOUND_MIXER_CD]			=  	{AC97_12_CD_VOL,		32},
	[SOUND_MIXER_IMIX]			=  	{0,	0},
	[SOUND_MIXER_ALTPCM]		=  	{AC97_04_HEADPHONE_VOL,	64},
	[SOUND_MIXER_RECLEV]		=  	{0,	0},
	[SOUND_MIXER_IGAIN]			=  	{AC97_1C_RECORD_GAIN,	16},
	[SOUND_MIXER_OGAIN]			=  	{0,	0},
	[SOUND_MIXER_LINE1]			=  	{AC97_16_AUX_VOL,		32},
	[SOUND_MIXER_LINE2]			=  	{0,	0},
	[SOUND_MIXER_LINE3]			=  	{0,	0},
	[SOUND_MIXER_DIGITAL1]		=  	{0,	0},
	[SOUND_MIXER_DIGITAL2]		=  	{0,	0},
	[SOUND_MIXER_DIGITAL3]		=  	{0,	0},
	[SOUND_MIXER_PHONEIN]		=  	{AC97_0C_PHONE_VOL,		32},
	[SOUND_MIXER_PHONEOUT]		=  	{AC97_06_MONO_VOL,		64},
	[SOUND_MIXER_VIDEO]			=  	{AC97_14_VIDEO_VOL,		32},
	[SOUND_MIXER_RADIO]			=  	{0,	0},
	[SOUND_MIXER_MONITOR]		=  	{0,	0},
};


/* the following tables allow us to go from OSS <-> ac97 quickly. */
enum ac97_recsettings 
{
	AC97_REC_MIC=0,
	AC97_REC_CD,
	AC97_REC_VIDEO,
	AC97_REC_AUX,
	AC97_REC_LINE,
	AC97_REC_STEREO, /* combination of all enabled outputs..  */
	AC97_REC_MONO,	      /*.. or the mono equivalent */
	AC97_REC_PHONE
};

static const unsigned int ac97_rm2oss[] = 
{
	[AC97_REC_MIC] 	 = SOUND_MIXER_MIC,
	[AC97_REC_CD] 	 = SOUND_MIXER_CD,
	[AC97_REC_VIDEO] = SOUND_MIXER_VIDEO,
	[AC97_REC_AUX] 	 = SOUND_MIXER_LINE1,
	[AC97_REC_LINE]  = SOUND_MIXER_LINE,
	[AC97_REC_STEREO]= SOUND_MIXER_IGAIN,
	[AC97_REC_PHONE] = SOUND_MIXER_PHONEIN
};

/* indexed by bit position */
static const unsigned int ac97_oss_rm[] = 
{
	[SOUND_MIXER_MIC] 	= AC97_REC_MIC,
	[SOUND_MIXER_CD] 	= AC97_REC_CD,
	[SOUND_MIXER_VIDEO] = AC97_REC_VIDEO,
	[SOUND_MIXER_LINE1] = AC97_REC_AUX,
	[SOUND_MIXER_LINE] 	= AC97_REC_LINE,
	[SOUND_MIXER_IGAIN]	= AC97_REC_STEREO,
	[SOUND_MIXER_PHONEIN] 	= AC97_REC_PHONE
};


/*
 * ep93xx_write_mixer
 *
 */
static void ep93xx_write_mixer
( 
	int oss_channel,
	unsigned int left, 
	unsigned int right
)
{
	u16 val = 0;
	ac97_mixer_hw_t * mh = &ac97_hw[oss_channel];

	DPRINTK("ac97_codec: wrote OSS %2d (ac97 0x%02x), "
	       "l:%2d, r:%2d:",
	       oss_channel, mh->offset, left, right);

	if( !mh->scale )
	{
		printk( "ep93xx-ac97.c: ep93xx_write_mixer - not a valid OSS channel\n");
		return;
	}

	if( AC97_STEREO_MASK & (1 << oss_channel) ) 
	{
		/* stereo mixers */
		if (left == 0 && right == 0) 
		{
			val = 0x8000;
		} 
		else 
		{
			if (oss_channel == SOUND_MIXER_IGAIN) 
			{
				right = (right * mh->scale) / 100;
				left = (left * mh->scale) / 100;
				if (right >= mh->scale)
					right = mh->scale-1;
				if (left >= mh->scale)
					left = mh->scale-1;
			} 
			else 
			{
				right = ((100 - right) * mh->scale) / 100;
				left = ((100 - left) * mh->scale) / 100;
				if (right >= mh->scale)
					right = mh->scale-1;
				if (left >= mh->scale)
					left = mh->scale-1;
			}
			val = (left << 8) | right;
		}
	} 
	else if(left == 0) 
	{
		val = 0x8000;
	} 
	else if( (oss_channel == SOUND_MIXER_SPEAKER) ||
			(oss_channel == SOUND_MIXER_PHONEIN) ||
			(oss_channel == SOUND_MIXER_PHONEOUT) )
	{
		left = ((100 - left) * mh->scale) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val = left;
	} 
	else if (oss_channel == SOUND_MIXER_MIC) 
	{
		val = peek( mh->offset) & ~0x801f;
		left = ((100 - left) * mh->scale) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= left;
	} 
	/*  
	 * For bass and treble, the low bit is optional.  Masking it
	 * lets us avoid the 0xf 'bypass'.
	 * Do a read, modify, write as we have two contols in one reg. 
	 */
	else if (oss_channel == SOUND_MIXER_BASS) 
	{
		val = peek( mh->offset) & ~0x0f00;
		left = ((100 - left) * mh->scale) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= (left << 8) & 0x0e00;
	} 
	else if (oss_channel == SOUND_MIXER_TREBLE) 
	{
		val = peek( mh->offset) & ~0x000f;
		left = ((100 - left) * mh->scale) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= left & 0x000e;
	}
	
	DPRINTK(" 0x%04x", val);

	poke( mh->offset, val );

#ifdef alsa_ac97_debug
	val = peek( mh->offset );
	DEBUG(" -> 0x%04x\n", val);
#endif

}

/* a thin wrapper for write_mixer */
static void ep93xx_set_mixer
(
	unsigned int oss_mixer, 
	unsigned int val 
) 
{
	unsigned int left,right;

	/* cleanse input a little */
	right = ((val >> 8)  & 0xff) ;
	left = (val  & 0xff) ;

	if (right > 100) right = 100;
	if (left > 100) left = 100;

	/*mixer_state[oss_mixer] = (right << 8) | left;*/
	ep93xx_write_mixer( oss_mixer, left, right);
}

static void ep93xx_init_mixer(void)
{
	u16 cap;
	int i;

	/* mixer masks */
	codec_supported_mixers 	= AC97_SUPPORTED_MASK;
	
	cap = peek( AC97_00_RESET );
	if( !(cap & 0x04) )
	{
		codec_supported_mixers &= ~(SOUND_MASK_BASS|SOUND_MASK_TREBLE);
	}
	if( !(cap & 0x10) )
	{
		codec_supported_mixers &= ~SOUND_MASK_ALTPCM;
	}

	/* 
	 * Detect bit resolution of output volume controls by writing to the
	 * 6th bit (not unmuting yet)
	 */
	poke( AC97_02_MASTER_VOL, 0xa020 );
	if( peek( AC97_02_MASTER_VOL) != 0xa020 )
	{
		ac97_hw[SOUND_MIXER_VOLUME].scale = 32;
	}

	poke( AC97_04_HEADPHONE_VOL, 0xa020 );
	if( peek( AC97_04_HEADPHONE_VOL) != 0xa020 )
	{
		ac97_hw[AC97_04_HEADPHONE_VOL].scale = 32;
	}

	poke( AC97_06_MONO_VOL, 0x8020 );
	if( peek( AC97_06_MONO_VOL) != 0x8020 )
	{
		ac97_hw[AC97_06_MONO_VOL].scale = 32;
	}

	/* initialize mixer channel volumes */
	for( i = 0; 
		(i < SOUND_MIXER_NRDEVICES) && (mixer_defaults[i].mixer != -1) ; 
		i++ ) 
	{
		if( !supported_mixer( mixer_defaults[i].mixer) )
		{ 
			continue;
		}
		
		ep93xx_set_mixer( mixer_defaults[i].mixer, mixer_defaults[i].value);
	}

}

static int ep93xx_set_recsource( int mask ) 
{
	unsigned int val;

	/* Arg contains a bit for each recording source */
	if( mask == 0 ) 
	{
		return 0;
	}
	
	mask &= AC97_RECORD_MASK;
	
	if( mask == 0 ) 
	{
		return -EINVAL;
	}
				
	/*
	 * May have more than one bit set.  So clear out currently selected
	 * record source value first (AC97 supports only 1 input) 
	 */
	val = (1 << ac97_rm2oss[peek( AC97_1A_RECORD_SELECT ) & 0x07]);
	if (mask != val)
	    mask &= ~val;
       
	val = ffs(mask); 
	val = ac97_oss_rm[val-1];
	val |= val << 8;  /* set both channels */

	/*
	 *
	 */
        val = peek( AC97_1A_RECORD_SELECT ) & 0x0707;
        if ((val&0x0404)!=0)
          val=0x0404;
        else if((val&0x0000)!=0)
          val=0x0101;


	DPRINTK("ac97_codec: setting ac97 recmask to 0x%04x\n", val);

	poke( AC97_1A_RECORD_SELECT, val);

	return 0;
}

/*
 * ep93xx_init_ac97_codec
 *
 * Program up the external Ac97 codec.
 *
 */
static void ep93xx_init_ac97_codec( void )
{
	DPRINTK("ep93xx_init_ac97_codec - enter\n");

	ep93xx_setup_src();
	ep93xx_set_samplerate( AUDIO_SAMPLE_RATE_DEFAULT, 0 );
	ep93xx_set_samplerate( AUDIO_SAMPLE_RATE_DEFAULT, 1 );
	ep93xx_init_mixer();

	DPRINTK("ep93xx_init_ac97_codec - EXIT\n");
	
}


/*
 * ep93xx_audio_init
 * Audio interface
 */
static void ep93xx_audio_init(void)
{
	DPRINTK("ep93xx_audio_init - enter\n");
	/*
	 * Init the controller, enable the ac-link.
	 * Initialize the codec.
	 */	 
	ep93xx_init_ac97_controller();
	ep93xx_init_ac97_codec();
	/*stop the audio loop from the input to the output directly*/
	ep93xx_stop_loop();
	
#ifdef alsa_ac97_debug
	ep93xx_dump_ac97_regs();
#endif
	DPRINTK("ep93xx_audio_init - EXIT\n");
}
                     
/*====================================================================================*/                     


static void print_audio_format( long format )
{
    switch( format ){
	case SNDRV_PCM_FORMAT_S8:
		DPRINTK( "AFMT_S8\n" );		   
		break;
			
	case SNDRV_PCM_FORMAT_U8:		   
		DPRINTK( "AFMT_U8\n" );		   
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		DPRINTK( "AFMT_S16_LE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_S16_BE:
		DPRINTK( "AFMT_S16_BE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_U16_LE:
		DPRINTK( "AFMT_U16_LE\n" );		   
		break;
	case SNDRV_PCM_FORMAT_U16_BE:
		DPRINTK( "AFMT_U16_BE\n" );
		break;
			
	case SNDRV_PCM_FORMAT_S24_LE:
		DPRINTK( "AFMT_S24_LE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_S24_BE:
		DPRINTK( "AFMT_S24_BE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
		DPRINTK( "AFMT_U24_LE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_U24_BE:
		DPRINTK( "AFMT_U24_BE\n" );		   
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		DPRINTK( "AFMT_S24_LE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_S32_BE:
		DPRINTK( "AFMT_S24_BE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_U32_LE:
		DPRINTK( "AFMT_U24_LE\n" );		   
		break;

	case SNDRV_PCM_FORMAT_U32_BE:
		DPRINTK( "AFMT_U24_BE\n" );		   
		break;		
	default:
		DPRINTK( "ep93xx_i2s_Unsupported Audio Format\n" );		   
		break;
    }
}

static void audio_set_format( audio_stream_t * s, long val )
{
    DPRINTK( "ep93xx_i2s_audio_set_format enter.  Format requested (%d) %d ", 
				(int)val,SNDRV_PCM_FORMAT_S16_LE);
    print_audio_format( val );
	
    switch( val ){
	case SNDRV_PCM_FORMAT_S8:
		s->audio_format = SNDRV_PCM_FORMAT_S8;
		s->audio_stream_bitwidth = 8;
		break;
			
	case SNDRV_PCM_FORMAT_U8:		   
		s->audio_format = SNDRV_PCM_FORMAT_U8;
		s->audio_stream_bitwidth = 8;
		break;
			
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		s->audio_format = SNDRV_PCM_FORMAT_S16_LE;
		s->audio_stream_bitwidth = 16;
		break;

	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_U16_BE:
		s->audio_format = SNDRV_PCM_FORMAT_U16_LE;
		s->audio_stream_bitwidth = 16;
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:		
		s->audio_format = SNDRV_PCM_FORMAT_S24_LE;
		s->audio_stream_bitwidth = 24;
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_U24_BE:		
        	s->audio_format = SNDRV_PCM_FORMAT_U24_LE;
		s->audio_stream_bitwidth = 24;
		break;
		
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S32_BE:				
        	s->audio_format = SNDRV_PCM_FORMAT_S32_LE;
		s->audio_stream_bitwidth = 32;
		break;		
	default:
		DPRINTK( "ep93xx_i2s_Unsupported Audio Format\n" );	
		break;
    }

    DPRINTK( "ep93xx_i2s_audio_set_format EXIT format set to be (%d) ", (int)s->audio_format );
    print_audio_format( (long)s->audio_format );
}

static __inline__ unsigned long copy_to_user_S24_LE( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
			    
    int total_to_count = to_count;
    int *user_ptr = (int *)to;	/* 32 bit user buffer */
    int count;
    	
    count = 8 * stream->dma_num_channels;
	
    while (to_count > 0){
    
	__put_user( (int)( *dma_buffer_0++ ), user_ptr++ );
	__put_user( (int)( *dma_buffer_0++ ), user_ptr++ );
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (int)( *dma_buffer_1++ ), user_ptr++ );
	    __put_user( (int)( *dma_buffer_1++ ), user_ptr++ );
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( (int)( *dma_buffer_2++ ), user_ptr++ );
	    __put_user( (int)( *dma_buffer_2++ ), user_ptr++ );
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U24_LE
(
    audio_stream_t *stream,
    const char *to, 
    unsigned long to_count
)
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
			
    int total_to_count = to_count;
    unsigned int * user_ptr = (unsigned int *)to;	/* 32 bit user buffer */
    int count;
	
    count = 8 * stream->dma_num_channels;
	
    while (to_count > 0){ 
	__put_user( ((unsigned int)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
	__put_user( ((unsigned int)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( ((unsigned int)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned int)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( ((unsigned int)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned int)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_S16_LE( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int total_to_count = to_count;
    short * user_ptr = (short *)to;	/* 16 bit user buffer */
    int count;
    	
    count = 4 * stream->dma_num_channels;
		
    while (to_count > 0){

	__put_user( (short)( *dma_buffer_0++ ), user_ptr++ );
	__put_user( (short)( *dma_buffer_0++ ), user_ptr++ );

        if( stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (short)( *dma_buffer_1++ ), user_ptr++ );
	    __put_user( (short)( *dma_buffer_1++ ), user_ptr++ );
	}

        if( stream->audio_channels_flag  & CHANNEL_CENTER_LFE ){
	    __put_user( (short)( *dma_buffer_2++ ), user_ptr++ );
	    __put_user( (short)( *dma_buffer_2++ ), user_ptr++ );
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U16_LE( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    short * user_ptr = (short *)to;	/* 16 bit user buffer */

    count = 4 * stream->dma_num_channels;
		
    while (to_count > 0){

	__put_user( ((unsigned short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
	__put_user( ((unsigned short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( ((unsigned short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( ((unsigned short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_S8( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    char * user_ptr = (char *)to;  /*  8 bit user buffer */

    count = 2 * stream->dma_num_channels;
	
    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;
		
    while (to_count > 0){

	__put_user( (char)( *dma_buffer_0 ), user_ptr++ );
	dma_buffer_0 += 4;
	__put_user( (char)( *dma_buffer_0 ), user_ptr++ );
	dma_buffer_0 += 4;

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (char)( *dma_buffer_1 ), user_ptr++ );
            dma_buffer_1 += 4;
	    __put_user( (char)( *dma_buffer_1 ), user_ptr++ );
	    dma_buffer_1 += 4;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( (char)( *dma_buffer_2 ), user_ptr++ );
	    dma_buffer_2 += 4;
	    __put_user( (char)( *dma_buffer_2 ), user_ptr++ );
	    dma_buffer_2 += 4;
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U8( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    char * user_ptr = (char *)to;  /*  8 bit user buffer */

    count = 2 * stream->dma_num_channels;
		
    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;
	
    while (to_count > 0){
	
	__put_user( (char)( *dma_buffer_0 ) ^ 0x80, user_ptr++ );
	dma_buffer_0 += 4;
	__put_user( (char)( *dma_buffer_0 ) ^ 0x80, user_ptr++ );
	dma_buffer_0 += 4;
				
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (char)( *dma_buffer_1 ) ^ 0x80, user_ptr++ );
	    dma_buffer_1 += 4;
	    __put_user( (char)( *dma_buffer_1 ) ^ 0x80, user_ptr++ );
	    dma_buffer_1 += 4;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( (char)( *dma_buffer_2 ) ^ 0x80, user_ptr++ );
	    dma_buffer_2 += 4;
	    __put_user( (char)( *dma_buffer_2 ) ^ 0x80, user_ptr++ );
	    dma_buffer_2 += 4;
	}
	to_count -= count;
    }
    return total_to_count;
}




static __inline__ unsigned long copy_to_user_S16_LE_CM
(
    audio_stream_t *stream,
    const char *to, 
    unsigned long to_count
)
{
    short *dma_buffer_0 = (short *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int total_to_count = to_count;
    short * user_ptr = (short *)to;	/* 16 bit user buffer */
    int count;
    
    	
    count = 4 * stream->dma_num_channels;
		
    while (to_count > 0){
    	if(stream->audio_num_channels == 2){
		__put_user( (short)( *dma_buffer_0++ ), user_ptr++ );
		__put_user( (short)( *dma_buffer_0++ ), user_ptr++ );
		to_count -= count;
	}
	else{
		dma_buffer_0++; 
		__put_user( (short)( *dma_buffer_0++ ), user_ptr++ );
		to_count -= 2;
	}
	
        if( stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (short)( *dma_buffer_1++ ), user_ptr++ );
	    __put_user( (short)( *dma_buffer_1++ ), user_ptr++ );
	}

        if( stream->audio_channels_flag  & CHANNEL_CENTER_LFE ){
	    __put_user( (short)( *dma_buffer_2++ ), user_ptr++ );
	    __put_user( (short)( *dma_buffer_2++ ), user_ptr++ );
	}
	//to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U16_LE_CM( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    unsigned short *dma_buffer_0 = (unsigned short *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    unsigned short * user_ptr = (unsigned short *)to;	/* 16 bit user buffer */

    count = 4 * stream->dma_num_channels;
		
    while (to_count > 0){

	if(stream->audio_num_channels == 2){
		__put_user( ((unsigned short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
		__put_user( ((unsigned short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
		to_count -= count;
	}
	else{
		dma_buffer_0++;
		__put_user( ((unsigned short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
		to_count -= 2;
	}
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( ((unsigned short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( ((unsigned short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((unsigned short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	}
	//to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_S8_CM( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    unsigned short *dma_buffer_0 = (unsigned short *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    char * user_ptr = (char *)to;  /*  8 bit user buffer */

    count = 2 * stream->dma_num_channels;
	
    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;
		
    while (to_count > 0){	
	if(stream->audio_num_channels == 2){
		__put_user( (char)( *dma_buffer_0++ >> 8), user_ptr++ );
		//dma_buffer_0 += 4;
		__put_user( (char)( *dma_buffer_0++ >> 8), user_ptr++ );
		//dma_buffer_0 += 4;
		to_count -= count;
	}
	else{
		dma_buffer_0++ ;
		__put_user( (char)( *dma_buffer_0++ >> 8), user_ptr++ );
		
		to_count -= 1;
	}
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (char)( *dma_buffer_1 ), user_ptr++ );
            dma_buffer_1 += 4;
	    __put_user( (char)( *dma_buffer_1 ), user_ptr++ );
	    dma_buffer_1 += 4;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( (char)( *dma_buffer_2 ), user_ptr++ );
	    dma_buffer_2 += 4;
	    __put_user( (char)( *dma_buffer_2 ), user_ptr++ );
	    dma_buffer_2 += 4;
	}
	//to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U8_CM( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    unsigned short *dma_buffer_0 = (unsigned short *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    char * user_ptr = (char *)to;  /*  8 bit user buffer */

    count = 2 * stream->dma_num_channels;
		
    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;
	
    while (to_count > 0){
	if(stream->audio_num_channels == 2){
		__put_user( (char)( *dma_buffer_0++  >>8) ^ 0x80, user_ptr++ );
		//dma_buffer_0 += 4;
		__put_user( (char)( *dma_buffer_0++  >>8) ^ 0x80, user_ptr++ );
		//dma_buffer_0 += 4;
		to_count -= count;
	}
	else{
		dma_buffer_0++;
		__put_user( (char)( *dma_buffer_0++  >>8) ^ 0x80, user_ptr++ );
		//dma_buffer_0 += 4;
		to_count--;
	}
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( (char)( *dma_buffer_1 ) ^ 0x80, user_ptr++ );
	    dma_buffer_1 += 4;
	    __put_user( (char)( *dma_buffer_1 ) ^ 0x80, user_ptr++ );
	    dma_buffer_1 += 4;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( (char)( *dma_buffer_2 ) ^ 0x80, user_ptr++ );
	    dma_buffer_2 += 4;
	    __put_user( (char)( *dma_buffer_2 ) ^ 0x80, user_ptr++ );
	    dma_buffer_2 += 4;
	}
	//to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_U32( audio_stream_t *stream, const char *to, unsigned long to_count )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];

    if(__copy_to_user( (char *)to, dma_buffer_0, to_count))
    {
	return -EFAULT;
    }
    return to_count;
}

static __inline__ int copy_to_user_with_conversion
(
    audio_stream_t *stream,
    const char *to, 
    int toCount,
    int bCompactMode
)
{
    int ret = 0;
	
    if( toCount == 0 ){
	DPRINTK("ep93xx_i2s_copy_to_user_with_conversion - nothing to copy!\n");
    }
    
    if( bCompactMode == 1 ){
        
        switch( stream->audio_format ){

	case SNDRV_PCM_FORMAT_S8:
		ret = copy_to_user_S8_CM( stream, to, toCount );
		break;
			
	case SNDRV_PCM_FORMAT_U8:
		ret = copy_to_user_U8_CM( stream, to, toCount );
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		ret = copy_to_user_S16_LE_CM( stream, to, toCount );
		break;
		
	case SNDRV_PCM_FORMAT_U16_LE:
		ret = copy_to_user_U16_LE_CM( stream, to, toCount );
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		//ret = copy_to_user_S24_LE( stream, to, toCount );
		//break;
		
	case SNDRV_PCM_FORMAT_U24_LE:
		//ret = copy_to_user_U24_LE( stream, to, toCount );
		//break;
		
	case SNDRV_PCM_FORMAT_S32_LE:	
        default:
                DPRINTK( "ep93xx_i2s copy to user unsupported audio format %x\n",stream->audio_format );
		break;
        }
        	
    }
    else{
     
        switch( stream->audio_format ){

	case SNDRV_PCM_FORMAT_S8:
		ret = copy_to_user_S8( stream, to, toCount );
		break;
			
	case SNDRV_PCM_FORMAT_U8:
		ret = copy_to_user_U8( stream, to, toCount );
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		ret = copy_to_user_S16_LE( stream, to, toCount );
		break;
		
	case SNDRV_PCM_FORMAT_U16_LE:
		ret = copy_to_user_U16_LE( stream, to, toCount );
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		//ret = copy_to_user_S24_LE( stream, to, toCount );
		//break;
		
	case SNDRV_PCM_FORMAT_U24_LE:
		//ret = copy_to_user_U24_LE( stream, to, toCount );
		//break;
		DPRINTK( "ep93xx_i2s copy to user unsupported audio format %x\n",stream->audio_format );
		break;
		
	case SNDRV_PCM_FORMAT_S32_LE:
	
		//__copy_to_user( (char *)to, from, toCount);
		ret = copy_to_user_U32( stream, to, toCount );
		break;	
        default:
                DPRINTK( "ep93xx_i2s copy to user unsupported audio format\n" );
		break;
        }
    
    }
    return ret;
}

static __inline__ int copy_from_user_S24_LE( audio_stream_t *stream, const char *from, int toCount )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;

    unsigned int * user_buffer = (unsigned int *)from;
    unsigned int data;
	
    int toCount0 = toCount;
    count = 8 * stream->dma_num_channels;
	
    while (toCount > 0){

	__get_user(data, user_buffer++);
	*dma_buffer_0++ = (unsigned int)data;
	__get_user(data, user_buffer++);
	*dma_buffer_0++ = (unsigned int)data;

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = (unsigned int)data;
	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = (unsigned int)data;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = (unsigned int)data;
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = (unsigned int)data;
        }
	toCount -= count;
    }
    return toCount0 / 2;
}

static __inline__ int copy_from_user_U24_LE( audio_stream_t *stream, const char *from, int toCount )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    unsigned int * user_buffer = (unsigned int *)from;
    unsigned int data;
	
    int toCount0 = toCount;
    count = 8 * stream->dma_num_channels;
	
    while (toCount > 0){

	__get_user(data, user_buffer++);
	*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);
	__get_user(data, user_buffer++);
	*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
	}
	toCount -= count;
    }
    return toCount0 / 2;
}

static __inline__ int copy_from_user_S16_LE( audio_stream_t *stream, const char *from, int toCount )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    unsigned short *user_buffer = (unsigned short *)from;
    unsigned short data;
	
    int toCount0 = toCount;
    int count;
    count = 8 * stream->dma_num_channels;
	
    while (toCount > 0){
    
	__get_user(data, user_buffer++);
	*dma_buffer_0++ = data;
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0++ = data;
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
    	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = data;
    	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = data;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = data;
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = data;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
    	return toCount0 / 4;	
    }
    return toCount0 / 2;
}

static __inline__ int copy_from_user_U16_LE( audio_stream_t *stream, const char *from, int toCount )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    unsigned short * user_buffer = (unsigned short *)from;
    unsigned short data;
	
    int toCount0 = toCount;
    count = 8 * stream->dma_num_channels;
	
    while (toCount > 0){
    
	__get_user(data, user_buffer++);
	*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
    	    __get_user(data, user_buffer++);
            *dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
	    __get_user(data, user_buffer++);
    	    *dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
        return toCount0 / 4;
    }
    return toCount0 / 2;
}

static __inline__ int copy_from_user_S8( audio_stream_t *stream, const char *from, int toCount )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    unsigned char * user_buffer = (unsigned char *)from;
    unsigned char data;
	
    int toCount0 = toCount;
    count = 8 * stream->dma_num_channels;

    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;

    while (toCount > 0){
	__get_user(data, user_buffer++);
	*dma_buffer_0 = data;
	dma_buffer_0 += 4;
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0 = data;
	dma_buffer_0 += 4;

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1 = data;
            dma_buffer_1 += 4;
	    __get_user(data, user_buffer++);
            *dma_buffer_1 = data;
	    dma_buffer_1 += 4;
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2 = data;
	    dma_buffer_2 += 4;
	    __get_user(data, user_buffer++);
    	    *dma_buffer_2 = data;
            dma_buffer_2 += 4;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
    	return toCount0 / 8;
    }
    return toCount0 / 4;
}

static __inline__ int copy_from_user_U8( audio_stream_t *stream, const char *from, int toCount )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    unsigned char *user_buffer = (unsigned char *)from;
    unsigned char data;
	
    int toCount0 = toCount;
    count = 8 * stream->dma_num_channels;
	
    dma_buffer_0 ++;
    dma_buffer_1 ++;
    dma_buffer_2 ++;
			
    while (toCount > 0){

	__get_user(data, user_buffer++);
	*dma_buffer_0 = ((unsigned char)data ^ 0x80);
	dma_buffer_0 += 4;
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0 = ((unsigned char)data ^ 0x80);
	dma_buffer_0 += 4;
        
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1 = ((unsigned char)data ^ 0x80);
            dma_buffer_1 += 4;
	    __get_user(data, user_buffer++);
            *dma_buffer_1 = ((unsigned char)data ^ 0x80);
            dma_buffer_1 += 4;
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2 = ((unsigned char)data ^ 0x80);
    	    dma_buffer_2 += 4;
	    __get_user(data, user_buffer++);
    	    *dma_buffer_2 = ((unsigned char)data ^ 0x80);
            dma_buffer_2 += 4;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
    	return toCount0 / 8;
    }
    return toCount0 / 4;
}

static __inline__ int copy_from_user_S16_LE_CM(	audio_stream_t *stream,	const char *from, int toCount )
{
    unsigned int *dma_buffer_0 = (int *)stream->hwbuf[0];
    unsigned int *dma_buffer_1 = (int *)stream->hwbuf[1];
    unsigned int *dma_buffer_2 = (int *)stream->hwbuf[2];
    unsigned short *user_buffer = (unsigned short *)from;
    short data;
    unsigned int val;	
    int toCount0 = toCount;
    int count;
    count = 4 * stream->dma_num_channels;

	//printk("count=%x tocount\n",count,toCount);	
    while (toCount > 0){
    
	__get_user(data, user_buffer++);
	//*dma_buffer_0++ = data;
	val = (unsigned int)data & 0x0000ffff;
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
        }
	*dma_buffer_0++ = ((unsigned int)data << 16) | val;
	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
    	    __get_user(data, user_buffer++);
	    //*dma_buffer_1++ = data;
	    val = (unsigned int)data & 0x0000ffff;
    	    __get_user(data, user_buffer++);
	    *dma_buffer_1++ = ((unsigned int)data << 16) | val;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    //*dma_buffer_2++ = data;
	    val = (unsigned int)data & 0x0000ffff;
	    __get_user(data, user_buffer++);
	    *dma_buffer_2++ = ((unsigned int)data << 16) | val;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
        return toCount0 /2 ;
    }
    
    return toCount0 ;
}

static __inline__ int copy_from_user_U16_LE_CM( audio_stream_t *stream, const char *from, int toCount )
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    unsigned short * user_buffer = (unsigned short *)from;
    unsigned short data;
    unsigned int val;	
    int toCount0 = toCount;
    count = 4 * stream->dma_num_channels;
	
    while (toCount > 0){
    
	__get_user(data, user_buffer++);
	//*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);
	val = (unsigned int)data & 0x0000ffff;
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
        }
	//*dma_buffer_0++ = ((unsigned int)data ^ 0x8000);
        *dma_buffer_0++ = (((unsigned int)data << 16) | val) ^ 0x80008000;

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    //*dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
	    val = (unsigned int)data & 0x0000ffff;
    	    __get_user(data, user_buffer++);
            //*dma_buffer_1++ = ((unsigned int)data ^ 0x8000);
            *dma_buffer_1++ = (((unsigned int)data << 16) | val) ^ 0x80008000;
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    //*dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
	    val = (unsigned int)data & 0x0000ffff;
	    __get_user(data, user_buffer++);
    	    //*dma_buffer_2++ = ((unsigned int)data ^ 0x8000);
    	    *dma_buffer_2++ = (((unsigned int)data << 16) | val) ^ 0x80008000;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
        return toCount0/2;
    }
    return toCount0 ;
}

static __inline__ int copy_from_user_S8_CM( audio_stream_t *stream, const char *from, int toCount )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
    char *dma_buffer_1 = (char *)stream->hwbuf[1];
    char *dma_buffer_2 = (char *)stream->hwbuf[2];
    int count;
    unsigned char * user_buffer = (unsigned char *)from;
    unsigned char data;	
    int toCount0 = toCount;
    count = 4 * stream->dma_num_channels;

    dma_buffer_0++;
    dma_buffer_1++;
    dma_buffer_2++;

    while (toCount > 0){
	__get_user(data, user_buffer++);
	*dma_buffer_0 = data;
	*(dma_buffer_0 +1 ) = 0;
	dma_buffer_0 += 2;
	
	if(stream->audio_num_channels == 2){
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0 = data;
	*(dma_buffer_0 +1 ) = 0;
	dma_buffer_0 += 2;

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1 = data;
	    dma_buffer_1 += 2;
	    __get_user(data, user_buffer++);
            *dma_buffer_1 = data;
            dma_buffer_1 += 2;
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2 = data;
	    dma_buffer_2 += 2;
	    __get_user(data, user_buffer++);
    	    *dma_buffer_2 = data;
    	    dma_buffer_2 += 2;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
        return toCount0 / 4;
    }
    
    return toCount0 / 2;
}

static __inline__ int copy_from_user_U8_CM( audio_stream_t *stream, const char *from, int toCount )
{
    unsigned char *dma_buffer_0 = (unsigned char *)stream->hwbuf[0];
    unsigned char *dma_buffer_1 = (unsigned char *)stream->hwbuf[1];
    unsigned char *dma_buffer_2 = (unsigned char *)stream->hwbuf[2];
    int count;
    unsigned char *user_buffer = (unsigned char *)from;
    unsigned char data;
	
    int toCount0 = toCount;
    count = 4 * stream->dma_num_channels;
	
    dma_buffer_0 ++;
    dma_buffer_1 ++;
    dma_buffer_2 ++;
			
    while (toCount > 0){

	__get_user(data, user_buffer++);
	*dma_buffer_0 = ((unsigned char)data ^ 0x80);
	*(dma_buffer_0 +1 ) = 0;
	dma_buffer_0 += 2;
	
	if(stream->audio_num_channels == 2){	
	    __get_user(data, user_buffer++);
	}
	*dma_buffer_0 = ((unsigned char)data ^ 0x80);
	*(dma_buffer_0 +1 ) = 0;
	dma_buffer_0 += 2;

	
        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_1 = ((unsigned char)data ^ 0x80);
	    dma_buffer_1 += 2;
	    __get_user(data, user_buffer++);
            *dma_buffer_1 = ((unsigned char)data ^ 0x80);
            dma_buffer_1 += 2;
	}
	
        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __get_user(data, user_buffer++);
	    *dma_buffer_2 = ((unsigned char)data ^ 0x80);
    	    dma_buffer_2 += 2;
	    __get_user(data, user_buffer++);
    	    *dma_buffer_2 = ((unsigned char)data ^ 0x80);
            dma_buffer_2 += 2;
	}
	toCount -= count;
    }
    
    if(stream->audio_num_channels == 1){
        return toCount0 / 4;
    }
    
    return toCount0 / 2;
}

static int copy_from_user_U32( audio_stream_t *stream, const char *from, int toCount )
{
    char *dma_buffer_0 = (char *)stream->hwbuf[0];
	
    if (copy_from_user( (char *)dma_buffer_0, from, toCount)) 
    {
	return -EFAULT;
    }

    return toCount;
    
}

/*
 * Returns negative for error
 * Returns # of bytes transferred out of the from buffer
 * for success.
 */
static __inline__ int copy_from_user_with_conversion( audio_stream_t *stream, const char *from, int toCount, int bCompactMode )
{
    int ret = 0;
//    DPRINTK("copy_from_user_with_conversion\n");	
    if( toCount == 0 ){
    	DPRINTK("ep93xx_i2s_copy_from_user_with_conversion - nothing to copy!\n");
    }

    if( bCompactMode == 1){
    	
    	switch( stream->audio_format ){

		case SNDRV_PCM_FORMAT_S8:
			DPRINTK("SNDRV_PCM_FORMAT_S8 CM\n");
			ret = copy_from_user_S8_CM( stream, from, toCount );
			break;
			
		case SNDRV_PCM_FORMAT_U8:
			DPRINTK("SNDRV_PCM_FORMAT_U8 CM\n");
			ret = copy_from_user_U8_CM( stream, from, toCount );
			break;

		case SNDRV_PCM_FORMAT_S16_LE:
			DPRINTK("SNDRV_PCM_FORMAT_S16_LE CM\n");
			ret = copy_from_user_S16_LE_CM( stream, from, toCount );
			break;
				
		case SNDRV_PCM_FORMAT_U16_LE:
			DPRINTK("SNDRV_PCM_FORMAT_U16_LE CM\n");
			ret = copy_from_user_U16_LE_CM( stream, from, toCount );
			break;

		case SNDRV_PCM_FORMAT_S24_LE:
			DPRINTK("SNDRV_PCM_FORMAT_S24_LE CM\n");
			//ret = copy_from_user_S24_LE( stream, from, toCount );
			//break;
		
		case SNDRV_PCM_FORMAT_U24_LE:
			DPRINTK("SNDRV_PCM_FORMAT_U24_LE CM\n");
			//ret = copy_from_user_U24_LE( stream, from, toCount );
			//break;
		case SNDRV_PCM_FORMAT_S32_LE:
			DPRINTK("SNDRV_PCM_FORMAT_S32_LE CM\n");
			//break;
        	default:
                	DPRINTK( "ep93xx_i2s copy from user unsupported audio format\n" );
			break;			
    	}
    }
    else{
        switch( stream->audio_format ){

	case SNDRV_PCM_FORMAT_S8:
		DPRINTK("SNDRV_PCM_FORMAT_S8\n");
		ret = copy_from_user_S8( stream, from, toCount );
		break;
			
	case SNDRV_PCM_FORMAT_U8:
		DPRINTK("SNDRV_PCM_FORMAT_U8\n");
		ret = copy_from_user_U8( stream, from, toCount );
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		DPRINTK("SNDRV_PCM_FORMAT_S16_LE\n");
		ret = copy_from_user_S16_LE( stream, from, toCount );
		break;
				
	case SNDRV_PCM_FORMAT_U16_LE:
		DPRINTK("SNDRV_PCM_FORMAT_U16_LE\n");
		ret = copy_from_user_U16_LE( stream, from, toCount );
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		DPRINTK("SNDRV_PCM_FORMAT_S24_LE\n");
		//ret = copy_from_user_S24_LE( stream, from, toCount );
		//break;
		
	case SNDRV_PCM_FORMAT_U24_LE:
		DPRINTK("SNDRV_PCM_FORMAT_U24_LE\n");
		//ret = copy_from_user_U24_LE( stream, from, toCount );
		//break;
		DPRINTK( "ep93xx_i2s copy from user unsupported audio format\n" );
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		DPRINTK("SNDRV_PCM_FORMAT_S32_LE\n");
		ret = copy_from_user_U32( stream, from, toCount );
		break;
        default:
                DPRINTK( "ep93xx_i2s copy from user unsupported audio format\n" );
		break;			
    	}
    }
	
    return ret;
}



/*
 *  For audio playback, we convert samples of arbitrary format to be 32 bit 
 *  for our hardware. We're scaling a user buffer to a dma buffer.  So when
 *  report byte counts, we scale them acording to the ratio of DMA sample
 *  size to user buffer sample size.  When we report # of DMA fragments,
 *  we don't scale that.  So:
 *
 *  Also adjust the size and number of dma fragments if sample size changed.
 *
 *  Input format       Input sample     Output sample size    ratio (out:in)
 *  bits   channels    size (bytes)       CM   non-CM          CM   non-CM
 *   8      stereo         2		   4      8            2:1   4:1
 *   16     stereo         4		   4      8            1:1   2:1
 *   24     stereo         6		   4      8             X    8:6 not a real case
 *
 */
static void snd_ep93xx_dma2usr_ratio( audio_stream_t * stream,int bCompactMode )
{
    unsigned int dma_sample_size, user_sample_size;
	
    if(bCompactMode == 1){	    	
	dma_sample_size = 4;	/* each stereo sample is 2 * 32 bits */    
    }    
    else{    	
    	dma_sample_size = 8;    
    }
	
    // If stereo 16 bit, user sample is 4 bytes.
    // If stereo  8 bit, user sample is 2 bytes.
    if(stream->audio_num_channels == 1){
    	user_sample_size = stream->audio_stream_bitwidth / 8;
    }
    else{
    	user_sample_size = stream->audio_stream_bitwidth / 4;
    }
	
    stream->dma2usr_ratio = dma_sample_size / user_sample_size;
}

/*---------------------------------------------------------------------------------------------*/

static int snd_ep93xx_dma_free(struct snd_pcm_substream *substream ){

    
    audio_state_t *state = substream->private_data;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
                              state->output_stream:state->input_stream;
    int i;
    DPRINTK("snd_ep93xx_dma_free - enter\n");

    for( i = 0 ; i < stream->dma_num_channels ;i++ ){
	ep93xx_dma_free( stream->dmahandles[i] );
    }
    DPRINTK("snd_ep93xx_dma_free - exit\n");
    return 0;	       
}

static int snd_ep93xx_dma_config(struct snd_pcm_substream *substream ){

    audio_state_t *state = substream->private_data;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
                               state->output_stream:state->input_stream;
    int i,err = 0;
	
    DPRINTK("snd_ep93xx_dma_config - enter\n");

    for( i = 0 ; i < stream->dma_num_channels ;i++ ){
    
        err = ep93xx_dma_request(&stream->dmahandles[i],
	                        stream->devicename,
	                        (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
				state->output_dma[i]:state->input_dma[i] );
        if (err){
	    printk("snd_ep93xx_dma_config - exit ERROR dma request failed\n");
	    return err;
        }
	err = ep93xx_dma_config( stream->dmahandles[i],
    				IGNORE_CHANNEL_ERROR,
				0,
				(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 
				snd_ep93xx_dma_tx_callback:snd_ep93xx_dma_rx_callback,
				(unsigned int)substream );
        if (err){
	    printk("snd_ep93xx_dma_config - exit ERROR dma request failed\n");
	    return err;
	}
    }

    DPRINTK("snd_ep93xx_dma_config - enter\n");
    return err;
}

static void snd_ep93xx_dma_start( audio_state_t * state, audio_stream_t * stream )
{
    int err,i;

    DPRINTK("snd_ep93xx_dma_start - enter\n");

    for(i = 0 ;i < stream->dma_num_channels;i++)
	err = ep93xx_dma_start( stream->dmahandles[i], 1,(unsigned int *) stream->dmahandles );
	
    stream->active = 1;
    
    DPRINTK("snd_ep93xx_dma_start - exit\n");
}

static void snd_ep93xx_dma_pause( audio_state_t * state, audio_stream_t * stream )
{
    int i;
 
    DPRINTK("snd_ep93xx_dma_pause - enter\n");

    for(i = 0 ;i < stream->dma_num_channels;i++)
	ep93xx_dma_pause( stream->dmahandles[i], 1,(unsigned int *)stream->dmahandles );

    stream->active = 0;
    DPRINTK("snd_ep93xx_dma_pause - exit\n");

}

static void snd_ep93xx_dma_flush( audio_state_t * state, audio_stream_t * stream ){

    int i;
    
    DPRINTK("snd_ep93xx_dma_flush - enter\n");
	
    for( i = 0 ; i < stream->dma_num_channels ; i++ )
	ep93xx_dma_flush( stream->dmahandles[i] );
	   
    DPRINTK("snd_ep93xx_dma_flush - exit\n");
}

static void snd_ep93xx_deallocate_buffers( struct snd_pcm_substream *substream, audio_stream_t *stream )
{
    int i;
    audio_channel_t *dma_chan;
    
    DPRINTK("snd_ep93xx_deallocate_buffers - enter\n");
    
    if( stream->dma_channels ){

        for(i = 0;i < stream->dma_num_channels;i++){

	    dma_chan = &stream->dma_channels[i];

	    if( dma_chan->area ){
	    	    
		if( dma_chan->audio_buffers ){

		    kfree(dma_chan->audio_buffers);
		    dma_chan->audio_buffers = NULL;
		    
		}

		kfree(dma_chan->area);
		dma_chan->area = NULL;
	    }    
	}
	kfree(stream->dma_channels);
	stream->dma_channels = NULL;
    }
    DPRINTK("snd_ep93xx_deallocate_buffers - exit\n");
}

static int snd_ep93xx_allocate_buffers( struct snd_pcm_substr *substream, audio_stream_t *stream)
{
    audio_channel_t *channel;
    unsigned int size,tmpsize,bufsize,bufextsize;
    int i,j;
    
        
    DPRINTK("snd_ep93xx_allocate_buffers - enter\n" );

    if (stream->dma_channels){
	printk("ep93xx_i2s  %s BUSY\n",__FUNCTION__);
        return -EBUSY;
    }
							       
    stream->dma_channels = (audio_channel_t *)kmalloc(sizeof(audio_channel_t) * stream->dma_num_channels , GFP_KERNEL);
	    
    if (!stream->dma_channels){
	printk(AUDIO_NAME ": unable to allocate dma_channels memory\n");
	return - ENOMEM;
    }
    
    size = ( stream->dmasize / stream->dma_num_channels ) * stream->dma2usr_ratio; 

    for( i = 0; i < stream->dma_num_channels;i++){
	channel = &stream->dma_channels[i];

	channel->area = kmalloc( size, GFP_DMA );
			
	if(!channel->area){
	    printk(AUDIO_NAME ": unable to allocate audio memory\n");
	    return -ENOMEM;
	}	
	channel->bytes = size;
	channel->addr = __virt_to_phys((int) channel->area);
        memset( channel->area, 0, channel->bytes );

	bufsize = ( stream->fragsize / stream->dma_num_channels ) * stream->dma2usr_ratio;
	channel->audio_buff_count = size / bufsize;
	bufextsize = size % bufsize;

	if( bufextsize > 0 ){
	    channel->audio_buff_count++;
	}
	
	channel->audio_buffers = (audio_buf_t *)kmalloc(sizeof(audio_buf_t) * channel->audio_buff_count , GFP_KERNEL);
		    
	if (!channel->audio_buffers){
	    printk(AUDIO_NAME ": unable to allocate audio memory\n ");
	    return -ENOMEM;
	}

	tmpsize = size;

	for( j = 0; j < channel->audio_buff_count; j++){

	    channel->audio_buffers[j].dma_addr = channel->addr + j * bufsize;		

	    if( tmpsize >= bufsize ){
		tmpsize -= bufsize;
		channel->audio_buffers[j].bytes = bufsize;
		channel->audio_buffers[j].reportedbytes = bufsize / stream->dma2usr_ratio; 
	    }
	    else{
                channel->audio_buffers[j].bytes = bufextsize;
                channel->audio_buffers[j].reportedbytes = bufextsize / stream->dma2usr_ratio;
	    }
	}								
    }

    DPRINTK("snd_ep93xx_allocate_buffers -- exit SUCCESS\n" );
    return 0;
}

/*
 * DMA callback functions
 */
 
static void snd_ep93xx_dma_tx_callback(	ep93xx_dma_int_t DMAInt, ep93xx_dma_dev_t device, unsigned int user_data )
{
    int handle;
    int i,chan;
    unsigned int buf_id;
	    		
	struct snd_pcm_substream *substream = (struct snd_pcm_substream *)user_data;
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    audio_stream_t *stream = state->output_stream;
    audio_buf_t *buf;

    switch( device )              
    {
	case DMATx_I2S3:
		    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S3\n");
	    	i = 2;
	    	break;

	case DMATx_I2S2:
		    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S2\n");
       	    i = 1;
	    	break;

	case DMATx_I2S1:
	default:
		    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S1\n");
       	    i = 0;
		    break;
    }
    
    if(stream->audio_num_channels == 1){
    	chan = 0;
    }
    else{
        chan = stream->audio_num_channels / 2 - 1;
    } 
    handle = stream->dmahandles[i];

    if(stream->stopped == 0){
		if( ep93xx_dma_remove_buffer( handle, &buf_id ) >= 0 ){

	    	buf = (audio_buf_t *)buf_id;
        	stream->bytecount += buf->reportedbytes;	
	    	ep93xx_dma_add_buffer( stream->dmahandles[i],
						   		  (unsigned int)buf->dma_addr,
				    			  0,
				    			  buf->bytes,
				    			  0,
				    			  (unsigned int) buf );
            if(chan == i)
	        	snd_pcm_period_elapsed(substream);
		}
    }
}


static void snd_ep93xx_dma_rx_callback(	ep93xx_dma_int_t DMAInt, ep93xx_dma_dev_t device,  unsigned int user_data )
{
    int handle,i,chan;
    unsigned int buf_id;
    audio_buf_t *buf;
		
    struct snd_pcm_substream *substream = (struct snd_pcm_substream *)user_data;
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    audio_stream_t *stream = state->input_stream;

    switch( device )
    {
	case DMARx_I2S3:
    	    DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S3\n");
	    	i = 2;
		    break;

    case DMARx_I2S2:
          DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S2\n");
		    i = 1;
	    	break;
	
	case DMARx_I2S1:
	default:
		    DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S1\n");
	    	i = 0;
	    	break;
    }
    
    if(stream->audio_num_channels == 1){
    	chan = 0;
    }
    else{
        chan = stream->audio_num_channels / 2 - 1;
    } 
    handle = stream->dmahandles[i];
    
    if( stream->stopped == 0 ){
	
        if( ep93xx_dma_remove_buffer( handle, &buf_id ) >= 0 ){

    	    buf = (audio_buf_t *)buf_id;
	    	stream->bytecount += buf->reportedbytes;
	    	ep93xx_dma_add_buffer( stream->dmahandles[i],
				    			  (unsigned int)buf->dma_addr,
				    			  0, 
				    			  buf->bytes,
				    			  0,
				    			  (unsigned int) buf );

            if( i == chan )
                snd_pcm_period_elapsed(substream);
		}
    } 
}

static int snd_ep93xx_release(struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *)substream->private_data;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
                             state->output_stream : state->input_stream;
    
    DPRINTK("snd_ep93xx_release - enter\n");

    down(&state->sem);
    stream->active = 0;
    stream->stopped = 0;
    snd_ep93xx_deallocate_buffers(substream, stream);
    up(&state->sem);

    DPRINTK("snd_ep93xx_release - exit\n");

    return 0;
}

static int ep93xx_ac97_pcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int r;
	int iTempMasterVol,iTempHeadphoneVol,iTempMonoVol,iTempRecordSelect;
        /*save the old mixer*/
      	iTempRecordSelect 	= peek(AC97_1A_RECORD_SELECT);
        iTempMasterVol		= peek( AC97_02_MASTER_VOL);
        iTempHeadphoneVol	= peek( AC97_04_HEADPHONE_VOL);
        iTempMonoVol		= peek( AC97_06_MONO_VOL);

	runtime->hw.channels_min = 1;
	runtime->hw.channels_max = 2;

	// [FALINUX]	
 	//ep93xx_audio_init();
	/*ep93xx_init_ac97_controller();*/

    /*reset the old output mixer*/
    poke( AC97_02_MASTER_VOL, iTempMasterVol);
    poke( AC97_04_HEADPHONE_VOL,iTempHeadphoneVol );
    poke( AC97_06_MONO_VOL, iTempMonoVol);
	poke( AC97_1A_RECORD_SELECT,iTempRecordSelect);
		
	r = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? AC97_RATES_FRONT_DAC : AC97_RATES_ADC;
	
	DPRINTK(" ep93xx_ac97_pcm_startup=%x\n",r);

	return 0;
}


static int snd_ep93xx_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hardware *params)
{
	DPRINTK("snd_ep93xx_pcm_hw_params - enter\n");
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static int snd_ep93xx_pcm_hw_free(struct snd_pcm_substream *substream)
{

	DPRINTK("snd_ep93xx_pcm_hw_free - enter\n");
	return snd_pcm_lib_free_pages(substream);
}

/*
 *snd_ep93xx_pcm_prepare: need to finish these functions as lower
 *chip_set_sample_format
 *chip_set_sample_rate
 *chip_set_channels
 *chip_set_dma_setup
 */

static int snd_ep93xx_pcm_prepare_playback( struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *) substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->output_stream;

    DPRINTK("snd_ep93xx_pcm_prepare_playback - enter\n");
    
    ep93xx_audio_disable(1);
    ep93xx_ac97_pcm_startup(substream);
	        
    snd_ep93xx_deallocate_buffers(substream,stream);
			
    //if(runtime->channels % 2 != 0)
    //	return -1;
		   	
    DPRINTK("The runtime item : \n");
    DPRINTK("runtime->dma_addr    = 0x%x\n", runtime->dma_addr);
    DPRINTK("runtime->dma_area    = 0x%x\n", runtime->dma_area);
    DPRINTK("runtime->dma_bytes   = %d\n",   runtime->dma_bytes);
    DPRINTK("runtime->frame_bits  = %d\n",   runtime->frame_bits);
    DPRINTK("runtime->buffer_size = %d\n",   runtime->buffer_size);
    DPRINTK("runtime->period_size = %d\n",   runtime->period_size);
    DPRINTK("runtime->periods     = %d\n",   runtime->periods);
    DPRINTK("runtime->rate        = %d\n",   runtime->rate);
    DPRINTK("runtime->format      = %d\n",   runtime->format);
    DPRINTK("runtime->channels    = %d\n",   runtime->channels);
	
    /* set requestd format when available */
    stream->audio_num_channels = runtime->channels;
    if(stream->audio_num_channels == 1){
    	stream->dma_num_channels = 1;
    }
    else{
    	stream->dma_num_channels = runtime->channels / 2;
    }

    stream->audio_channels_flag = CHANNEL_FRONT;
    if(stream->dma_num_channels == 2)
        stream->audio_channels_flag |= CHANNEL_REAR;
    if(stream->dma_num_channels == 3)
        stream->audio_channels_flag |= CHANNEL_REAR | CHANNEL_CENTER_LFE;
			    
    stream->dmasize = runtime->dma_bytes;
    stream->nbfrags = runtime->periods;
    stream->fragsize = frames_to_bytes (runtime, runtime->period_size);
    stream->bytecount = 0;

    if( !state->codec_set_by_capture ){
	state->codec_set_by_playback = 1;
	
	if( stream->audio_rate != runtime->rate ){
	    ep93xx_set_samplerate( runtime->rate,0 );
	}    
	//if( stream->audio_format != runtime->format ){
    	//    snd_ep93xx_i2s_init((stream->audio_stream_bitwidth == 24));
	//}
    }
    else{
        audio_stream_t *s = state->input_stream;
        if( runtime->format != s->audio_format)
    	    return -1;
	if( runtime->rate != s->audio_rate )
	    return -1;
    }
    
    stream->audio_format = runtime->format ;	
    ep93xx_set_hw_format(stream->audio_format,stream->audio_num_channels);
    
    
    stream->audio_rate = runtime->rate;
    audio_set_format( stream, runtime->format );
    snd_ep93xx_dma2usr_ratio( stream,state->bCompactMode );
	
    if( snd_ep93xx_allocate_buffers( substream, stream ) != 0 ){
        snd_ep93xx_deallocate_buffers( substream, stream );
        return -1;
    }
    
    ep93xx_audio_enable(1);
												 
    DPRINTK("snd_ep93xx_pcm_prepare_playback - exit\n");
    return 0;	
}

static int snd_ep93xx_pcm_prepare_capture( struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *) substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->input_stream;
    
    ep93xx_audio_disable(0);
    ep93xx_ac97_pcm_startup(substream);
	
    snd_ep93xx_deallocate_buffers(substream,stream);

    //if(runtime->channels % 2 != 0)
	//return -1;
		   		       
    DPRINTK("snd_ep93xx_pcm_prepare_capture - enter\n");
			
//    printk("The runtime item : \n");
//    printk("runtime->dma_addr    = 0x%x\n", runtime->dma_addr);
//    printk("runtime->dma_area    = 0x%x\n", runtime->dma_area);
//    printk("runtime->dma_bytes   = %d\n",   runtime->dma_bytes);
//    printk("runtime->frame_bits  = %d\n",   runtime->frame_bits);
//    printk("runtime->buffer_size = %d\n",   runtime->buffer_size);
//    printk("runtime->period_size = %d\n",   runtime->period_size);
//    printk("runtime->periods     = %d\n",   runtime->periods);
//    printk("runtime->rate        = %d\n",   runtime->rate);
//    printk("runtime->format      = %d\n",   runtime->format);
//    printk("runtime->channels    = %d\n",   runtime->channels);
	
    /* set requestd format when available */
    stream->audio_num_channels = runtime->channels;
    if(stream->audio_num_channels == 1){
    	stream->dma_num_channels = 1;
    }
    else{
    	stream->dma_num_channels = runtime->channels / 2;
    }

    stream->audio_channels_flag = CHANNEL_FRONT;
    if(stream->dma_num_channels == 2)
	stream->audio_channels_flag |= CHANNEL_REAR;
    if(stream->dma_num_channels == 3)
	stream->audio_channels_flag |= CHANNEL_REAR | CHANNEL_CENTER_LFE;
			    
    stream->dmasize = runtime->dma_bytes;
    stream->nbfrags = runtime->periods;
    stream->fragsize = frames_to_bytes (runtime, runtime->period_size);
    stream->bytecount = 0;

    if( !state->codec_set_by_playback ){
	state->codec_set_by_capture = 1;
	
	/*rate*/
	if( stream->audio_rate != runtime->rate ){
    	    ep93xx_set_samplerate( runtime->rate,1 );
	}
	
	/*mixer*/
	ep93xx_set_recsource(SOUND_MASK_MIC|SOUND_MASK_LINE1 | SOUND_MASK_LINE);
	poke( AC97_1C_RECORD_GAIN, 0);
	
	/*format*/	
        //if( stream->audio_format != runtime->format ){
    	//    snd_ep93xx_i2s_init((stream->audio_stream_bitwidth == 24));
	//}
    }
    else{
        audio_stream_t *s = state->output_stream;
        if( runtime->format != s->audio_format)
    	    return -1;
	if( runtime->rate != s->audio_rate )
    	    return -1;
    }
    
    stream->audio_format = runtime->format ;	
    ep93xx_set_hw_format(stream->audio_format,stream->audio_num_channels);
    
    
    stream->audio_rate = runtime->rate;
    audio_set_format( stream, runtime->format );
    snd_ep93xx_dma2usr_ratio( stream,state->bCompactMode );

    if( snd_ep93xx_allocate_buffers( substream, stream ) != 0 ){
        snd_ep93xx_deallocate_buffers( substream, stream );
	return -1;
    }
    
    ep93xx_audio_enable(0);
												 
    DPRINTK("snd_ep93xx_pcm_prepare_capture - exit\n");
    return 0;	
}
/*
 *start/stop/pause dma translate
 */
static int snd_ep93xx_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{	
    audio_state_t  *state = (audio_state_t *)substream->private_data;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
				state->output_stream:state->input_stream;
    audio_buf_t *buf; 
    audio_channel_t *dma_channel;
    int i,count,ret = 0;
    unsigned long flags;

    DPRINTK("snd_ep93xx_pcm_triger %d - enter \n",cmd);
					
    switch (cmd){
    
	case SNDRV_PCM_TRIGGER_START:
				
	    snd_ep93xx_dma_config( substream );

            stream->stopped = 0;
		
            if( !stream->active && !stream->stopped ){
	        stream->active = 1;
    		snd_ep93xx_dma_start( state, stream );
            }

            local_irq_save(flags);
    
	    for (i = 0; i < stream->dma_num_channels; i++){
		dma_channel = &stream->dma_channels[i];

		for(count = 0 ;count < dma_channel->audio_buff_count; count++){
		
		    buf = &dma_channel->audio_buffers[count];																	
    		    ep93xx_dma_add_buffer( stream->dmahandles[i],
					    (unsigned int)buf->dma_addr,
		            		    0,
		                	    buf->bytes,
					    0,
					    (unsigned int) buf );
		}
	    }	
						
	    local_irq_restore(flags);
	    break;

	case SNDRV_PCM_TRIGGER_STOP:
	    stream->stopped = 1;
	    snd_ep93xx_dma_pause( state, stream );
	    snd_ep93xx_dma_flush( state, stream );
	    snd_ep93xx_dma_free( substream );
	    break;

	case SNDRV_PCM_TRIGGER_SUSPEND:
	    break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	    break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	    break;

	    default:
	    ret = -EINVAL;
    }
    DPRINTK("snd_ep93xx_pcm_triger %d - exit \n",cmd);
    return ret;
}

static snd_pcm_uframes_t snd_ep93xx_pcm_pointer_playback(struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *)(substream->private_data);
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->output_stream;
    snd_pcm_uframes_t pointer = 0;

    pointer = bytes_to_frames( runtime,stream->bytecount );

    if (pointer >= runtime->buffer_size){
	pointer = 0;
	stream->bytecount = 0;
    }
			    
    DPRINTK("snd_ep93xx_pcm_pointer_playback - exit\n");
    return pointer;
}

static snd_pcm_uframes_t snd_ep93xx_pcm_pointer_capture(struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *)(substream->private_data);
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->input_stream;
    snd_pcm_uframes_t pointer = 0;
	
    pointer = bytes_to_frames( runtime,stream->bytecount );
	
    if (pointer >= runtime->buffer_size){
	pointer = 0;
	stream->bytecount = 0;
    }
	
    DPRINTK("snd_ep93xx_pcm_pointer_capture - exit\n");
    return pointer;
}

static int snd_ep93xx_pcm_open(struct snd_pcm_substream *substream)
{
    audio_state_t *state = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
                                state->output_stream:state->input_stream;

    DPRINTK("snd_ep93xx_pcm_open - enter\n");

    down(&state->sem);
            
    runtime->hw = ep93xx_ac97_pcm_hardware;

    stream->dma_num_channels = AUDIO_DEFAULT_DMACHANNELS;
    stream->dma_channels = NULL;
    stream->audio_rate = 0;
    stream->audio_stream_bitwidth = 0;
    	    
    up(&state->sem);
	
    DPRINTK("snd_ep93xx_pcm_open - exit\n");
    return 0;		
}

/*
 *free the HW dma channel
 *free the HW dma buffer
 *free the Hw dma decrotion using function :kfree
 */
static int snd_ep93xx_pcm_close(struct snd_pcm_substream *substream)
{
    audio_state_t *state = (audio_state_t *)(substream->private_data);

    DPRINTK("snd_ep93xx_pcm_close - enter\n");

    snd_ep93xx_release(substream);

    if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
	state->codec_set_by_playback = 0;
    else
	state->codec_set_by_capture = 0;

    DPRINTK("snd_ep93xx_pcm_close - exit\n");
    return 0;
}

static int snd_ep93xx_pcm_copy_playback(struct snd_pcm_substream * substream,int channel, 
				snd_pcm_uframes_t pos,void __user *src, snd_pcm_uframes_t count)
{

    audio_state_t *state = (audio_state_t *)substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->output_stream ;
    audio_channel_t *dma_channel;
    int i;
    int tocount = frames_to_bytes(runtime,count);
    
    for( i = 0; i < stream->dma_num_channels; i++ ){

	dma_channel = &stream->dma_channels[i];	
	stream->hwbuf[i] = dma_channel->area + ( frames_to_bytes(runtime,pos) * stream->dma2usr_ratio / stream->dma_num_channels );
    
    }

    if(copy_from_user_with_conversion(stream ,(const char*)src,(tocount * stream->dma2usr_ratio),state->bCompactMode) <=0 ){
	DPRINTK(KERN_ERR "copy_from_user_with_conversion() failed\n");
	return -EFAULT;
    }
					
    DPRINTK("snd_ep93xx_pcm_copy_playback - exit\n");
    return 0;
}


static int snd_ep93xx_pcm_copy_capture(struct snd_pcm_substream * substream,int channel, 
				snd_pcm_uframes_t pos,void __user *src, snd_pcm_uframes_t count)
{
    audio_state_t *state = (audio_state_t *)substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
    audio_stream_t *stream = state->input_stream ;
    audio_channel_t *dma_channel;
    int i;
		       
    int tocount = frames_to_bytes(runtime,count);
			   
    for( i = 0; i < stream->dma_num_channels; i++ ){
  		dma_channel = &stream->dma_channels[i];
		stream->hwbuf[i] = dma_channel->area + ( frames_to_bytes(runtime,pos) * stream->dma2usr_ratio / stream->dma_num_channels );
    }

    if(copy_to_user_with_conversion(stream,(const char*)src,tocount,state->bCompactMode) <=0 ){

	DPRINTK(KERN_ERR "copy_to_user_with_conversion() failed\n");
	return -EFAULT;
    }
										       
    DPRINTK("snd_ep93xx_pcm_copy_capture - exit\n");
    return 0;
}

/*----------------------------------------------------------------------------------*/
static unsigned short ep93xx_ac97_read(struct snd_ac97 *ac97, unsigned short reg)
{
	int val = -1;
	/*volatile u32 *reg_addr;*/

	DPRINTK(" number of codec:%x reg=%x\n",ac97->num,reg);
	val=peek(reg);
	if(val==-1){
		printk(KERN_ERR "%s: read error (ac97_reg=%d )val=%x\n",
				__FUNCTION__, reg, val);
		return 0;
	}

	return val;
}

static void ep93xx_ac97_write(struct snd_ac97 *ac97, unsigned short reg, unsigned short val)
{
	/*volatile u32 *reg_addr;*/
	int ret;

	DPRINTK(" number of codec:%x rge=%x val=%x\n",ac97->num,reg,val);
	ret=poke(reg, val);
	if(ret!=0){
		printk(KERN_ERR "%s: write error (ac97_reg=%d val=%x)\n",
				__FUNCTION__, reg, val);
	}

}

static void ep93xx_ac97_reset(struct snd_ac97 *ac97)
{

	DPRINTK(" ep93xx_ac97_reset\n");
	ep93xx_audio_init();

}

static struct snd_ac97_bus_ops ep93xx_ac97_ops = {
	.read	= ep93xx_ac97_read,
	.write	= ep93xx_ac97_write,
	.reset	= ep93xx_ac97_reset,
};

static struct snd_pcm 	*ep93xx_ac97_pcm;
static struct snd_ac97  *ep93xx_ac97_ac97;																					     

static struct snd_pcm_ops snd_ep93xx_pcm_playback_ops = {
	.open		= snd_ep93xx_pcm_open,
	.close		= snd_ep93xx_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= snd_ep93xx_pcm_hw_params,
	.hw_free	= snd_ep93xx_pcm_hw_free,
	.prepare	= snd_ep93xx_pcm_prepare_playback,
	.trigger	= snd_ep93xx_pcm_trigger,
	.pointer	= snd_ep93xx_pcm_pointer_playback,
	.copy		= snd_ep93xx_pcm_copy_playback,
	
};

static struct snd_pcm_ops snd_ep93xx_pcm_capture_ops = {
	.open		= snd_ep93xx_pcm_open,
	.close		= snd_ep93xx_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= snd_ep93xx_pcm_hw_params,
	.hw_free	= snd_ep93xx_pcm_hw_free,
	.prepare	= snd_ep93xx_pcm_prepare_capture,
	.trigger	= snd_ep93xx_pcm_trigger,
	.pointer	= snd_ep93xx_pcm_pointer_capture,
	.copy 		= snd_ep93xx_pcm_copy_capture,
};

/*--------------------------------------------------------------------------*/


static int snd_ep93xx_pcm_new(struct snd_card *card, audio_state_t *state, struct snd_pcm **rpcm)
{
    struct snd_pcm *pcm;
    int play = state->output_stream? 1 : 0;/*SNDRV_PCM_STREAM_PLAYBACK*/
    int capt = state->input_stream ? 1 : 0;/*SNDRV_PCM_STREAM_CAPTURE*/
    int ret = 0;

    DPRINTK("snd_ep93xx_pcm_new - enter\n");
	
    /* Register the new pcm device interface */
    ret = snd_pcm_new(card, "EP93xx-AC97-PCM", 0, play, capt, &pcm);

    if (ret){
	DPRINTK("%s--%x:card=%x,play=%x,capt=%x,&pcm=%x\n",__FUNCTION__,ret,(int)card,play,capt,(int)pcm);
	goto out;
    }

    /* allocate the pcm(DMA) memory */
    ret = snd_pcm_lib_preallocate_pages_for_all(pcm, /*SNDRV_DMA_TYPE_DEV,0,*/SNDRV_DMA_TYPE_CONTINUOUS,snd_dma_continuous_data(GFP_KERNEL),128*1024,128*1024);

    DPRINTK("The substream item : \n");
    DPRINTK("pcm->streams[0].substream->dma_buffer.addr  = 0x%x\n", pcm->streams[0].substream->dma_buffer.addr);
    DPRINTK("pcm->streams[0].substream->dma_buffer.area  = 0x%x\n", pcm->streams[0].substream->dma_buffer.area);
    DPRINTK("pcm->streams[0].substream->dma_buffer.bytes = 0x%x\n", pcm->streams[0].substream->dma_buffer.bytes);
    DPRINTK("pcm->streams[1].substream->dma_buffer.addr  = 0x%x\n", pcm->streams[1].substream->dma_buffer.addr);
    DPRINTK("pcm->streams[1].substream->dma_buffer.area  = 0x%x\n", pcm->streams[1].substream->dma_buffer.area);
    DPRINTK("pcm->streams[1].substream->dma_buffer.bytes = 0x%x\n", pcm->streams[1].substream->dma_buffer.bytes);	

    pcm->private_data = state;
	
    /* seem to free the pcm data struct-->self dma buffer */
    pcm->private_free = (void*) snd_pcm_lib_preallocate_free_for_all;

    /* alsa pcm ops setting for SNDRV_PCM_STREAM_PLAYBACK */
    if (play) {
	int stream = SNDRV_PCM_STREAM_PLAYBACK;
	snd_pcm_set_ops(pcm, stream, &snd_ep93xx_pcm_playback_ops);
    }

    /* alsa pcm ops setting for SNDRV_PCM_STREAM_CAPTURE */	
    if (capt) {
	int stream = SNDRV_PCM_STREAM_CAPTURE;
	snd_pcm_set_ops(pcm, stream, &snd_ep93xx_pcm_capture_ops);
    }

    if (rpcm)
	*rpcm = pcm;
    DPRINTK("snd_ep93xx_pcm_new - exit\n");
out:
    return ret;
}

#ifdef CONFIG_PM

int ep93xx_ac97_do_suspend(struct snd_card *card, unsigned int state)
{
	if (card->power_state != SNDRV_CTL_POWER_D3cold) {
		snd_pcm_suspend_all(ep93xx_ac97_pcm);
		snd_ac97_suspend(ep93xx_ac97_ac97);
		snd_power_change_state(card, SNDRV_CTL_POWER_D3cold);
	}

	return 0;
}

int ep93xx_ac97_do_resume(struct snd_card *card, unsigned int state)
{
	if (card->power_state != SNDRV_CTL_POWER_D0) {

		snd_ac97_resume(ep93xx_ac97_ac97);
		snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	}

	return 0;
}

int ep93xx_ac97_suspend(struct platform_device *_dev, u32 state, u32 level)
{
	struct snd_card *card = platform_get_drvdata(_dev);
	int ret = 0;

	if (card && level == SUSPEND_DISABLE)
		ret = ep93xx_ac97_do_suspend(card, SNDRV_CTL_POWER_D3cold);

	return ret;
}

int ep93xx_ac97_resume(struct platform_device *_dev, u32 level)
{
	struct snd_card *card = platform_get_drvdata(_dev);
	int ret = 0;

	if (card && level == RESUME_ENABLE)
		ret = ep93xx_ac97_do_resume(card, SNDRV_CTL_POWER_D0);

	return ret;
}

#else
/*
#define ep93xx_ac97_do_suspend		NULL
#define ep93xx_ac97_do_resume		NULL
#define ep93xx_ac97_suspend		NULL
#define ep93xx_ac97_resume		NULL
*/

int ep93xx_ac97_do_suspend(struct snd_card *card, unsigned int state)
{                                                                                                                            
        return 0;
}
                                                                                                                             
int ep93xx_ac97_do_resume(struct snd_card *card, unsigned int state)
{                                                                                                                     
        return 0;
}

int ep93xx_ac97_resume(struct platform_device *_dev, u32 level)
{
        struct snd_card *card = platform_get_drvdata(_dev);
        int ret = 0;
                                                                                                                             
        //if (card && level == RESUME_ENABLE)
                ret = ep93xx_ac97_do_resume(card, SNDRV_CTL_POWER_D0);
                                                                                                                             
        return ret;
}

int ep93xx_ac97_suspend(struct platform_device *_dev, u32 state, u32 level)
{
        struct snd_card *card = platform_get_drvdata(_dev);
        int ret = 0;
                                                                                                                             
        //if (card && level == SUSPEND_DISABLE)
                ret = ep93xx_ac97_do_suspend(card, SNDRV_CTL_POWER_D3cold);
                                                                                                                             
        return ret;
}

#endif



/* module init & exit */
static int ep93xx_ac97_probe(struct platform_device *dev)
{
    struct snd_card *card;
    struct snd_ac97_bus *ac97_bus;
    struct snd_ac97_template ac97_template;
    int err = -ENOMEM;
    struct resource *res = NULL;
            
    printk("snd_ep93xx_probe - enter\n");
	
    /* Enable audio early on, give the DAC time to come up. */ 
    res = platform_get_resource( dev, IORESOURCE_MEM, 0);

    if(!res) {
	printk("error : platform_get_resource \n");
        return -ENODEV;
    }

    if (!request_mem_region(res->start,res->end - res->start + 1, "snd-ac97-cs4202" )){
    	printk("error : request_mem_region\n");
        return -EBUSY;
    }
									    
    /*enable ac97 codec*/
    ep93xx_audio_init();
			    
    /* register the soundcard */
    card = snd_card_new(SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			    THIS_MODULE, 0);
    if (!card){
	printk("AC97: snd_card_new error\n");
	goto error;
    }
    
    card->dev = &dev->dev;
    /*regist the new pcm device*/
    err = snd_ep93xx_pcm_new(card, &audio_state, &ep93xx_ac97_pcm);
    if (err){
	printk("AC97: ep93xx_ac97_pcm_new error\n");
	goto error;
    }
    if (card == NULL) {
	DPRINTK(KERN_ERR "snd_card_new() failed\n");
	goto error;
    }

    /*driver name*/
    strcpy(card->driver, "CS4202A");
    strcpy(card->shortname, "Cirrus Logic AC97 Audio ");
    strcpy(card->longname, "Cirrus Logic AC97 Audio with CS4202A");

    /*regist the new ac97 device*/
    err = snd_ac97_bus(card, 0, &ep93xx_ac97_ops, NULL, &ac97_bus);
    if (err){
	printk("AC97: snd_ac97_bus error\n");
	goto error;
    }
    
    memset(&ac97_template, 0, sizeof(ac97_template));
    err = snd_ac97_mixer(ac97_bus, &ac97_template, &ep93xx_ac97_ac97);
    if (err){
	printk("AC97: snd_ac97_mixer error\n");
	goto error;
    }
   
    /**/
    ep93xx_audio_init();	 
    /*setting the card device callback*/
    //err = snd_card_set_pm_callback(card, ep93xx_ac97_do_suspend,ep93xx_ac97_do_resume, (void*)NULL);
    //if(err != 0){
    //	printk("snd_card_set_pm_callback error\n");
    //}

    /*regist the new CARD device*/
    err = snd_card_register(card);
    if (err == 0) {
	printk( KERN_INFO "Cirrus Logic ep93xx ac97 audio initialized\n" );
	platform_set_drvdata(dev,card);
	DPRINTK("snd_ep93xx_probe - exit\n");
    	return 0;
    }

error:
    snd_card_free(card);
    printk("snd_ep93xx_probe - error\n");
    return err;

return 0;
}

static int ep93xx_ac97_remove(struct platform_device *dev)
{
    struct resource *res;
    struct snd_card *card = platform_get_drvdata(dev);

    res = platform_get_resource( dev, IORESOURCE_MEM, 0);
    release_mem_region(res->start, res->end - res->start + 1);
	
    DPRINTK("snd_ep93xx_ac97_remove - enter\n");
    
    if (card) {
	snd_card_free(card);
	platform_set_drvdata(dev, NULL);
    }
    DPRINTK("snd_ep93xx_remove - exit\n");

return 0;
}


static struct platform_driver ep93xx_ac97_driver = {
	.probe		= ep93xx_ac97_probe,
	.remove		= ep93xx_ac97_remove,
	.suspend	= ep93xx_ac97_suspend,
	.resume		= ep93xx_ac97_resume,
	.driver		= {
		.name	= "ep93xx-ac97",
	},
};						
						

static int __init ep93xx_ac97_init(void)
{
    int ret;
    
    DPRINTK(KERN_INFO "%s: version %s\n", DRIVER_DESC, DRIVER_VERSION);
    DPRINTK("snd_ep93xx_AC97_init - enter\n");	
    ret = platform_driver_register(&ep93xx_ac97_driver);
    DPRINTK("snd_ep93xx_AC97_init - exit\n");
    return ret;										
}

static void __exit ep93xx_ac97_exit(void)
{
    DPRINTK("ep93xx_ac97_exit  - enter\n");
    return platform_driver_unregister(&ep93xx_ac97_driver);
}

module_init(ep93xx_ac97_init);
module_exit(ep93xx_ac97_exit);

MODULE_DESCRIPTION("Cirrus Logic audio module");
MODULE_LICENSE("GPL");
