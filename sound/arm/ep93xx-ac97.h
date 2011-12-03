/*
 * linux/sound/arm/ep93xx-i2s.c -- ALSA PCM interface for the edb93xx i2s audio
 *
 * Author:      Fred Wei
 * Created:     July 19, 2005
 * Copyright:   Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define EP93XX_DEFAULT_NUM_CHANNELS     2
#define EP93XX_DEFAULT_FORMAT           SNDRV_PCM_FORMAT_S16_LE
#define EP93XX_DEFAULT_BIT_WIDTH        16
#define MAX_DEVICE_NAME 		20

/*
 * Buffer Management
 */
				
typedef struct {

    unsigned char	*area;    	/* virtual pointer */
    dma_addr_t 		dma_addr;       /* physical address */
    size_t 			bytes;      
    size_t 			reportedbytes;	/* buffer size */
    int 			sent;		/* indicates that dma has the buf */
    char			*start;		/* points to actual buffer */

} audio_buf_t;


typedef struct {

    unsigned char	*area;  		/* virtual pointer */
    dma_addr_t 		addr;        		/* physical address */
    size_t 			bytes;          	/* buffer size in bytes */
    unsigned char   *buff_pos;              /* virtual pointer */
    audio_buf_t     *audio_buffers; 	/* array of audio buffer structures */
    int 			audio_buff_count;
		

} audio_channel_t;

typedef struct audio_stream_s {

    /* dma stuff */
    int					dmahandles[3];		/* handles for dma driver instances */
    char				devicename[MAX_DEVICE_NAME]; /* string - name of device */
    int					dma_num_channels;		/* 1, 2, or 3 DMA channels */
    audio_channel_t		*dma_channels;
    u_int 				nbfrags;		/* nbr of fragments i.e. buffers */
    u_int				fragsize;		/* fragment i.e. buffer size */
    u_int				dmasize;
    int 				bytecount;		/* nbr of processed bytes */
    int 				externedbytecount;	/* nbr of processed bytes */
    volatile int        active;                 /* actually in progress                 */
    volatile int        stopped;                /* might be active but stopped          */
    char 				*hwbuf[3];
    long				audio_rate;
    long 				audio_num_channels;		/* Range: 1 to 6 */
    int					audio_channels_flag;
    long 				audio_format;
    long 				audio_stream_bitwidth;		/* Range: 8, 16, 24 */
    int					dma2usr_ratio;

} audio_stream_t;


/*
 * State structure for one instance
 */
typedef struct {
	    
    audio_stream_t 		*output_stream;
    audio_stream_t 		*input_stream;
    ep93xx_dma_dev_t	output_dma[3];
    ep93xx_dma_dev_t	input_dma[3];
    char 				*output_id[3];
    char 				*input_id[3];
    struct              semaphore sem;          /* to protect against races in attach() */
    int					codec_set_by_playback;
    int                 codec_set_by_capture;
    int                 DAC_bit_width;          /* 16, 20, 24 bits */
    int                 bCompactMode;           /* set if 32bits = a stereo sample */
	
} audio_state_t;

