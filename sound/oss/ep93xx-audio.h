/*
 * ep93xx-audio.h
 *
 * Common audio handling for the Cirrus Logic EP93xx
 *
 * Copyright (c) 2003 Cirrus Logic Corp.
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * Taken from sa1100-audio.h
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

/*
 *  Getting this from the rme96xx driver.  Has same numerical value
 *  as AC3.  Hmmmm.
 */
#ifndef AFMT_S32_BLOCKED
#define AFMT_S32_BLOCKED 0x0000400
#endif

#define EP93XX_DEFAULT_NUM_CHANNELS      2
#define EP93XX_DEFAULT_FORMAT            AFMT_S16_LE
#define EP93XX_DEFAULT_BIT_WIDTH         16

/*
 * Buffer Management
 */

struct audio_buf_t {

	int size;		/* buffer size                                  */
	int sent;		/* indicates that dma has the dma_buffer        */
	char *start;		/* points to actual buffer                      */
	dma_addr_t dma_addr;	/* physical buffer address                      */
	int owner;
	struct semaphore sem;	/* down before touching the buffer              */
	int master;		/* owner for buffer allocation,                 */
	/* contains size when true                      */
	int num;		/* which out of nbfrags buffers is this?        */
	struct audio_stream_t *stream;	/* owning stream                                */

};

#define MAX_DEVICE_NAME 20

struct audio_stream_t {

	/* dma stuff */
	int dmahandles[3];	/* handles for dma driver instances     */
	char devicename[MAX_DEVICE_NAME];	/* string - name of device              */
	int NumDmaChannels;	/* 1, 2, or 3 DMA channels              */
	ep93xx_dma_dev_t dmachannel[3];	/* dma channel numbers                  */

	struct audio_buf_t *buffers;	/* array of audio buffer structures     */
	struct audio_buf_t *dma_buffer;	/* current buffer used by read/write    */
	u_int dma_buffer_index;	/* index for the pointer above...       */

	u_int fragsize;		/* fragment i.e. buffer size            */
	u_int nbfrags;		/* nbr of fragments i.e. buffers        */
	u_int requested_fragsize;
	u_int requested_nbfrags;

	int bytecount;		/* nbr of processed bytes               */
	int getptrCount;	/* value of bytecount last time         */
	/* anyone asked via GETxPTR             */
	int fragcount;		/* nbr of fragment transitions          */
	wait_queue_head_t wq;	/* for poll                             */
	int mapped:1;		/* mmap()'ed buffers                    */
	int active:1;		/* actually in progress                 */
	int stopped:1;		/* might be active but stopped          */
	u_int TX_dma_ch_bitfield;	/* bit0 = txdma1, bit1= txdma2, etc     */
	u_int RX_dma_ch_bitfield;	/* bit0 = rxdma1, bit1= rxdma2, etc     */
	int bFirstCaptureBuffer;	/* flag meaning there is junk in rx fifo */

	/*
	 * Data about the file format that we are configured to play.
	 */
	long audio_num_channels;	/* Range: 1 to 6 */
	long audio_format;
	long audio_stream_bitwidth;	/* Range: 8, 16, 24, 32 */

	long sample_rate;	/* Fs in Hz. */

	/*
	 * Data about how the audio controller and codec are set up.
	 */
	int hw_bit_width;	/* width of dac or adc */
	int bCompactMode;	/* set if 32bits == a stereo sample */

	/*
	 * dma2usr_ratio = dma sample size (8 or 4 bytes) / usr sample size (8,4,2,1)
	 */
	int dma2usr_ratio;

	/*
	 * This provides a way of determining audio playback position that will not
	 * overflow.  This is (size of buffers written) - (buffers we've gotten back)
	 */
	int buffered_bytes_to_play;

};

/*
 * No matter how many streams are going, we are using one hardware device.
 * This struct describes that hardware.
 */
struct audio_hw_t {

	ep93xx_dma_dev_t txdmachannels[3];	/* all dma channels available for */
	ep93xx_dma_dev_t rxdmachannels[3];	/* this audio ctrlr */

	int MaxTxDmaChannels;	/* number of tx and rx channels */
	int MaxRxDmaChannels;	/* number of tx and rx channels */

	/*
	 * Pointers to init, shutdown, and ioctl functions.
	 */
	void (*hw_enable) (struct audio_stream_t * stream);
	void (*hw_disable) (struct audio_stream_t * stream);
	void (*hw_clear_fifo) (struct audio_stream_t * stream);
	int (*client_ioctl) (struct inode *, struct file *, uint, ulong);
	void (*set_hw_serial_format) (struct audio_stream_t *, long);

	unsigned int modcnt;	/* count the # of volume writes */

};

/*
 * State structure for one instance
 */
struct audio_state_t {
	struct audio_stream_t *output_stream;
	struct audio_stream_t *input_stream;

	struct audio_hw_t *hw;	/* hw info */

	int rd_ref:1;		/* open reference for recording */
	int wr_ref:1;		/* open reference for playback */

	struct semaphore sem;	/* to protect against races in attach() */

};

/*
 * Functions exported by this module
 */
extern int ep93xx_audio_attach(struct inode *inode, struct file *file,
			       struct audio_state_t *state);
