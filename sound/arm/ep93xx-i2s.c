/*
 * linux/sound/arm/ep93xx-i2s.c -- ALSA PCM interface for the edb93xx i2s audio
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/initval.h>

#include <asm/irq.h>
#include <asm/semaphore.h>
#include <asm/hardware.h>
#include <asm/io.h>                                                                                                                             
#include <asm/arch/dma.h>
#include <asm/arch/regs_i2s.h>
#include <asm/arch/ssp.h>

#include "ep93xx-i2s.h"


//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

#define WL16 	0
#define WL24	1

#define AUDIO_NAME              	"ep93xx-i2s"
#define AUDIO_SAMPLE_RATE_DEFAULT       44100
#define AUDIO_DEFAULT_VOLUME            0
#define AUDIO_MAX_VOLUME	        181
#define AUDIO_DEFAULT_DMACHANNELS       3
#define PLAYBACK_DEFAULT_DMACHANNELS    3
#define CAPTURE_DEFAULT_DMACHANNELS     3

#define CHANNEL_FRONT			(1<<0)
#define CHANNEL_REAR                   	(1<<1)
#define CHANNEL_CENTER_LFE              (1<<2)

#ifdef CONFIG_CODEC_CS4271
static int 	mute = 0;
#endif

static int 	index = SNDRV_DEFAULT_IDX1;          /* Index 0-MAX */
static char 	*id = SNDRV_DEFAULT_STR1;            /* ID for this card */
static int 	SSP_Handle = -1;

static void snd_ep93xx_dma_tx_callback( ep93xx_dma_int_t DMAInt,
					ep93xx_dma_dev_t device,
					unsigned int user_data);
static void snd_ep93xx_dma_rx_callback( ep93xx_dma_int_t DMAInt,
					ep93xx_dma_dev_t device,
					unsigned int user_data);

static const snd_pcm_hardware_t ep93xx_i2s_pcm_hardware = {


    .info		= ( SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_PAUSE  ),
    .formats		= ( SNDRV_PCM_FMTBIT_U8     | SNDRV_PCM_FMTBIT_S8     |
			    SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S16_LE |
			    SNDRV_PCM_FMTBIT_U16_BE | SNDRV_PCM_FMTBIT_S16_BE |
			    SNDRV_PCM_FMTBIT_U24_LE | SNDRV_PCM_FMTBIT_S24_LE |
			    SNDRV_PCM_FMTBIT_U24_BE | SNDRV_PCM_FMTBIT_S24_BE ),
    .rates		= ( SNDRV_PCM_RATE_8000  | SNDRV_PCM_RATE_11025 |
			    SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |
			    SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000),
    .rate_min		= 8000,
    .rate_max		= 96000,
    .channels_min	= 1,/*2,*/
#ifdef CONFIG_CODEC_CS4271
    .channels_max	= 2,
#else//CONFIG_CODEC_CS4228A
    .channels_max	= 6,
#endif
					
    .period_bytes_min	= 1 * 1024,
    .period_bytes_max	= 32 * 1024,
    .periods_min	= 1,
    .periods_max	= 32,
    .buffer_bytes_max	= /*32*/128 * 1024,
    .fifo_size		= 0,
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
    .output_stream          = &output_stream,
    .output_dma[0]          = DMATx_I2S1,
    .output_id[0]           = "I2S_OUT1",
    .output_dma[1]          = DMATx_I2S2,
    .output_id[1]           = "I2S_OUT2",
    .output_dma[2]          = DMATx_I2S3,
    .output_id[2]           = "I2S_OUT3",
	       
	       	       
    .input_stream           = &input_stream,
    .input_dma[0]           = DMARx_I2S1,
    .input_id[0]            = "I2S_IN1",
    .input_dma[1]           = DMARx_I2S2,
    .input_id[1]            = "I2S_IN2",
    .input_dma[2]           = DMARx_I2S3,
    .input_id[2]            = "I2S_IN3",
	
    .sem                    = __SEMAPHORE_INIT(audio_state.sem,1),
    .codec_set_by_playback  = 0,
    .codec_set_by_capture   = 0,
};			
		
static snd_pcm_t *ep93xx_i2s_pcm;

typedef struct {
    ulong   ulTotalDiv;
    ulong   ulI2SDiv;
} DIV_TABLE;

static const DIV_TABLE I2SDivTable[] =
{
    {   6, SYSCON_I2SDIV_PDIV_2  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {   8, SYSCON_I2SDIV_PDIV_2  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  10, SYSCON_I2SDIV_PDIV_25 | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  12, SYSCON_I2SDIV_PDIV_3  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  15, SYSCON_I2SDIV_PDIV_25 | (  3 & SYSCON_I2SDIV_MDIV_MASK) },
    {  16, SYSCON_I2SDIV_PDIV_2  | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  18, SYSCON_I2SDIV_PDIV_3  | (  3 & SYSCON_I2SDIV_MDIV_MASK) },
    {  20, SYSCON_I2SDIV_PDIV_25 | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  24, SYSCON_I2SDIV_PDIV_3  | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  25, SYSCON_I2SDIV_PDIV_25 | (  5 & SYSCON_I2SDIV_MDIV_MASK) },
    {  28, SYSCON_I2SDIV_PDIV_2  | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  30, SYSCON_I2SDIV_PDIV_3  | (  5 & SYSCON_I2SDIV_MDIV_MASK) },
    {  32, SYSCON_I2SDIV_PDIV_2  | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  35, SYSCON_I2SDIV_PDIV_25 | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  36, SYSCON_I2SDIV_PDIV_3  | (  6 & SYSCON_I2SDIV_MDIV_MASK) },
    {  40, SYSCON_I2SDIV_PDIV_25 | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  42, SYSCON_I2SDIV_PDIV_3  | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  44, SYSCON_I2SDIV_PDIV_2  | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  45, SYSCON_I2SDIV_PDIV_25 | (  9 & SYSCON_I2SDIV_MDIV_MASK) },
    {  48, SYSCON_I2SDIV_PDIV_3  | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  50, SYSCON_I2SDIV_PDIV_25 | ( 10 & SYSCON_I2SDIV_MDIV_MASK) },
    {  52, SYSCON_I2SDIV_PDIV_2  | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  54, SYSCON_I2SDIV_PDIV_3  | (  9 & SYSCON_I2SDIV_MDIV_MASK) },
    {  55, SYSCON_I2SDIV_PDIV_25 | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  56, SYSCON_I2SDIV_PDIV_2  | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  60, SYSCON_I2SDIV_PDIV_3  | ( 10 & SYSCON_I2SDIV_MDIV_MASK) },
    {  64, SYSCON_I2SDIV_PDIV_2  | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    {  65, SYSCON_I2SDIV_PDIV_25 | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  66, SYSCON_I2SDIV_PDIV_3  | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  68, SYSCON_I2SDIV_PDIV_2  | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    {  70, SYSCON_I2SDIV_PDIV_25 | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  72, SYSCON_I2SDIV_PDIV_3  | ( 12 & SYSCON_I2SDIV_MDIV_MASK) },
    {  75, SYSCON_I2SDIV_PDIV_25 | ( 15 & SYSCON_I2SDIV_MDIV_MASK) },
    {  76, SYSCON_I2SDIV_PDIV_2  | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    {  78, SYSCON_I2SDIV_PDIV_3  | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  80, SYSCON_I2SDIV_PDIV_25 | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    {  84, SYSCON_I2SDIV_PDIV_3  | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  85, SYSCON_I2SDIV_PDIV_25 | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    {  88, SYSCON_I2SDIV_PDIV_2  | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    {  90, SYSCON_I2SDIV_PDIV_3  | ( 15 & SYSCON_I2SDIV_MDIV_MASK) },
    {  92, SYSCON_I2SDIV_PDIV_2  | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    {  95, SYSCON_I2SDIV_PDIV_25 | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    {  96, SYSCON_I2SDIV_PDIV_3  | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    { 100, SYSCON_I2SDIV_PDIV_25 | ( 20 & SYSCON_I2SDIV_MDIV_MASK) },
    { 102, SYSCON_I2SDIV_PDIV_3  | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    { 104, SYSCON_I2SDIV_PDIV_2  | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 105, SYSCON_I2SDIV_PDIV_25 | ( 21 & SYSCON_I2SDIV_MDIV_MASK) },
    { 108, SYSCON_I2SDIV_PDIV_3  | ( 18 & SYSCON_I2SDIV_MDIV_MASK) },
    { 110, SYSCON_I2SDIV_PDIV_25 | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    { 112, SYSCON_I2SDIV_PDIV_2  | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 114, SYSCON_I2SDIV_PDIV_3  | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    { 115, SYSCON_I2SDIV_PDIV_25 | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    { 116, SYSCON_I2SDIV_PDIV_2  | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 120, SYSCON_I2SDIV_PDIV_3  | ( 20 & SYSCON_I2SDIV_MDIV_MASK) },
    { 124, SYSCON_I2SDIV_PDIV_2  | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 125, SYSCON_I2SDIV_PDIV_25 | ( 25 & SYSCON_I2SDIV_MDIV_MASK) },
    { 126, SYSCON_I2SDIV_PDIV_3  | ( 21 & SYSCON_I2SDIV_MDIV_MASK) },
    { 128, SYSCON_I2SDIV_PDIV_2  | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 130, SYSCON_I2SDIV_PDIV_25 | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 132, SYSCON_I2SDIV_PDIV_3  | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    { 135, SYSCON_I2SDIV_PDIV_25 | ( 27 & SYSCON_I2SDIV_MDIV_MASK) },
    { 136, SYSCON_I2SDIV_PDIV_2  | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 138, SYSCON_I2SDIV_PDIV_3  | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    { 140, SYSCON_I2SDIV_PDIV_25 | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 144, SYSCON_I2SDIV_PDIV_3  | ( 24 & SYSCON_I2SDIV_MDIV_MASK) },
    { 145, SYSCON_I2SDIV_PDIV_25 | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 148, SYSCON_I2SDIV_PDIV_2  | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 150, SYSCON_I2SDIV_PDIV_3  | ( 25 & SYSCON_I2SDIV_MDIV_MASK) },
    { 152, SYSCON_I2SDIV_PDIV_2  | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 155, SYSCON_I2SDIV_PDIV_25 | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 156, SYSCON_I2SDIV_PDIV_3  | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 160, SYSCON_I2SDIV_PDIV_25 | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 162, SYSCON_I2SDIV_PDIV_3  | ( 27 & SYSCON_I2SDIV_MDIV_MASK) },
    { 164, SYSCON_I2SDIV_PDIV_2  | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 165, SYSCON_I2SDIV_PDIV_25 | ( 33 & SYSCON_I2SDIV_MDIV_MASK) },
    { 168, SYSCON_I2SDIV_PDIV_3  | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 170, SYSCON_I2SDIV_PDIV_25 | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 172, SYSCON_I2SDIV_PDIV_2  | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 174, SYSCON_I2SDIV_PDIV_3  | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 175, SYSCON_I2SDIV_PDIV_25 | ( 35 & SYSCON_I2SDIV_MDIV_MASK) },
    { 176, SYSCON_I2SDIV_PDIV_2  | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 180, SYSCON_I2SDIV_PDIV_3  | ( 30 & SYSCON_I2SDIV_MDIV_MASK) },
    { 184, SYSCON_I2SDIV_PDIV_2  | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 185, SYSCON_I2SDIV_PDIV_25 | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 186, SYSCON_I2SDIV_PDIV_3  | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 188, SYSCON_I2SDIV_PDIV_2  | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 190, SYSCON_I2SDIV_PDIV_25 | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 192, SYSCON_I2SDIV_PDIV_3  | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 195, SYSCON_I2SDIV_PDIV_25 | ( 39 & SYSCON_I2SDIV_MDIV_MASK) },
    { 196, SYSCON_I2SDIV_PDIV_2  | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 198, SYSCON_I2SDIV_PDIV_3  | ( 33 & SYSCON_I2SDIV_MDIV_MASK) },
    { 200, SYSCON_I2SDIV_PDIV_25 | ( 40 & SYSCON_I2SDIV_MDIV_MASK) },
    { 204, SYSCON_I2SDIV_PDIV_3  | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 205, SYSCON_I2SDIV_PDIV_25 | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 208, SYSCON_I2SDIV_PDIV_2  | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 210, SYSCON_I2SDIV_PDIV_3  | ( 35 & SYSCON_I2SDIV_MDIV_MASK) },
    { 212, SYSCON_I2SDIV_PDIV_2  | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 215, SYSCON_I2SDIV_PDIV_25 | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 216, SYSCON_I2SDIV_PDIV_3  | ( 36 & SYSCON_I2SDIV_MDIV_MASK) },
    { 220, SYSCON_I2SDIV_PDIV_25 | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 222, SYSCON_I2SDIV_PDIV_3  | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 224, SYSCON_I2SDIV_PDIV_2  | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 225, SYSCON_I2SDIV_PDIV_25 | ( 45 & SYSCON_I2SDIV_MDIV_MASK) },
    { 228, SYSCON_I2SDIV_PDIV_3  | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 230, SYSCON_I2SDIV_PDIV_25 | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 232, SYSCON_I2SDIV_PDIV_2  | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 234, SYSCON_I2SDIV_PDIV_3  | ( 39 & SYSCON_I2SDIV_MDIV_MASK) },
    { 235, SYSCON_I2SDIV_PDIV_25 | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 236, SYSCON_I2SDIV_PDIV_2  | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 240, SYSCON_I2SDIV_PDIV_3  | ( 40 & SYSCON_I2SDIV_MDIV_MASK) },
    { 244, SYSCON_I2SDIV_PDIV_2  | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 245, SYSCON_I2SDIV_PDIV_25 | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 246, SYSCON_I2SDIV_PDIV_3  | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 248, SYSCON_I2SDIV_PDIV_2  | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 250, SYSCON_I2SDIV_PDIV_25 | ( 50 & SYSCON_I2SDIV_MDIV_MASK) },
    { 252, SYSCON_I2SDIV_PDIV_3  | ( 42 & SYSCON_I2SDIV_MDIV_MASK) },
    { 255, SYSCON_I2SDIV_PDIV_25 | ( 51 & SYSCON_I2SDIV_MDIV_MASK) },
    { 256, SYSCON_I2SDIV_PDIV_2  | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 258, SYSCON_I2SDIV_PDIV_3  | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 260, SYSCON_I2SDIV_PDIV_25 | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 264, SYSCON_I2SDIV_PDIV_3  | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 265, SYSCON_I2SDIV_PDIV_25 | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 268, SYSCON_I2SDIV_PDIV_2  | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 270, SYSCON_I2SDIV_PDIV_3  | ( 45 & SYSCON_I2SDIV_MDIV_MASK) },
    { 272, SYSCON_I2SDIV_PDIV_2  | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 275, SYSCON_I2SDIV_PDIV_25 | ( 55 & SYSCON_I2SDIV_MDIV_MASK) },
    { 276, SYSCON_I2SDIV_PDIV_3  | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 280, SYSCON_I2SDIV_PDIV_25 | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 282, SYSCON_I2SDIV_PDIV_3  | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 284, SYSCON_I2SDIV_PDIV_2  | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 285, SYSCON_I2SDIV_PDIV_25 | ( 57 & SYSCON_I2SDIV_MDIV_MASK) },
    { 288, SYSCON_I2SDIV_PDIV_3  | ( 48 & SYSCON_I2SDIV_MDIV_MASK) },
    { 290, SYSCON_I2SDIV_PDIV_25 | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 292, SYSCON_I2SDIV_PDIV_2  | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 294, SYSCON_I2SDIV_PDIV_3  | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 295, SYSCON_I2SDIV_PDIV_25 | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 296, SYSCON_I2SDIV_PDIV_2  | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 300, SYSCON_I2SDIV_PDIV_3  | ( 50 & SYSCON_I2SDIV_MDIV_MASK) },
    { 304, SYSCON_I2SDIV_PDIV_2  | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 305, SYSCON_I2SDIV_PDIV_25 | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 306, SYSCON_I2SDIV_PDIV_3  | ( 51 & SYSCON_I2SDIV_MDIV_MASK) },
    { 308, SYSCON_I2SDIV_PDIV_2  | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 310, SYSCON_I2SDIV_PDIV_25 | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 312, SYSCON_I2SDIV_PDIV_3  | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 315, SYSCON_I2SDIV_PDIV_25 | ( 63 & SYSCON_I2SDIV_MDIV_MASK) },
    { 316, SYSCON_I2SDIV_PDIV_2  | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 318, SYSCON_I2SDIV_PDIV_3  | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 320, SYSCON_I2SDIV_PDIV_25 | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 324, SYSCON_I2SDIV_PDIV_3  | ( 54 & SYSCON_I2SDIV_MDIV_MASK) },
    { 325, SYSCON_I2SDIV_PDIV_25 | ( 65 & SYSCON_I2SDIV_MDIV_MASK) },
    { 328, SYSCON_I2SDIV_PDIV_2  | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 330, SYSCON_I2SDIV_PDIV_3  | ( 55 & SYSCON_I2SDIV_MDIV_MASK) },
    { 332, SYSCON_I2SDIV_PDIV_2  | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 335, SYSCON_I2SDIV_PDIV_25 | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 336, SYSCON_I2SDIV_PDIV_3  | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 340, SYSCON_I2SDIV_PDIV_25 | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 342, SYSCON_I2SDIV_PDIV_3  | ( 57 & SYSCON_I2SDIV_MDIV_MASK) },
    { 344, SYSCON_I2SDIV_PDIV_2  | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 345, SYSCON_I2SDIV_PDIV_25 | ( 69 & SYSCON_I2SDIV_MDIV_MASK) },
    { 348, SYSCON_I2SDIV_PDIV_3  | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 350, SYSCON_I2SDIV_PDIV_25 | ( 70 & SYSCON_I2SDIV_MDIV_MASK) },
    { 352, SYSCON_I2SDIV_PDIV_2  | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 354, SYSCON_I2SDIV_PDIV_3  | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 355, SYSCON_I2SDIV_PDIV_25 | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 356, SYSCON_I2SDIV_PDIV_2  | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 360, SYSCON_I2SDIV_PDIV_3  | ( 60 & SYSCON_I2SDIV_MDIV_MASK) },
    { 364, SYSCON_I2SDIV_PDIV_2  | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 365, SYSCON_I2SDIV_PDIV_25 | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 366, SYSCON_I2SDIV_PDIV_3  | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 368, SYSCON_I2SDIV_PDIV_2  | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 370, SYSCON_I2SDIV_PDIV_25 | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 372, SYSCON_I2SDIV_PDIV_3  | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 375, SYSCON_I2SDIV_PDIV_25 | ( 75 & SYSCON_I2SDIV_MDIV_MASK) },
    { 376, SYSCON_I2SDIV_PDIV_2  | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 378, SYSCON_I2SDIV_PDIV_3  | ( 63 & SYSCON_I2SDIV_MDIV_MASK) },
    { 380, SYSCON_I2SDIV_PDIV_25 | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 384, SYSCON_I2SDIV_PDIV_3  | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 385, SYSCON_I2SDIV_PDIV_25 | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 388, SYSCON_I2SDIV_PDIV_2  | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 390, SYSCON_I2SDIV_PDIV_3  | ( 65 & SYSCON_I2SDIV_MDIV_MASK) },
    { 392, SYSCON_I2SDIV_PDIV_2  | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 395, SYSCON_I2SDIV_PDIV_25 | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 396, SYSCON_I2SDIV_PDIV_3  | ( 66 & SYSCON_I2SDIV_MDIV_MASK) },
    { 400, SYSCON_I2SDIV_PDIV_25 | ( 80 & SYSCON_I2SDIV_MDIV_MASK) },
    { 402, SYSCON_I2SDIV_PDIV_3  | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 404, SYSCON_I2SDIV_PDIV_2  | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 405, SYSCON_I2SDIV_PDIV_25 | ( 81 & SYSCON_I2SDIV_MDIV_MASK) },
    { 408, SYSCON_I2SDIV_PDIV_3  | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 410, SYSCON_I2SDIV_PDIV_25 | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 412, SYSCON_I2SDIV_PDIV_2  | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 414, SYSCON_I2SDIV_PDIV_3  | ( 69 & SYSCON_I2SDIV_MDIV_MASK) },
    { 415, SYSCON_I2SDIV_PDIV_25 | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 416, SYSCON_I2SDIV_PDIV_2  | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 420, SYSCON_I2SDIV_PDIV_3  | ( 70 & SYSCON_I2SDIV_MDIV_MASK) },
    { 424, SYSCON_I2SDIV_PDIV_2  | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 425, SYSCON_I2SDIV_PDIV_25 | ( 85 & SYSCON_I2SDIV_MDIV_MASK) },
    { 426, SYSCON_I2SDIV_PDIV_3  | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 428, SYSCON_I2SDIV_PDIV_2  | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 430, SYSCON_I2SDIV_PDIV_25 | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 432, SYSCON_I2SDIV_PDIV_3  | ( 72 & SYSCON_I2SDIV_MDIV_MASK) },
    { 435, SYSCON_I2SDIV_PDIV_25 | ( 87 & SYSCON_I2SDIV_MDIV_MASK) },
    { 436, SYSCON_I2SDIV_PDIV_2  | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 438, SYSCON_I2SDIV_PDIV_3  | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 440, SYSCON_I2SDIV_PDIV_25 | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 444, SYSCON_I2SDIV_PDIV_3  | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 445, SYSCON_I2SDIV_PDIV_25 | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 448, SYSCON_I2SDIV_PDIV_2  | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 450, SYSCON_I2SDIV_PDIV_3  | ( 75 & SYSCON_I2SDIV_MDIV_MASK) },
    { 452, SYSCON_I2SDIV_PDIV_2  | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 455, SYSCON_I2SDIV_PDIV_25 | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 456, SYSCON_I2SDIV_PDIV_3  | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 460, SYSCON_I2SDIV_PDIV_25 | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 462, SYSCON_I2SDIV_PDIV_3  | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 464, SYSCON_I2SDIV_PDIV_2  | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 465, SYSCON_I2SDIV_PDIV_25 | ( 93 & SYSCON_I2SDIV_MDIV_MASK) },
    { 468, SYSCON_I2SDIV_PDIV_3  | ( 78 & SYSCON_I2SDIV_MDIV_MASK) },
    { 470, SYSCON_I2SDIV_PDIV_25 | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 472, SYSCON_I2SDIV_PDIV_2  | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 474, SYSCON_I2SDIV_PDIV_3  | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 475, SYSCON_I2SDIV_PDIV_25 | ( 95 & SYSCON_I2SDIV_MDIV_MASK) },
    { 476, SYSCON_I2SDIV_PDIV_2  | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 480, SYSCON_I2SDIV_PDIV_3  | ( 80 & SYSCON_I2SDIV_MDIV_MASK) },
    { 484, SYSCON_I2SDIV_PDIV_2  | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 485, SYSCON_I2SDIV_PDIV_25 | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 486, SYSCON_I2SDIV_PDIV_3  | ( 81 & SYSCON_I2SDIV_MDIV_MASK) },
    { 488, SYSCON_I2SDIV_PDIV_2  | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 490, SYSCON_I2SDIV_PDIV_25 | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 492, SYSCON_I2SDIV_PDIV_3  | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 495, SYSCON_I2SDIV_PDIV_25 | ( 99 & SYSCON_I2SDIV_MDIV_MASK) },
    { 496, SYSCON_I2SDIV_PDIV_2  | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 498, SYSCON_I2SDIV_PDIV_3  | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 500, SYSCON_I2SDIV_PDIV_25 | (100 & SYSCON_I2SDIV_MDIV_MASK) },
    { 504, SYSCON_I2SDIV_PDIV_3  | ( 84 & SYSCON_I2SDIV_MDIV_MASK) },
    { 505, SYSCON_I2SDIV_PDIV_25 | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 508, SYSCON_I2SDIV_PDIV_2  | (127 & SYSCON_I2SDIV_MDIV_MASK) },
    { 510, SYSCON_I2SDIV_PDIV_3  | ( 85 & SYSCON_I2SDIV_MDIV_MASK) },
    { 515, SYSCON_I2SDIV_PDIV_25 | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 516, SYSCON_I2SDIV_PDIV_3  | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 520, SYSCON_I2SDIV_PDIV_25 | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 522, SYSCON_I2SDIV_PDIV_3  | ( 87 & SYSCON_I2SDIV_MDIV_MASK) },
    { 525, SYSCON_I2SDIV_PDIV_25 | (105 & SYSCON_I2SDIV_MDIV_MASK) },
    { 528, SYSCON_I2SDIV_PDIV_3  | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 530, SYSCON_I2SDIV_PDIV_25 | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 534, SYSCON_I2SDIV_PDIV_3  | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 535, SYSCON_I2SDIV_PDIV_25 | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 540, SYSCON_I2SDIV_PDIV_3  | ( 90 & SYSCON_I2SDIV_MDIV_MASK) },
    { 545, SYSCON_I2SDIV_PDIV_25 | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 546, SYSCON_I2SDIV_PDIV_3  | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 550, SYSCON_I2SDIV_PDIV_25 | (110 & SYSCON_I2SDIV_MDIV_MASK) },
    { 552, SYSCON_I2SDIV_PDIV_3  | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 555, SYSCON_I2SDIV_PDIV_25 | (111 & SYSCON_I2SDIV_MDIV_MASK) },
    { 558, SYSCON_I2SDIV_PDIV_3  | ( 93 & SYSCON_I2SDIV_MDIV_MASK) },
    { 560, SYSCON_I2SDIV_PDIV_25 | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 564, SYSCON_I2SDIV_PDIV_3  | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 565, SYSCON_I2SDIV_PDIV_25 | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 570, SYSCON_I2SDIV_PDIV_3  | ( 95 & SYSCON_I2SDIV_MDIV_MASK) },
    { 575, SYSCON_I2SDIV_PDIV_25 | (115 & SYSCON_I2SDIV_MDIV_MASK) },
    { 576, SYSCON_I2SDIV_PDIV_3  | ( 96 & SYSCON_I2SDIV_MDIV_MASK) },
    { 580, SYSCON_I2SDIV_PDIV_25 | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 582, SYSCON_I2SDIV_PDIV_3  | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 585, SYSCON_I2SDIV_PDIV_25 | (117 & SYSCON_I2SDIV_MDIV_MASK) },
    { 588, SYSCON_I2SDIV_PDIV_3  | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 590, SYSCON_I2SDIV_PDIV_25 | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 594, SYSCON_I2SDIV_PDIV_3  | ( 99 & SYSCON_I2SDIV_MDIV_MASK) },
    { 595, SYSCON_I2SDIV_PDIV_25 | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 600, SYSCON_I2SDIV_PDIV_3  | (100 & SYSCON_I2SDIV_MDIV_MASK) },
    { 605, SYSCON_I2SDIV_PDIV_25 | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 606, SYSCON_I2SDIV_PDIV_3  | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 610, SYSCON_I2SDIV_PDIV_25 | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 612, SYSCON_I2SDIV_PDIV_3  | (102 & SYSCON_I2SDIV_MDIV_MASK) },
    { 615, SYSCON_I2SDIV_PDIV_25 | (123 & SYSCON_I2SDIV_MDIV_MASK) },
    { 618, SYSCON_I2SDIV_PDIV_3  | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 620, SYSCON_I2SDIV_PDIV_25 | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 624, SYSCON_I2SDIV_PDIV_3  | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 625, SYSCON_I2SDIV_PDIV_25 | (125 & SYSCON_I2SDIV_MDIV_MASK) },
    { 630, SYSCON_I2SDIV_PDIV_3  | (105 & SYSCON_I2SDIV_MDIV_MASK) },
    { 635, SYSCON_I2SDIV_PDIV_25 | (127 & SYSCON_I2SDIV_MDIV_MASK) },
    { 636, SYSCON_I2SDIV_PDIV_3  | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 642, SYSCON_I2SDIV_PDIV_3  | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 648, SYSCON_I2SDIV_PDIV_3  | (108 & SYSCON_I2SDIV_MDIV_MASK) },
    { 654, SYSCON_I2SDIV_PDIV_3  | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 660, SYSCON_I2SDIV_PDIV_3  | (110 & SYSCON_I2SDIV_MDIV_MASK) },
    { 666, SYSCON_I2SDIV_PDIV_3  | (111 & SYSCON_I2SDIV_MDIV_MASK) },
    { 672, SYSCON_I2SDIV_PDIV_3  | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 678, SYSCON_I2SDIV_PDIV_3  | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 684, SYSCON_I2SDIV_PDIV_3  | (114 & SYSCON_I2SDIV_MDIV_MASK) },
    { 690, SYSCON_I2SDIV_PDIV_3  | (115 & SYSCON_I2SDIV_MDIV_MASK) },
    { 696, SYSCON_I2SDIV_PDIV_3  | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 702, SYSCON_I2SDIV_PDIV_3  | (117 & SYSCON_I2SDIV_MDIV_MASK) },
    { 708, SYSCON_I2SDIV_PDIV_3  | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 714, SYSCON_I2SDIV_PDIV_3  | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 720, SYSCON_I2SDIV_PDIV_3  | (120 & SYSCON_I2SDIV_MDIV_MASK) },
    { 726, SYSCON_I2SDIV_PDIV_3  | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 732, SYSCON_I2SDIV_PDIV_3  | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 738, SYSCON_I2SDIV_PDIV_3  | (123 & SYSCON_I2SDIV_MDIV_MASK) },
    { 744, SYSCON_I2SDIV_PDIV_3  | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 750, SYSCON_I2SDIV_PDIV_3  | (125 & SYSCON_I2SDIV_MDIV_MASK) },
    { 756, SYSCON_I2SDIV_PDIV_3  | (126 & SYSCON_I2SDIV_MDIV_MASK) },
    { 762, SYSCON_I2SDIV_PDIV_3  | (127 & SYSCON_I2SDIV_MDIV_MASK) }
};

/*
 * When we get to the multichannel case the pre-fill and enable code
 * will go to the dma driver's start routine.
 */
static void snd_ep93xx_i2s_enable_transmit(void){
    
    int i;
    unsigned long ulTemp;
    
    outl( 0, I2STX0En );
    outl( 0, I2STX1En );
    outl( 0, I2STX2En );
    ulTemp = inl( I2STX2En ); /* input to push the outl's past the wrapper */
    
    for( i = 0 ; i < 8 ; i++ ){
	outl( 0, I2STX0Lft );
	outl( 0, I2STX0Rt );
	outl( 0, I2STX1Lft );
	outl( 0, I2STX1Rt );
	outl( 0, I2STX2Lft );
	outl( 0, I2STX2Rt );
	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */
    }
    outl( 1, I2STX0En );
    outl( 1, I2STX1En );
    outl( 1, I2STX2En );
    ulTemp = inl( I2STX2En ); /* input to push the outl's past the wrapper */
}

static void snd_ep93xx_i2s_disable_transmit( void )
{
    unsigned long ulTemp;
         
    outl( 0, I2STX0En );
    outl( 0, I2STX1En );
    outl( 0, I2STX2En );
    ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */
    					
}
					    
 
static void snd_ep93xx_i2s_enable_receive(void)
{
    unsigned long ulTemp;
	
    outl( 1, I2SRX0En );
    outl( 1, I2SRX1En );
    outl( 1, I2SRX2En );
    ulTemp = inl( I2SRX2En );

}

static void snd_ep93xx_i2s_disable_receive( void )
{
    unsigned long ulTemp;

    outl( 0, I2SRX0En );
    outl( 0, I2SRX1En );
    outl( 0, I2SRX2En );
    ulTemp = inl( I2SRX2En );
}

static void snd_ep93xx_i2s_enable( void )
{
    unsigned long ulTemp;
        
    outl(1, I2SGlCtrl );
    ulTemp = inl( I2SGlCtrl );
}

static void snd_ep93xx_i2s_disable( void )
{
    unsigned long ulTemp;
    
    outl(0, I2SGlCtrl );
    ulTemp = inl( I2SGlCtrl );
}


/*
 * ep93xx_calc_closest_freq
 * 
 *   Return            0 - Failure
 *                     1 - Success
 */
static int ep93xx_calc_closest_freq
(
    ulong   ulPLLFreq, 
    ulong   ulRequestedMClkFreq,
    ulong * pulActualMClkFreq,
    ulong * pulI2SDiv
)
{
    ulong   ulLower;
    ulong   ulUpper;
    ulong   ulDiv;
    int     x;
    
    /* Calculate the closest divisor. */
    ulDiv =  (ulPLLFreq * 2)/ ulRequestedMClkFreq;

    for(x = 1; x < sizeof(I2SDivTable)/sizeof(DIV_TABLE); x++){

	/* Calculate the next greater and lower value. */
	ulLower = I2SDivTable[x - 1].ulTotalDiv;     
	ulUpper = I2SDivTable[x].ulTotalDiv;     

	/* Check to see if it is in between the two values. */
	if(ulLower <= ulDiv && ulDiv < ulUpper)
	    break;
    }

    /* Return if we did not find a divisor. */
    if(x == sizeof(I2SDivTable)/sizeof(DIV_TABLE)){
	
	DPRINTK("ep93xx_i2s couldn't find a divisor.\n");

	*pulActualMClkFreq  = 0;
	*pulI2SDiv          = 0;
	return -1;
    }

    /* See which is closer, the upper or the lower case. */
    if(ulUpper * ulRequestedMClkFreq - ulPLLFreq * 2 >  
		ulPLLFreq * 2 - ulLower * ulRequestedMClkFreq){
	x-=1;
    }

    *pulActualMClkFreq  = (ulPLLFreq * 2)/ I2SDivTable[x].ulTotalDiv;
    *pulI2SDiv          = I2SDivTable[x].ulI2SDiv;

    return 0;
}

/*
 * ep93xx_get_PLL_frequency
 *
 * Given a value for ClkSet1 or ClkSet2, calculate the PLL1 or PLL2 frequency.
 */
static ulong ep93xx_get_PLL_frequency( ulong ulCLKSET )
{
    ulong ulX1FBD, ulX2FBD, ulX2IPD, ulPS, ulPLL_Freq;

    ulPS = (ulCLKSET & SYSCON_CLKSET1_PLL1_PS_MASK) >> SYSCON_CLKSET1_PLL1_PS_SHIFT;
    ulX1FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X1FBD1_MASK) >> SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT;
    ulX2FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2FBD2_MASK) >> SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT;
    ulX2IPD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2IPD_MASK) >> SYSCON_CLKSET1_PLL1_X2IPD_SHIFT;
                                                                                                                             
    ulPLL_Freq = (((0x00e10000 * (ulX1FBD+1)) / (ulX2IPD+1)) * (ulX2FBD+1)) >> ulPS;
    return ulPLL_Freq;
}

/*
 * SetSampleRate
 * disables i2s channels and sets up i2s divisors
 * in syscon for the requested sample rate.
  * lFrequency - Sample Rate in Hz
 */
static void ep93xx_set_samplerate(long lFrequency)
{
    ulong ulRequestedMClkFreq, ulPLL1_CLOCK, ulPLL2_CLOCK;
    ulong ulMClkFreq1, ulMClkFreq2, ulClkSet1, ulClkSet2;
    ulong ulI2SDiv, ulI2SDiv1, ulI2SDiv2, ulI2SDIV, actual_samplerate;
    
    /* Clock ratios: MCLK to SCLK and SCLK to LRCK */
    ulong ulM2SClock  = 4;
    ulong ulS2LRClock = 64;

    DPRINTK( "ep93xx_set_samplerate = %d Hz.\n", (int)lFrequency );
    /*
     * Read CLKSET1 and CLKSET2 in the System Controller and calculate
     * the PLL frequencies from that.
     */
    ulClkSet1 =	inl(SYSCON_CLKSET1);
    ulClkSet2 =	inl(SYSCON_CLKSET2);
    ulPLL1_CLOCK = ep93xx_get_PLL_frequency( ulClkSet1 );
    ulPLL2_CLOCK = ep93xx_get_PLL_frequency( ulClkSet2 );

    DPRINTK( "ep93xx_i2s_ClkSet1=0x%08x Clkset2=0x%08x  PLL1=%d Hz  PLL2=%d Hz\n", 
    	(int)ulClkSet1, (int)ulClkSet2, (int)ulPLL1_CLOCK, (int)ulPLL2_CLOCK );

    ulRequestedMClkFreq = ( lFrequency * ulM2SClock * ulS2LRClock);

    ep93xx_calc_closest_freq
    (
    	ulPLL1_CLOCK, 
    	ulRequestedMClkFreq,
	&ulMClkFreq1,
	&ulI2SDiv1
    );
    ep93xx_calc_closest_freq
    (
    	ulPLL2_CLOCK, 
    	ulRequestedMClkFreq,
	&ulMClkFreq2,
	&ulI2SDiv2
    );

    /* See which is closer, MClk rate 1 or MClk rate 2. */
    if(abs(ulMClkFreq1 - ulRequestedMClkFreq) < abs(ulMClkFreq2 -ulRequestedMClkFreq)){
	ulI2SDiv = ulI2SDiv1;
	actual_samplerate = ulMClkFreq1/ (ulM2SClock * ulS2LRClock);
	DPRINTK( "ep93xx_set_samplerate - using PLL1\n" );
    }
    else{
	ulI2SDiv = ulI2SDiv2 | SYSCON_I2SDIV_PSEL;
	actual_samplerate = ulMClkFreq1 / (ulM2SClock * ulS2LRClock);
	DPRINTK( "ep93xx_set_samplerate - using PLL2\n" );
    }
        
    /* Calculate the new I2SDIV register value and write it out. */
    ulI2SDIV = ulI2SDiv | SYSCON_I2SDIV_SENA |  SYSCON_I2SDIV_ORIDE |
	    SYSCON_I2SDIV_SPOL| SYSCON_I2SDIV_LRDIV_64 |
	    SYSCON_I2SDIV_SDIV | SYSCON_I2SDIV_MENA | SYSCON_I2SDIV_ESEL;

DPRINTK("I2SDIV set to 0x%08x\n", (unsigned int)ulI2SDIV );									
    SysconSetLocked(SYSCON_I2SDIV, ulI2SDIV);
DPRINTK("I2SDIV set to 0x%08x\n", inl(SYSCON_I2SDIV));
}

/*
 * BringUpI2S
 * This routine sets up the I2S Controller.
 */
static void snd_ep93xx_i2s_init( char wordlenght )
{
    unsigned int uiDEVCFG;

    DPRINTK("snd_ep93xx_i2s_init - enter\n");

    if(wordlenght)
	DPRINTK("i2s_controller set to 24bit lenght\n");
    else
        DPRINTK("i2s_controller set to 16bit lenght\n");
			
    /*
     * Configure 
     * EGPIO[4,5,6,13] to be SDIN's and SDOUT's for the second and third
     * I2S stereo channels if the codec is a 6 channel codec.
     */
    uiDEVCFG = inl(SYSCON_DEVCFG/*EP93XX_SYSCON_DEVICE_CONFIG*/);
 DPRINTK("snd_ep93xx_i2s_init =%x - 0\n",uiDEVCFG);
#ifdef CONFIG_CODEC_CS4271
    uiDEVCFG |= SYSCON_DEVCFG_I2SonAC97;
 DPRINTK("snd_ep93xx_i2s_init =%x- 0 0\n",uiDEVCFG);
#else /* CONFIG_CODEC_CS4228A */
    uiDEVCFG |= SYSCON_DEVCFG_I2SonAC97 | SYSCON_DEVCFG_A1onG | SYSCON_DEVCFG_A2onG;
#endif
 DPRINTK("snd_ep93xx_i2s_init - 0 1\n");
    SysconSetLocked(SYSCON_DEVCFG/*EP93XX_SYSCON_DEVICE_CONFIG*/, uiDEVCFG);

uiDEVCFG = inl(SYSCON_DEVCFG/*EP93XX_SYSCON_DEVICE_CONFIG*/);
 DPRINTK("snd_ep93xx_i2s_init=%x - 1\n",uiDEVCFG);
    /* Configure I2S Tx channel */
    /* Justify Left, MSB first */
    outl( 0, I2STXLinCtrlData );
	
    /* WL = 24 bits */
    outl( wordlenght, I2STXWrdLen );

    /*
     * Set the I2S control block to master mode.
     * Tx bit clk rate determined by word legnth 
     * Do gate off last 8 clks (24 bit samples in a 32 bit field)
     * LRclk = I2S timing; LRck = 0 for left
     */
		 
    outl( ( i2s_txcc_mstr | i2s_txcc_trel), I2STxClkCfg );
    /* Configure I2S rx channel */
    /* First, clear all config bits. */
    outl( 0, I2SRXLinCtrlData );
    outl( wordlenght, I2SRXWrdLen );

    /* 
     * Set the I2S control block to master mode.
     * Rx bit clk rate determined by word legnth 
     * Do gate off last 8 clks 
     * setting i2s_rxcc_rrel gives us I2S timing
     * clearing i2s_rlrs gives us LRck = 0 for left, 1 for right
     * setting i2s_rxcc_nbcg specifies to not gate off extra 8 clocks 
     */
    outl( (i2s_rxcc_bcr_64x | i2s_rxcc_nbcg |i2s_rxcc_mstr | i2s_rxcc_rrel), I2SRxClkCfg );
	
    /* Do an input to push the outl's past the wrapper */
    uiDEVCFG = inl(SYSCON_DEVCFG/*EP93XX_SYSCON_DEVICE_CONFIG*/);
    
    DPRINTK("snd_ep93xx_i2s_init =%x- EXIT\n",uiDEVCFG);
}

/*
 * ep93xx_init_i2s_codec
 *
 * Note that codec must be getting i2s clocks for any codec
 * register writes to work.
 */
static void snd_ep93xx_codec_init( void )
{

#if defined(CONFIG_MACH_EDB9301) || defined(CONFIG_MACH_EDB9302)
	unsigned int uiPADR, uiPADDR;
#endif

#if defined(CONFIG_MACH_EDB9315A)
	unsigned int uiPBDR, uiPBDDR;
#endif

#if defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A)
	unsigned int uiPHDR, uiPHDDR, uiDEVCFG;;
#endif


#ifdef CONFIG_CODEC_CS4271
	/*
	 * Some EDB9301 boards use EGPIO1 (port 1, bit 1) for the I2S reset.
	 * EGPIO1 has a pulldown so if it isn't configured as an output, it is low.
	 */
#if defined(CONFIG_MACH_EDB9301) || defined(CONFIG_MACH_EDB9302)
	uiPADR  = inl(GPIO_PADR);
	uiPADDR = inl(GPIO_PADDR);
   	DPRINTK("%s 01 02\n",__FUNCTION__); 
	/* Clear bit 1 of the data register */
	outl( (uiPADR & 0xfd), GPIO_PADR );
	uiPADR  = inl(GPIO_PADR);

	/* Set bit 1 of the DDR to set it to output
	 * Now we are driving the reset pin low.
	 */
	outl( (uiPADDR | 0x02), GPIO_PADDR );
	uiPADDR = inl(GPIO_PADDR);

	udelay( 2 );  /* plenty of time */

	/* Set bit 1 of the data reg.  Now we drive the reset pin high. */
	outl( (uiPADR | 0x02),  GPIO_PADR );
	uiPADR  = inl(GPIO_PADR);
#endif

#if defined(CONFIG_MACH_EDB9315A)
        DPRINTK("%s EDB15A\n",__FUNCTION__);
	//outl( 0xa3, GPIO_PBDR );
	//outl( 0,GPIO_PBDDR);
	uiPBDR  = inl(GPIO_PBDR);
	uiPBDDR = inl(GPIO_PBDDR);
   	DPRINTK("uiPBDR=%x,uiPBDDR=%x\n",uiPBDR,uiPBDDR); 
	/* Clear bit 1 of the data register */
	outl( (uiPBDR & 0xbf), GPIO_PBDR );
	uiPBDR  = inl(GPIO_PBDR);

	/* Set bit 1 of the DDR to set it to output
	 * Now we are driving the reset pin low.
	 */
	outl( (uiPBDDR | 0x40), GPIO_PBDDR );
	uiPBDDR = inl(GPIO_PBDDR);

        //uiPBDR  = inl(GPIO_PBDR);
        //uiPBDDR = inl(GPIO_PBDDR);
	//DPRINTK("uiPBDR=%x,uiPBDDR=%x\n",uiPBDR,uiPBDDR);
	udelay( 2 );  /* plenty of time */

	/* Set bit 1 of the data reg.  Now we drive the reset pin high. */
	outl( (uiPBDR | 0x40),  GPIO_PBDR );
	uiPBDR  = inl(GPIO_PBDR);

        //uiPBDR  = inl(GPIO_PBDR);
        //uiPBDDR = inl(GPIO_PBDDR);
	//DPRINTK("uiPBDR=%x,uiPBDDR=%x\n",uiPBDR,uiPBDDR);
#endif

#if defined(CONFIG_MACH_EDB9307A) || defined(CONFIG_MACH_EDB9302A)
    
	/* Setup bit 11 in DEV_CONFIG for Port HonIDE to do GPIO */
	uiDEVCFG = inl(SYSCON_DEVCFG);
	uiDEVCFG |= SYSCON_DEVCFG_HonIDE;
	
	/* Unlock SYSCON_DEVCFG */
	SysconSetLocked(SYSCON_DEVCFG, uiDEVCFG);
	
	uiPHDR  = inl(GPIO_PHDR);
	uiPHDDR = inl(GPIO_PHDDR);
    
	/* Clear bit 3 of the data register */
	outl( (uiPHDR & 0xfb), GPIO_PHDR );
	uiPHDR  = inl(GPIO_PHDR);

	/* Set bit 3 of the DDR to set it to output
	 * Now we are driving the reset pin low.
	 */
	outl( (uiPHDDR | 0x04), GPIO_PHDDR );
	uiPHDDR = inl(GPIO_PHDDR);

	udelay( 2 );  /* plenty of time */

	/* Set bit 3 of the data reg.  Now we drive the reset pin high. */
	outl( (uiPHDR | 0x04),  GPIO_PHDR );
	uiPHDR  = inl(GPIO_PHDR);
#endif
	/*
	 * Write to the control port, setting the enable control port bit
	 * so that we can write to the control port.  OK?
	 */
	SSPDriver->Write( SSP_Handle, 7, 0x02 );

	/* Select slave, 24Bit I2S serial mode */
	SSPDriver->Write( SSP_Handle, 1, 0x01 );

	SSPDriver->Write( SSP_Handle, 6, 0x10 );

	/* Set AMUTE (auto-mute) bit. */
	SSPDriver->Write( SSP_Handle, 2, 0x00 );

#else // CONFIG_CODEC_CS4228A
    SSPDriver->Write( SSP_Handle, 0x01, 0x04 );
    SSPDriver->Write( SSP_Handle, 0x0D, 0x84 );

#endif

}

static int snd_ep93xx_codec_setvolume( int value )
{
#ifdef CONFIG_CODEC_CS4271
        int iMute = 0;
#endif

	DPRINTK("snd_ep93xx_codec_setvolume %x- enter\n",value);

#ifdef CONFIG_CODEC_CS4271
        /*
         * CS4271 DAC attenuation is 0 to 127 dB in 1 dB steps
         */
        if( mute )
        {
                iMute = 0x80;
        }
        DPRINTK("%x---%x\n",iMute,mute);
        /*
         * Write the volume for DAC1 and DAC2 (reg 4 and 5)
         */
        SSPDriver->Write( SSP_Handle, 4, (value | iMute) );
        SSPDriver->Write( SSP_Handle, 5, (value | iMute) );


#else // CONFIG_CODEC_CS4228A
    /*
     * CS4228A DAC attenuation is 0 to 90.5 dB in 0.5 dB steps
     */
    SSPDriver->Write( SSP_Handle, 0x07, value );
    SSPDriver->Write( SSP_Handle, 0x08, value );
    SSPDriver->Write( SSP_Handle, 0x09, value );
    SSPDriver->Write( SSP_Handle, 0x0A, value );
    SSPDriver->Write( SSP_Handle, 0x0B, value );
    SSPDriver->Write( SSP_Handle, 0x0C, value );

#endif
    DPRINTK("snd_ep93xx_codec_setvolume - exit\n");
    return 0;
}

/*
 * ep93xx_automute_i2s_codec
 *
 * Note that codec must be getting i2s clocks for any codec
 * register writes to work.
 */
static void snd_ep93xx_codec_automute (audio_state_t *state)
{
	DPRINTK("snd_ep93xx_codec_automute - enter\n");	
#ifdef CONFIG_CODEC_CS4271
	/*
	 * The automute bit is set by default for the CS4271.
	 * Clear the driver's mute flag and use the set_volume routine
	 * to write the current volume out with the mute bit cleared.
	 */
    mute = 0;
    snd_ep93xx_codec_setvolume(0);
    
    state->playback_volume[0][0] = 127 ;
    state->playback_volume[0][0] = 127 ;


#else // CONFIG_CODEC_CS4228A	
    SSPDriver->Write( SSP_Handle, 4, 0 );
		
    /* Unmute the DACs */
    SSPDriver->Write( SSP_Handle, 5, 0x80 );
    /* Unmute the MUTEC pin, turn on automute. */
    SSPDriver->Write( SSP_Handle, 5, 0x40 );
    
    state->playback_volume[0][0] = 181 ;
    state->playback_volume[0][1] = 181 ;
    state->playback_volume[1][0] = 181 ;
    state->playback_volume[1][1] = 181 ;
    state->playback_volume[2][0] = 181 ;
    state->playback_volume[2][1] = 181 ;

#endif    
    DPRINTK("snd_ep93xx_codec_automute - exit\n");
}


/*
 * ep93xx_audio_init
 * Note that the codec powers up with its DAC's muted and
 * the serial format set to 24 bit I2S mode.
 */
static void snd_ep93xx_audio_init(audio_state_t *state)
{
    DPRINTK("snd_ep93xx_audio_init - enter\n");
    /* Mute the DACs. Disable all audio channels. */  
    /* Must do this to change sample rate. */

    snd_ep93xx_i2s_disable_transmit();
    snd_ep93xx_i2s_disable_receive();
    snd_ep93xx_i2s_disable();
    
    /*Set up the i2s clocks in syscon.  Enable them. */ 
    ep93xx_set_samplerate( AUDIO_SAMPLE_RATE_DEFAULT );

    /* Set i2s' controller serial format, and enable */ 
    snd_ep93xx_i2s_init(WL16);

    /* Initialize codec serial format, etc. */
    snd_ep93xx_codec_init();

    /* Clear the fifo and enable the tx0 channel. */
    snd_ep93xx_i2s_enable_transmit();
    snd_ep93xx_i2s_enable_receive();
    snd_ep93xx_i2s_enable();

    /* Set the volume for the first time. */
    snd_ep93xx_codec_setvolume( AUDIO_DEFAULT_VOLUME );

    /* Unmute the DAC and set the mute pin MUTEC to automute. */
    snd_ep93xx_codec_automute(state);
	
    DPRINTK("snd_ep93xx_audio_init - exit\n");
}

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
	case SNDRV_PCM_FMTBIT_S24_BE:		
		s->audio_format = SNDRV_PCM_FORMAT_S24_LE;
		s->audio_stream_bitwidth = 24;
		break;

	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FMTBIT_U24_BE:		
        default:
		s->audio_format = SNDRV_PCM_FORMAT_U24_LE;
		s->audio_stream_bitwidth = 24;
		break;
    }

    DPRINTK( "ep93xx_i2s_audio_set_format EXIT format set to be (%d) ", (int)s->audio_format );
    print_audio_format( (long)s->audio_format );
}

static __inline__ unsigned long copy_to_user_S24_LE
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

static __inline__ unsigned long copy_to_user_S16_LE
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

static __inline__ unsigned long copy_to_user_U16_LE
(
    audio_stream_t *stream,
    const char *to, 
    unsigned long to_count
)
{
    int *dma_buffer_0 = (int *)stream->hwbuf[0];
    int *dma_buffer_1 = (int *)stream->hwbuf[1];
    int *dma_buffer_2 = (int *)stream->hwbuf[2];
    int count;
    int total_to_count = to_count;
    short * user_ptr = (short *)to;	/* 16 bit user buffer */

    count = 4 * stream->dma_num_channels;
		
    while (to_count > 0){

	__put_user( ((short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );
	__put_user( ((short)( *dma_buffer_0++ )) ^ 0x8000, user_ptr++ );

        if(stream->audio_channels_flag & CHANNEL_REAR ){
	    __put_user( ((short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((short)( *dma_buffer_1++ )) ^ 0x8000, user_ptr++ );
	}

        if(stream->audio_channels_flag & CHANNEL_CENTER_LFE ){
	    __put_user( ((short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	    __put_user( ((short)( *dma_buffer_2++ )) ^ 0x8000, user_ptr++ );
	}
	to_count -= count;
    }
    return total_to_count;
}

static __inline__ unsigned long copy_to_user_S8
(
    audio_stream_t *stream,
    const char *to, 
    unsigned long to_count
)
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

static __inline__ unsigned long copy_to_user_U8
(
    audio_stream_t *stream,
    const char *to, 
    unsigned long to_count
)
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

static __inline__ int copy_to_user_with_conversion
(
    audio_stream_t *stream,
    const char *to, 
    int toCount
)
{
    int ret = 0;
	
    if( toCount == 0 ){
	DPRINTK("ep93xx_i2s_copy_to_user_with_conversion - nothing to copy!\n");
    }

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
		ret = copy_to_user_S24_LE( stream, to, toCount );
		break;
		
	case SNDRV_PCM_FORMAT_U24_LE:
		ret = copy_to_user_U24_LE( stream, to, toCount );
		break;
        default:
                DPRINTK( "ep93xx_i2s copy to user unsupported audio format\n" );
		break;
    }
    return ret;
}

static __inline__ int copy_from_user_S24_LE
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
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

static __inline__ int copy_from_user_U24_LE
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
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

static __inline__ int copy_from_user_S16_LE
(
	audio_stream_t *stream,
	const char *from, 
	int toCount 
)
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
	__get_user(data, user_buffer++);
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
    return toCount0 / 2;
}

static __inline__ int copy_from_user_U16_LE
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
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

static __inline__ int copy_from_user_S8
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
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
	__get_user(data, user_buffer++);
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
    return toCount0 / 4;
}

static __inline__ int copy_from_user_U8
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
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
	__get_user(data, user_buffer++);
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
    return toCount0 / 4;
}

/*
 * Returns negative for error
 * Returns # of bytes transferred out of the from buffer
 * for success.
 */
static __inline__ int copy_from_user_with_conversion
(
    audio_stream_t *stream,
    const char *from, 
    int toCount 
)
{
    int ret = 0;
//    DPRINTK("copy_from_user_with_conversion\n");	
    if( toCount == 0 ){
    	DPRINTK("ep93xx_i2s_copy_from_user_with_conversion - nothing to copy!\n");
    }

    switch( stream->audio_format ){

	case SNDRV_PCM_FORMAT_S8:
		//DPRINTK("SNDRV_PCM_FORMAT_S8\n");
		ret = copy_from_user_S8( stream, from, toCount );
		break;
			
	case SNDRV_PCM_FORMAT_U8:
		//DPRINTK("SNDRV_PCM_FORMAT_U8\n");
		ret = copy_from_user_U8( stream, from, toCount );
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
		//DPRINTK("SNDRV_PCM_FORMAT_S16_LE\n");
		ret = copy_from_user_S16_LE( stream, from, toCount );
		break;
				
	case SNDRV_PCM_FORMAT_U16_LE:
		//DPRINTK("SNDRV_PCM_FORMAT_U16_LE\n");
		ret = copy_from_user_U16_LE( stream, from, toCount );
		break;

	case SNDRV_PCM_FORMAT_S24_LE:
		//DPRINTK("SNDRV_PCM_FORMAT_S24_LE\n");
		ret = copy_from_user_S24_LE( stream, from, toCount );
		break;
		
	case SNDRV_PCM_FORMAT_U24_LE:
		//DPRINTK("SNDRV_PCM_FORMAT_U24_LE\n");
		ret = copy_from_user_U24_LE( stream, from, toCount );
		break;
        default:
                DPRINTK( "ep93xx_i2s copy from user unsupported audio format\n" );
		break;			
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
static void snd_ep93xx_dma2usr_ratio( audio_stream_t * stream )
{
    unsigned int dma_sample_size, user_sample_size;
	
    dma_sample_size = 8;	/* each stereo sample is 2 * 32 bits */
	
    // If stereo 16 bit, user sample is 4 bytes.
    // If stereo  8 bit, user sample is 2 bytes.

    user_sample_size = stream->audio_stream_bitwidth / 4;

    stream->dma2usr_ratio = dma_sample_size / user_sample_size;
}

/*---------------------------------------------------------------------------------------------*/

static int snd_ep93xx_dma_free(snd_pcm_substream_t *substream ){

    
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

static int snd_ep93xx_dma_config(snd_pcm_substream_t *substream ){

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

static void snd_ep93xx_deallocate_buffers( snd_pcm_substream_t *substream, audio_stream_t *stream )
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

		//kfree(dma_chan->area);
                dma_free_writecombine(0, dma_chan->bytes,
                                      dma_chan->area, dma_chan->addr);
		dma_chan->area = NULL;
	    }    
	}
	kfree(stream->dma_channels);
	stream->dma_channels = NULL;
    }
    DPRINTK("snd_ep93xx_deallocate_buffers - exit\n");
}

static int snd_ep93xx_allocate_buffers( snd_pcm_substream_t *substream, audio_stream_t *stream)
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

	//channel->area = kmalloc( size, GFP_DMA );
	channel->area = dma_alloc_writecombine(0, size,&channel->addr,GFP_KERNEL);		
	if(!channel->area){
	    printk(AUDIO_NAME ": unable to allocate audio memory\n");
	    return -ENOMEM;
	}	
	channel->bytes = size;
	//channel->addr = __virt_to_phys((int) channel->area);
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
 
static void snd_ep93xx_dma_tx_callback
( 
	ep93xx_dma_int_t DMAInt,
	ep93xx_dma_dev_t device, 
	unsigned int user_data 
)
{
    int handle;
    int i,chan;
    unsigned int buf_id;
	    		
    snd_pcm_substream_t *substream = (snd_pcm_substream_t *)user_data;
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    audio_stream_t *stream = state->output_stream;
    audio_buf_t *buf;

    switch( device )              
    {
	case DMATx_I2S3:
//	    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S3\n");
	    i = 2;
	    break;
    	case DMATx_I2S2:
//	    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S2\n");
       	    i = 1;
	    break;
	case DMATx_I2S1:
	    default:
//	    DPRINTK( "snd_ep93xx_dma_tx_callback - DMATx_I2S1\n");
       	    i = 0;
	    break;
    }

    chan = stream->audio_num_channels / 2 - 1;
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

static void snd_ep93xx_dma_rx_callback
(
	ep93xx_dma_int_t DMAInt,
	ep93xx_dma_dev_t device, 
	unsigned int user_data 
)
{
    int handle,i,chan;
    unsigned int buf_id;
    audio_buf_t *buf;
		
    snd_pcm_substream_t *substream = (snd_pcm_substream_t *)user_data;
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    audio_stream_t *stream = state->input_stream;

    switch( device ){
		
	case DMARx_I2S3:
//    	    DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S3\n");
	    i = 2;
	    break;
    	case DMARx_I2S2:
//          DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S2\n");
	    i = 1;
	    break;
	case DMARx_I2S1:
	    default:
//	    DPRINTK( "snd_ep93xx_dma_rx_callback - DMARx_I2S1\n");
	    i = 0;
	    break;
    }
    chan = stream->audio_num_channels / 2 - 1;
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

static int snd_ep93xx_release(snd_pcm_substream_t *substream)
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

static int snd_ep93xx_pcm_hw_params(snd_pcm_substream_t *substream,
				snd_pcm_hw_params_t *params)
{
        DPRINTK("snd_ep93xx_pcm_hw_params - enter\n");
	return snd_pcm_lib_malloc_pages(substream,params_buffer_bytes(params));
}

static int snd_ep93xx_pcm_hw_free(snd_pcm_substream_t *substream)
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

static int snd_ep93xx_pcm_prepare_playback( snd_pcm_substream_t *substream)
{
    audio_state_t *state = (audio_state_t *) substream->private_data;
    snd_pcm_runtime_t *runtime = substream->runtime;
    audio_stream_t *stream = state->output_stream;

    DPRINTK("snd_ep93xx_pcm_prepare_playback - enter\n");
    
    snd_ep93xx_i2s_disable_transmit();
	        
    snd_ep93xx_deallocate_buffers(substream,stream);
			
    if(runtime->channels % 2 != 0)
	return -1;
		   	
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
    stream->dma_num_channels = runtime->channels / 2;


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
	    ep93xx_set_samplerate( runtime->rate );
	}    
	if( stream->audio_format != runtime->format ){
    	    snd_ep93xx_i2s_init((stream->audio_stream_bitwidth == 24));
	}
    }
    else{
        audio_stream_t *s = state->input_stream;
        if( runtime->format != s->audio_format)
    	    return -1;
	if( runtime->rate != s->audio_rate )
	    return -1;
    }

    stream->audio_rate = runtime->rate;
    audio_set_format( stream, runtime->format );
    snd_ep93xx_dma2usr_ratio( stream );
	
    if( snd_ep93xx_allocate_buffers( substream, stream ) != 0 ){
        snd_ep93xx_deallocate_buffers( substream, stream );
        return -1;
    }
    
    snd_ep93xx_i2s_enable_transmit();
												 
    DPRINTK("snd_ep93xx_pcm_prepare_playback - exit\n");
    return 0;	
}

static int snd_ep93xx_pcm_prepare_capture( snd_pcm_substream_t *substream)
{
    audio_state_t *state = (audio_state_t *) substream->private_data;
    snd_pcm_runtime_t *runtime = substream->runtime;
    audio_stream_t *stream = state->input_stream;
    
    snd_ep93xx_i2s_disable_receive();
	
    snd_ep93xx_deallocate_buffers(substream,stream);

    if(runtime->channels % 2 != 0)
	return -1;
		   		       
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
    stream->dma_num_channels = runtime->channels / 2;

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

	if( stream->audio_rate != runtime->rate ){
    	    ep93xx_set_samplerate( runtime->rate );
	}
        if( stream->audio_format != runtime->format ){
    	    snd_ep93xx_i2s_init((stream->audio_stream_bitwidth == 24));
	}
    }
    else{
        audio_stream_t *s = state->output_stream;
        if( runtime->format != s->audio_format)
    	    return -1;
	if( runtime->rate != s->audio_rate )
    	    return -1;
    }

    stream->audio_rate = runtime->rate;
    audio_set_format( stream, runtime->format );
    snd_ep93xx_dma2usr_ratio( stream );

    if( snd_ep93xx_allocate_buffers( substream, stream ) != 0 ){
        snd_ep93xx_deallocate_buffers( substream, stream );
	return -1;
    }
    
    snd_ep93xx_i2s_enable_receive();
												 
    DPRINTK("snd_ep93xx_pcm_prepare_capture - exit\n");
    return 0;	
}
/*
 *start/stop/pause dma translate
 */
static int snd_ep93xx_pcm_trigger(snd_pcm_substream_t *substream, int cmd)
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
		
		memset(dma_channel->area,0,dma_channel->bytes);
		
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

static snd_pcm_uframes_t snd_ep93xx_pcm_pointer_playback(snd_pcm_substream_t *substream)
{
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    snd_pcm_runtime_t *runtime = substream->runtime;
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

static snd_pcm_uframes_t snd_ep93xx_pcm_pointer_capture(snd_pcm_substream_t *substream)
{
    audio_state_t *state = (audio_state_t *)(substream->private_data);
    snd_pcm_runtime_t *runtime = substream->runtime;
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

static int snd_ep93xx_pcm_open(snd_pcm_substream_t *substream)
{
    audio_state_t *state = substream->private_data;
    snd_pcm_runtime_t *runtime = substream->runtime;
    audio_stream_t *stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
                                state->output_stream:state->input_stream;

    DPRINTK("snd_ep93xx_pcm_open - enter\n");

    down(&state->sem);
            
    runtime->hw = ep93xx_i2s_pcm_hardware;

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
static int snd_ep93xx_pcm_close(snd_pcm_substream_t *substream)
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

static int snd_ep93xx_pcm_copy_playback(snd_pcm_substream_t * substream,int channel, 
				snd_pcm_uframes_t pos,void __user *src, snd_pcm_uframes_t count)
{

    audio_state_t *state = (audio_state_t *)substream->private_data;
    snd_pcm_runtime_t *runtime = substream->runtime;
    audio_stream_t *stream = state->output_stream ;
    audio_channel_t *dma_channel;
    int i;
    int tocount = frames_to_bytes(runtime,count);
    
    for( i = 0; i < stream->dma_num_channels; i++ ){

	dma_channel = &stream->dma_channels[i];	
	stream->hwbuf[i] = dma_channel->area + ( frames_to_bytes(runtime,pos) * stream->dma2usr_ratio / stream->dma_num_channels );
    
    }

    if(copy_from_user_with_conversion(stream ,(const char*)src,(tocount * stream->dma2usr_ratio)) <=0 ){
	DPRINTK(KERN_ERR "copy_from_user_with_conversion() failed\n");
	return -EFAULT;
    }

//printk("snd_ep93xx_pcm_copy_playback %x %x %x- exit\n",stream->dma_num_channels,stream->dma2usr_ratio,frames_to_bytes(runtime,pos));
					
    DPRINTK("snd_ep93xx_pcm_copy_playback - exit\n");
    return 0;
}


static int snd_ep93xx_pcm_copy_capture(snd_pcm_substream_t * substream,int channel, 
				snd_pcm_uframes_t pos,void __user *src, snd_pcm_uframes_t count)
{
    audio_state_t *state = (audio_state_t *)substream->private_data;
    snd_pcm_runtime_t *runtime = substream->runtime;
    audio_stream_t *stream = state->input_stream ;
    audio_channel_t *dma_channel;
    int i;
		       
    int tocount = frames_to_bytes(runtime,count);
			   
    for( i = 0; i < stream->dma_num_channels; i++ ){
  
	dma_channel = &stream->dma_channels[i];
	stream->hwbuf[i] = dma_channel->area + ( frames_to_bytes(runtime,pos) * stream->dma2usr_ratio / stream->dma_num_channels );

    }

    if(copy_to_user_with_conversion(stream,(const char*)src,tocount) <=0 ){

	DPRINTK(KERN_ERR "copy_to_user_with_conversion() failed\n");
	return -EFAULT;
    }
										       
    DPRINTK("snd_ep93xx_pcm_copy_capture - exit\n");
    return 0;
}
																					     
static snd_pcm_ops_t snd_ep93xx_pcm_playback_ops = {
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

static snd_pcm_ops_t snd_ep93xx_pcm_capture_ops = {
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

static int snd_ep93xx_pcm_new(snd_card_t *card, audio_state_t *state, snd_pcm_t **rpcm)
{
    snd_pcm_t *pcm;
    int play = state->output_stream? 1 : 0;/*SNDRV_PCM_STREAM_PLAYBACK*/
    int capt = state->input_stream ? 1 : 0;/*SNDRV_PCM_STREAM_CAPTURE*/
    int ret = 0;

    DPRINTK("snd_ep93xx_pcm_new - enter\n");
	
    /* Register the new pcm device interface */
    ret = snd_pcm_new(card, "EP93XX", 0, play, capt, &pcm);

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

/* mixer */


#ifdef CONFIG_CODEC_CS4228A

static int ep93xx_i2s_volume_info(snd_kcontrol_t *kcontrol,
                                        snd_ctl_elem_info_t * uinfo)
{
    int mask = 181;
    
    DPRINTK("%s \n",__FUNCTION__);
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 2;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = mask;
    uinfo->value.integer.step = 2;	       
    return 0;
}

static int ep93xx_i2s_volume_get(snd_kcontrol_t *kcontrol,
                            	    snd_ctl_elem_value_t * ucontrol)
{
    audio_state_t * state = (audio_state_t *)kcontrol->private_data;
    int max = 181;
 
    DPRINTK("%s ucontrol->value.integer.value[0]=%x ucontrol->value.integer.value[1]=%x\n",__FUNCTION__,state->playback_volume[0][0],state->playback_volume[0][1]);
 
    ucontrol->value.integer.value[0] = state->playback_volume[0][0] % (max + 1);/*state->playback_volume[0][0];*/
    ucontrol->value.integer.value[1] = state->playback_volume[0][1] % (max + 1);/*state->playback_volume[0][1];*/
    ucontrol->value.integer.value[2] = state->playback_volume[1][0] % (max + 1);/*state->playback_volume[0][0];*/
    ucontrol->value.integer.value[3] = state->playback_volume[1][1] % (max + 1);/*state->playback_volume[0][1];*/
    ucontrol->value.integer.value[4] = state->playback_volume[2][0] % (max + 1);
    ucontrol->value.integer.value[5] = state->playback_volume[2][1] % (max + 1);

    return 0;
}

static int ep93xx_i2s_volume_put(snd_kcontrol_t *kcontrol,
                                   snd_ctl_elem_value_t * ucontrol)
{
    int i;
    audio_state_t *state = (audio_state_t *) kcontrol->private_data;
    int valL,valR;
    int reg = 0x07;
    int max = 181;
    
    DPRINTK("%s \n",__FUNCTION__);
		
    state->playback_volume[0][0] = ucontrol->value.integer.value[0];
    state->playback_volume[0][1] = ucontrol->value.integer.value[1];
    state->playback_volume[1][0] = ucontrol->value.integer.value[2];
    state->playback_volume[1][1] = ucontrol->value.integer.value[3];
    state->playback_volume[2][0] = ucontrol->value.integer.value[4];
    state->playback_volume[2][1] = ucontrol->value.integer.value[5];

    for( i = 0; i < 3; i++ ){
    	if(state->playback_volume[i][0]>=max){
        	valL = 0;
		state->playback_volume[i][0] = max;
    	}
    	else{
        	valL = max - state->playback_volume[i][0];
    	}

    	if(state->playback_volume[i][1]>=max){
        	valR = 0;
        	state->playback_volume[i][1] = max;
    	}
    	else{
        	valR = max - state->playback_volume[i][1];
    	}


    	DPRINTK("%x  channle :L=%x,R=%x reg=%x\n",i,valL,valR,reg);
    	SSPDriver->Write( SSP_Handle, reg++, valL );
    	DPRINTK("reg=%x\n",reg);
    	SSPDriver->Write( SSP_Handle, reg++, valR );
    }
	    
    return 0;
}

#endif


					
static int ep93xx_i2s_volume_info_double(snd_kcontrol_t *kcontrol, 
					snd_ctl_elem_info_t * uinfo)
{

#ifdef CONFIG_CODEC_CS4271
    int mask = (kcontrol->private_value >> 16) & 0x7f;
    
    DPRINTK("%s %x\n",__FUNCTION__,mask);
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 1;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = mask;
    uinfo->value.integer.step = 1;
#else // CONFIG_ARCH_CS4228A
    int mask = (kcontrol->private_value >> 16);

    DPRINTK("%s max=%x\n",__FUNCTION__,mask); 

    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 2;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = mask;
    uinfo->value.integer.step = 2;
#endif    
    return 0;
}

static int ep93xx_i2s_volume_get_double(snd_kcontrol_t * kcontrol,
                                       snd_ctl_elem_value_t * ucontrol)
{

    audio_state_t * state = (audio_state_t *)kcontrol->private_data;
#ifdef CONFIG_CODEC_CS4271
    int mask = (kcontrol->private_value >> 16) & 0x7f;
    int val = 0;

    DPRINTK("%s %x\n",__FUNCTION__,mask);	
    val = state->playback_volume[0][0];
    ucontrol->value.integer.value[0] = val & mask;

#else // CONFIG_ARCH_CS4228A
    unsigned int idx = ucontrol->id.numid -2;
    int max = (kcontrol->private_value >> 16);

    DPRINTK("%s max=%x idex=%x %x %x\n",__FUNCTION__,max,idx,state->playback_volume[idx][0],state->playback_volume[idx][1]);
    
    ucontrol->value.integer.value[0] = state->playback_volume[idx][0] % (max + 1);
    ucontrol->value.integer.value[1] = state->playback_volume[idx][1] % (max + 1);
#endif
    
    return 0;
}

static int ep93xx_i2s_volume_put_double(snd_kcontrol_t * kcontrol,
                                       snd_ctl_elem_value_t * ucontrol)
{
    int reg1 = (kcontrol->private_value) & 0xf;
    int reg2 = (kcontrol->private_value >> 4) & 0xf;
    audio_state_t *state = (audio_state_t *) kcontrol->private_data;	
#ifdef CONFIG_CODEC_CS4271
	int mask = (kcontrol->private_value >> 16) & 0x7f;
	int val = 0;
	val = mask - (ucontrol->value.integer.value[0] & mask);
	state->playback_volume[0][0] = ucontrol->value.integer.value[0] & mask;
	
	DPRINTK("%s %x\n",__FUNCTION__,mask);
	DPRINTK("reg1 = 0x%x\n", reg1);
	DPRINTK("reg2 = 0x%x\n", reg2);
	DPRINTK("mask = %d\n", mask);
	DPRINTK("val = %d\n", val);
	DPRINTK("volume = %d\n", state->playback_volume[0][0]);
	
	
	SSPDriver->Write( SSP_Handle, reg1, val );
	SSPDriver->Write( SSP_Handle, reg2, val );

#else // CONFIG_ARCH_CS4228A
    //audio_state_t *state = (audio_state_t *) kcontrol->private_data;
    unsigned int idx = ucontrol->id.numid -2;
    int max = (kcontrol->private_value >> 16);
    int valL=0,valR=0;

    state->playback_volume[idx][0] = ucontrol->value.integer.value[0];
    state->playback_volume[idx][1] = ucontrol->value.integer.value[1];

	DPRINTK("%s max=%x idex=%x\n",__FUNCTION__,max,idx);
        DPRINTK("reg1 = 0x%x\n", reg1);
        DPRINTK("reg2 = 0x%x\n", reg2);

        if(state->playback_volume[idx][0]>=max){
                valL = 0;
                state->playback_volume[idx][0] = max;
        }
        else{
                valL = max - state->playback_volume[idx][0];
        }

        if(state->playback_volume[idx][1]>=max){
                valR = 0;
                state->playback_volume[idx][1] = max;
        }
        else{
                valR = max - state->playback_volume[idx][1];
        }

        SSPDriver->Write( SSP_Handle, reg1, valL );
        SSPDriver->Write( SSP_Handle, reg2, valR );
#endif

    return 0;
}

#ifdef CONFIG_CODEC_CS4228A

static int ep93xx_i2s_volume_info_single(snd_kcontrol_t *kcontrol,
                                        snd_ctl_elem_info_t * uinfo)
{
    int mask = (kcontrol->private_value >> 16);
    DPRINTK("%s \n",__FUNCTION__);				
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 1;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = mask;
    uinfo->value.integer.step = 1;
    return 0;
}
								
static int ep93xx_i2s_volume_get_single(snd_kcontrol_t * kcontrol,
                                       snd_ctl_elem_value_t * ucontrol)
{
    audio_state_t *state = (audio_state_t *) kcontrol->private_data;
    int idx = (kcontrol->private_value >> 4) & 0xf;
    int max = (kcontrol->private_value >> 16);

    DPRINTK("%s idex=%x\n",__FUNCTION__,idx);
    ucontrol->value.integer.value[0] = state->playback_volume[2][idx] % (max + 1);
    	   
    return 0;
}				       
				       
static int ep93xx_i2s_volume_put_single(snd_kcontrol_t * kcontrol,
                                       snd_ctl_elem_value_t * ucontrol)
{
    audio_state_t *state = (audio_state_t *) kcontrol->private_data;
    int reg = (kcontrol->private_value) & 0xf;
    int idx = (kcontrol->private_value >> 4) & 0xf;
    int max = (kcontrol->private_value >> 16);
    int valL=0;
    
    DPRINTK("%s \n",__FUNCTION__);

    state->playback_volume[2][idx] = ucontrol->value.integer.value[0];

        if(state->playback_volume[2][idx]>=max){
                valL = 0;
                state->playback_volume[2][idx] = max;
        }
        else{
                valL = max - state->playback_volume[2][idx];
        }

        SSPDriver->Write( SSP_Handle, reg, valL );
		   		
    return 0;
}				       
#endif


static snd_kcontrol_new_t snd_ep93xx_i2s_controls[]  = {
	
#ifdef CONFIG_CODEC_CS4271
    {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master",
	.info = ep93xx_i2s_volume_info_double,
        .get = ep93xx_i2s_volume_get_double,
        .put = ep93xx_i2s_volume_put_double,
        .private_value = ( 0x04 ) | ( 0x05 << 4 )| (0x7f << 16)
    }
#else // CONFIG_ARCH_CS4228A      
    {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Master",
	.info = ep93xx_i2s_volume_info,
        .get = ep93xx_i2s_volume_get,
        .put = ep93xx_i2s_volume_put,
    },
    {
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "PCM",
        .info = ep93xx_i2s_volume_info_double,
        .get = ep93xx_i2s_volume_get_double,
        .put = ep93xx_i2s_volume_put_double,
        .private_value = ( 0x07 ) | ( 0x08 << 4 ) | (0xb5 << 16)
    },
    {
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "Surround",
        .info = ep93xx_i2s_volume_info_double,
        .get = ep93xx_i2s_volume_get_double,
        .put = ep93xx_i2s_volume_put_double,
        .private_value = ( 0x09 ) | ( 0x0A << 4 ) | (0xb5 << 16)
    },
    {
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "Center",
        .info = ep93xx_i2s_volume_info_single,
        .get = ep93xx_i2s_volume_get_single,
        .put = ep93xx_i2s_volume_put_single,
        .private_value =  0x0B  | (0xb5 <<16)
    },
    {
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "LFE",
        .info = ep93xx_i2s_volume_info_single,
        .get = ep93xx_i2s_volume_get_single,
        .put = ep93xx_i2s_volume_put_single,
        .private_value = 0x0C | ( 1 << 4 ) | (0xb5 <<16)
    }
#endif							    
};

static int __init snd_ep93xx_mixer_new(snd_card_t *card, audio_state_t *state)
{
    int idx, err;
    
    snd_assert(card != NULL, return -EINVAL);
    
    strcpy(card->mixername, "Cirrus Logic Mixer");
    
    for (idx = 0; idx < ARRAY_SIZE(snd_ep93xx_i2s_controls); idx++) {
	if ((err = snd_ctl_add(card, snd_ctl_new1(&snd_ep93xx_i2s_controls[idx], state))) < 0)
	    return err;
    }

    return 0;
}

/* module init & exit */
static int snd_ep93xx_probe(struct platform_device *dev)
{
    snd_card_t *card;
    int err = -ENOMEM;
    struct resource *res = NULL;
            
    printk("snd_ep93xx_probe - enter\n");
	
    /* Open an instance of the SSP driver for I2S codec. */
    SSP_Handle = SSPDriver->Open(I2S_CODEC,0);

    if( SSP_Handle < 1 ){
	printk( KERN_ERR "Couldn't open SSP driver!\n");
	return -ENODEV;
    }
    /* Enable audio early on, give the DAC time to come up. */ 
    res = platform_get_resource( dev, IORESOURCE_MEM, 0);

    if(!res) {
	printk("error : platform_get_resource \n");
        return -ENODEV;
    }

    if (!request_mem_region(res->start,res->end - res->start + 1, "snd-cs4228a" ))
        return -EBUSY;
									    
    
    snd_ep93xx_audio_init(&audio_state);
			    
    /* register the soundcard */
    card = snd_card_new(index, id, THIS_MODULE, 0);
    
    card->dev = &dev->dev;

    if (card == NULL) {
	DPRINTK(KERN_ERR "snd_card_new() failed\n");
	goto error;
    }

#ifdef CONFIG_CODEC_CS4271
    strcpy(card->driver, "CS4271");
    strcpy(card->shortname, "Cirrus Logic I2S Audio ");
    strcpy(card->longname, "Cirrus Logic I2S Audio with CS4271");
#else
    strcpy(card->driver, "CS4228A");
    strcpy(card->shortname, "Cirrus Logic I2S Audio ");
    strcpy(card->longname, "Cirrus Logic I2S Audio with CS4228A");
#endif

    /* pcm */
    if (snd_ep93xx_pcm_new(card, &audio_state, &ep93xx_i2s_pcm) < 0)
	goto error;

    /* mixer */
    if (snd_ep93xx_mixer_new(card, &audio_state) < 0)
	goto error;

    if ((err = snd_card_register(card)) == 0) {
	printk( KERN_INFO "Cirrus Logic ep93xx i2s audio initialized\n" );
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

static int snd_ep93xx_remove(struct platform_device *dev)
{
    struct resource *res;
    snd_card_t *card = platform_get_drvdata(dev);

    res = platform_get_resource( dev, IORESOURCE_MEM, 0);
    release_mem_region(res->start, res->end - res->start + 1);
	
    DPRINTK("snd_ep93xx_remove - enter\n");
    SSPDriver->Close(SSP_Handle);
    
    if (card) {
	snd_card_free(card);
	platform_set_drvdata(dev, NULL);
    }
    DPRINTK("snd_ep93xx_remove - exit\n");

return 0;
}


static struct resource ep93xx_i2s_resources = {
    .start          = I2S_PHYS_BASE,
    .end            = I2S_PHYS_BASE + 0x6C,
    .flags          = IORESOURCE_MEM,
};

static struct platform_driver snd_ep93xx_driver = {
    .probe      = snd_ep93xx_probe,
    .remove     = snd_ep93xx_remove,
    .driver	= {
		    .name = AUDIO_NAME,
		    .bus  = &platform_bus_type,
		  },
};

static void snd_ep93xx_platform_release(struct device *device)
{
        // This is called when the reference count goes to zero.
}
	
static struct platform_device snd_ep93xx_device = {
    .name   	= AUDIO_NAME,
    .id     	= -1,
    .dev    	= {
    		     .release = snd_ep93xx_platform_release,
		  },
    .num_resources  = 1,
    .resource 	= &ep93xx_i2s_resources,
        		    
};						
						

static int __init snd_ep93xx_init(void)
{
    int ret;

    DPRINTK("snd_ep93xx_init - enter\n");	
    ret = platform_driver_register(&snd_ep93xx_driver);
    if (!ret) {
	ret = platform_device_register(&snd_ep93xx_device);
	if (ret)
	    platform_driver_unregister(&snd_ep93xx_driver);
    }
    DPRINTK("snd_ep93xx_init - exit\n");
    return ret;										
}

static void __exit snd_ep93xx_exit(void)
{
    DPRINTK("snd_ep93xx_exit - enter\n");
    platform_device_unregister(&snd_ep93xx_device);
    platform_driver_unregister(&snd_ep93xx_driver);
    DPRINTK("snd_ep93xx_exit - exit\n");
    
}

module_init(snd_ep93xx_init);
module_exit(snd_ep93xx_exit);

MODULE_DESCRIPTION("Cirrus Logic audio module");
MODULE_LICENSE("GPL");
