/*
 * linux/include/asm-arm/arch-ep93xx/ep93xx-regs.h
 */

#ifndef __ASM_ARCH_EP93XX_REGS_H
#define __ASM_ARCH_EP93XX_REGS_H

/*
 * EP93xx linux memory map:
 *
 * virt		phys		size
 * ff000000	80800000	2M		APB
 * ff80000	80000000	1M		AHB
 */
#define EP93XX_AHB_PHYS_BASE	0x80000000
#define EP93XX_AHB_VIRT_BASE	0xff000000
#define EP93XX_AHB_SIZE			0x00100000

#define EP93XX_APB_PHYS_BASE	0x80800000
#define EP93XX_APB_VIRT_BASE	0xff800000
#define EP93XX_APB_SIZE			0x00200000

#define IO_BASE_PHYS 			EP93XX_AHB_PHYS_BASE
#define IO_BASE_VIRT 			EP93XX_AHB_VIRT_BASE

/******************************************************************/
/*         EP93xx Memory Map and Register list                    */
/******************************************************************/
/*                                                                */
/* 0000_0000 - 0000_ffff: Internal ROM/nSDCE3/nCS0                */
/* 0001_0000 - 0fff_ffff: nSDCE3/nCS0                             */
/* 1000_0000 - 1fff_ffff: nCS1                                    */
/* 2000_0000 - 2fff_ffff: nCS2                                    */
/* 3000_0000 - 3fff_ffff: nCS3                                    */
/* 4000_0000 - 4fff_ffff: Reserved                                */
/* 5000_0000 - 5fff_ffff: Reserved                                */
/* 6000_0000 - 6fff_ffff: nCS6                                    */
/* 7000_0000 - 7fff_ffff: nCS7                                    */
/* 8000_0000 - 800f_ffff: AHB Registers                           */
/* 8010_0000 - 807f_ffff: Reserved                                */
/* 8080_0000 - 8fff_ffff: APB Registers                           */
/* 9000_0000 - bfff_ffff: Not Used                                */
/* c000_0000 - cfff_ffff: nSDCE0                                  */
/* d000_0000 - dfff_ffff: nSDCE1                                  */
/* e000_0000 - efff_ffff: nSDCE2                                  */
/* f000_0000 - ffff_ffff: nCS0/nSDCE3                             */
/*                                                                */
/******************************************************************/
#define EP9312_CS0_PHYS         0x00000000
#define EP9312_CS1_PHYS         0x10000000
#define EP9312_CS2_PHYS         0x20000000
#define EP9312_CS3_PHYS         0x30000000
#define EP9312_CS6_PHYS         0x60000000
#define EP9312_CS7_PHYS         0x70000000

#define EP9315_CS0_PHYS         0x00000000
#define EP9315_CS1_PHYS         0x10000000
#define EP9315_CS2_PHYS         0x20000000
#define EP9315_CS3_PHYS         0x30000000
#define EP9315_CS6_PHYS         0x60000000
#define EP9315_CS7_PHYS         0x70000000

/*
 * We don't map the PCMCIA initially.  The PCMCIA driver will use ioremap
 * to be able to see it.  But besides that PCMCIA will not exist in the
 * memory map.
 */
#define PCMCIA_BASE_VIRT    	0xD0000000     // Virtual address of PCMCIA
#define PCMCIA_BASE_PHYS    	0x40000000     // Physical address of PCMCIA
#define PCMCIA_SIZE         	0x10000000     // How much?

/*
 * We don't map the PCMCIA initially.  The PCMCIA driver will use ioremap
 * to be able to see it.  But besides that PCMCIA will not exist in the	       */
/* SMC register map                                                            */
/* Address     Read Location                   Write Location                  */
/* 0x8000.2000 SMCBCR0(Bank config register 0) SMCBCR0(Bank config register 0) */
/* 0x8000.2004 SMCBCR1(Bank config register 1) SMCBCR1(Bank config register 1) */
/* 0x8000.2008 SMCBCR2(Bank config register 2) SMCBCR2(Bank config register 2) */
/* 0x8000.200C SMCBCR3(Bank config register 3) SMCBCR3(Bank config register 3) */
/* 0x8000.2010 Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2014 Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2018 SMCBCR6(Bank config register 6) SMCBCR6(Bank config register 6) */
/* 0x8000.201C SMCBCR7(Bank config register 7) SMCBCR7(Bank config register 7) */
/* 0x8000.2020 PCAttribute Register            PCAttribute Register            */
/* 0x8000.2024 PCCommon Register               PCCommon Register               */
/* 0x8000.2028 PCIO Register                   PCIO Register                   */
/* 0x8000.202C Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2030 Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2034 Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2038 Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.203C Reserved, RAZ                   Reserved, RAZ                   */
/* 0x8000.2040 PCMCIACtrl Register             PCMCIACtrl Register             */
#define SRAM_OFFSET             0x080000
#define SRAM_BASE               (EP93XX_AHB_VIRT_BASE|SRAM_OFFSET)
#define SMCBCR0                 (SRAM_BASE+0x00) /* 0x8000.2000  Bank config register 0 */
#define SMCBCR1                 (SRAM_BASE+0x04) /* 0x8000.2004  Bank config register 1 */
#define SMCBCR2                 (SRAM_BASE+0x08) /* 0x8000.2008  Bank config register 2 */
#define SMCBCR3                 (SRAM_BASE+0x0C) /* 0x8000.200C  Bank config register 3 */
                                                 /* 0x8000.2010  Reserved, RAZ          */
                                                 /* 0x8000.2014  Reserved, RAZ          */
#define SMCBCR6                 (SRAM_BASE+0x18) /* 0x8000.2018  Bank config register 6 */
#define SMCBCR7                 (SRAM_BASE+0x1C) /* 0x8000.201C  Bank config register 7 */

#define SMC_PCAttribute         (SRAM_BASE+0x20) /* 0x8000.2020  PCMCIA Attribute Register */
#define SMC_PCCommon            (SRAM_BASE+0x24) /* 0x8000.2024  PCMCIA Common Register    */
#define SMC_PCIO                (SRAM_BASE+0x28) /* 0x8000.2028  PCMCIA IO Register        */
                                                 /* 0x8000.202C  Reserved, RAZ           */
                                                 /* 0x8000.2030  Reserved, RAZ           */
                                                 /* 0x8000.2034  Reserved, RAZ           */
                                                 /* 0x8000.2038  Reserved, RAZ           */
                                                 /* 0x8000.203C  Reserved, RAZ           */
#define SMC_PCMCIACtrl          (SRAM_BASE+0x40) /* 0x8000.2040  PCMCIA control register */


/* 8000_0000 - 8000_ffff: DMA  */
#define EP93XX_DMA_BASE			(EP93XX_AHB_VIRT_BASE + 0x00000000)
#define DMA_OFFSET              0x000000
#define DMA_BASE                (EP93XX_DMA_BASE)
#define DMAMP_TX_0_CONTROL      (DMA_BASE+0x0000)
#define DMAMP_TX_0_INTERRUPT    (DMA_BASE+0x0004)
#define DMAMP_TX_0_PPALLOC      (DMA_BASE+0x0008)
#define DMAMP_TX_0_STATUS       (DMA_BASE+0x000C)       
#define DMAMP_TX_0_REMAIN       (DMA_BASE+0x0014)
#define DMAMP_TX_0_MAXCNT0      (DMA_BASE+0x0020)
#define DMAMP_TX_0_BASE0        (DMA_BASE+0x0024)
#define DMAMP_TX_0_CURRENT0     (DMA_BASE+0x0028)
#define DMAMP_TX_0_MAXCNT1      (DMA_BASE+0x0030)
#define DMAMP_TX_0_BASE1        (DMA_BASE+0x0034)
#define DMAMP_TX_0_CURRENT1     (DMA_BASE+0x0038)

#define DMAMP_RX_1_CONTROL      (DMA_BASE+0x0040)
#define DMAMP_RX_1_INTERRUPT    (DMA_BASE+0x0044)
#define DMAMP_RX_1_PPALLOC      (DMA_BASE+0x0048)
#define DMAMP_RX_1_STATUS       (DMA_BASE+0x004C)       
#define DMAMP_RX_1_REMAIN       (DMA_BASE+0x0054)
#define DMAMP_RX_1_MAXCNT0      (DMA_BASE+0x0060)
#define DMAMP_RX_1_BASE0        (DMA_BASE+0x0064)
#define DMAMP_RX_1_CURRENT0     (DMA_BASE+0x0068)
#define DMAMP_RX_1_MAXCNT1      (DMA_BASE+0x0070)
#define DMAMP_RX_1_BASE1        (DMA_BASE+0x0074)
#define DMAMP_RX_1_CURRENT1     (DMA_BASE+0x0078)

#define DMAMP_TX_2_CONTROL      (DMA_BASE+0x0080)
#define DMAMP_TX_2_INTERRUPT    (DMA_BASE+0x0084)
#define DMAMP_TX_2_PPALLOC      (DMA_BASE+0x0088)
#define DMAMP_TX_2_STATUS       (DMA_BASE+0x008C)       
#define DMAMP_TX_2_REMAIN       (DMA_BASE+0x0094)
#define DMAMP_TX_2_MAXCNT0      (DMA_BASE+0x00A0)
#define DMAMP_TX_2_BASE0        (DMA_BASE+0x00A4)
#define DMAMP_TX_2_CURRENT0     (DMA_BASE+0x00A8)
#define DMAMP_TX_2_MAXCNT1      (DMA_BASE+0x00B0)
#define DMAMP_TX_2_BASE1        (DMA_BASE+0x00B4)
#define DMAMP_TX_2_CURRENT1     (DMA_BASE+0x00B8)

#define DMAMP_RX_3_CONTROL      (DMA_BASE+0x00C0)
#define DMAMP_RX_3_INTERRUPT    (DMA_BASE+0x00C4)
#define DMAMP_RX_3_PPALLOC      (DMA_BASE+0x00C8)
#define DMAMP_RX_3_STATUS       (DMA_BASE+0x00CC)       
#define DMAMP_RX_3_REMAIN       (DMA_BASE+0x00D4)
#define DMAMP_RX_3_MAXCNT0      (DMA_BASE+0x00E0)
#define DMAMP_RX_3_BASE0        (DMA_BASE+0x00E4)
#define DMAMP_RX_3_CURRENT0     (DMA_BASE+0x00E8)
#define DMAMP_RX_3_MAXCNT1      (DMA_BASE+0x00F0)
#define DMAMP_RX_3_BASE1        (DMA_BASE+0x00F4)
#define DMAMP_RX_3_CURRENT1     (DMA_BASE+0x00F8)

#define DMAMM_0_CONTROL         (DMA_BASE+0x0100)
#define DMAMM_0_INTERRUPT       (DMA_BASE+0x0104)
#define DMAMM_0_STATUS          (DMA_BASE+0x010C)
#define DMAMM_0_BCR0            (DMA_BASE+0x0110)
#define DMAMM_0_BCR1            (DMA_BASE+0x0114)
#define DMAMM_0_SAR_BASE0       (DMA_BASE+0x0118)
#define DMAMM_0_SAR_BASE1       (DMA_BASE+0x011C)
#define DMAMM_0_SAR_CURRENT0    (DMA_BASE+0x0124)
#define DMAMM_0_SAR_CURRENT1    (DMA_BASE+0x0128)
#define DMAMM_0_DAR_BASE0       (DMA_BASE+0x012C)
#define DMAMM_0_DAR_BASE1       (DMA_BASE+0x0130)
#define DMAMM_0_DAR_CURRENT0    (DMA_BASE+0x0134)
#define DMAMM_0_DAR_CURRENT1    (DMA_BASE+0x013C)

#define DMAMM_1_CONTROL         (DMA_BASE+0x0140)
#define DMAMM_1_INTERRUPT       (DMA_BASE+0x0144)
#define DMAMM_1_STATUS          (DMA_BASE+0x014C)
#define DMAMM_1_BCR0            (DMA_BASE+0x0150)       
#define DMAMM_1_BCR1            (DMA_BASE+0x0154)
#define DMAMM_1_SAR_BASE0       (DMA_BASE+0x0158)
#define DMAMM_1_SAR_BASE1       (DMA_BASE+0x015C)
#define DMAMM_1_SAR_CURRENT0    (DMA_BASE+0x0164)
#define DMAMM_1_SAR_CURRENT1    (DMA_BASE+0x0168)
#define DMAMM_1_DAR_BASE0       (DMA_BASE+0x016C)
#define DMAMM_1_DAR_BASE1       (DMA_BASE+0x0170)
#define DMAMM_1_DAR_CURRENT0    (DMA_BASE+0x0174)
#define DMAMM_1_DAR_CURRENT1    (DMA_BASE+0x017C)

#define DMAMP_RX_5_CONTROL      (DMA_BASE+0x0200)
#define DMAMP_RX_5_INTERRUPT    (DMA_BASE+0x0204)
#define DMAMP_RX_5_PPALLOC      (DMA_BASE+0x0208)
#define DMAMP_RX_5_STATUS       (DMA_BASE+0x020C)       
#define DMAMP_RX_5_REMAIN       (DMA_BASE+0x0214)
#define DMAMP_RX_5_MAXCNT0      (DMA_BASE+0x0220)
#define DMAMP_RX_5_BASE0        (DMA_BASE+0x0224)
#define DMAMP_RX_5_CURRENT0     (DMA_BASE+0x0228)
#define DMAMP_RX_5_MAXCNT1      (DMA_BASE+0x0230)
#define DMAMP_RX_5_BASE1        (DMA_BASE+0x0234)
#define DMAMP_RX_5_CURRENT1     (DMA_BASE+0x0238)

#define DMAMP_TX_4_CONTROL      (DMA_BASE+0x0240)
#define DMAMP_TX_4_INTERRUPT    (DMA_BASE+0x0244)
#define DMAMP_TX_4_PPALLOC      (DMA_BASE+0x0248)
#define DMAMP_TX_4_STATUS       (DMA_BASE+0x024C)       
#define DMAMP_TX_4_REMAIN       (DMA_BASE+0x0254)
#define DMAMP_TX_4_MAXCNT0      (DMA_BASE+0x0260)
#define DMAMP_TX_4_BASE0        (DMA_BASE+0x0264)
#define DMAMP_TX_4_CURRENT0     (DMA_BASE+0x0268)
#define DMAMP_TX_4_MAXCNT1      (DMA_BASE+0x0270)
#define DMAMP_TX_4_BASE1        (DMA_BASE+0x0274)
#define DMAMP_TX_4_CURRENT1     (DMA_BASE+0x0278)

#define DMAMP_RX_7_CONTROL      (DMA_BASE+0x0280)
#define DMAMP_RX_7_INTERRUPT    (DMA_BASE+0x0284)
#define DMAMP_RX_7_PPALLOC      (DMA_BASE+0x0288)
#define DMAMP_RX_7_STATUS       (DMA_BASE+0x028C)       
#define DMAMP_RX_7_REMAIN       (DMA_BASE+0x0294)
#define DMAMP_RX_7_MAXCNT0      (DMA_BASE+0x02A0)
#define DMAMP_RX_7_BASE0        (DMA_BASE+0x02A4)
#define DMAMP_RX_7_CURRENT0     (DMA_BASE+0x02A8)
#define DMAMP_RX_7_MAXCNT1      (DMA_BASE+0x02B0)
#define DMAMP_RX_7_BASE1        (DMA_BASE+0x02B4)
#define DMAMP_RX_7_CURRENT1     (DMA_BASE+0x02B8)

#define DMAMP_TX_6_CONTROL      (DMA_BASE+0x02C0)
#define DMAMP_TX_6_INTERRUPT    (DMA_BASE+0x02C4)
#define DMAMP_TX_6_PPALLOC      (DMA_BASE+0x02C8)
#define DMAMP_TX_6_STATUS       (DMA_BASE+0x02CC)       
#define DMAMP_TX_6_REMAIN       (DMA_BASE+0x02D4)
#define DMAMP_TX_6_MAXCNT0      (DMA_BASE+0x02E0)
#define DMAMP_TX_6_BASE0        (DMA_BASE+0x02E4)
#define DMAMP_TX_6_CURRENT0     (DMA_BASE+0x02E8)
#define DMAMP_TX_6_MAXCNT1      (DMA_BASE+0x02F0)
#define DMAMP_TX_6_BASE1        (DMA_BASE+0x02F4)
#define DMAMP_TX_6_CURRENT1     (DMA_BASE+0x02F8)

#define DMAMP_RX_9_CONTROL      (DMA_BASE+0x0300)
#define DMAMP_RX_9_INTERRUPT    (DMA_BASE+0x0304)
#define DMAMP_RX_9_PPALLOC      (DMA_BASE+0x0308)
#define DMAMP_RX_9_STATUS       (DMA_BASE+0x030C)
#define DMAMP_RX_9_REMAIN       (DMA_BASE+0x0314)
#define DMAMP_RX_9_MAXCNT0      (DMA_BASE+0x0320)
#define DMAMP_RX_9_BASE0        (DMA_BASE+0x0324)
#define DMAMP_RX_9_CURRENT0     (DMA_BASE+0x0328)
#define DMAMP_RX_9_MAXCNT1      (DMA_BASE+0x0330)
#define DMAMP_RX_9_BASE1        (DMA_BASE+0x0334)
#define DMAMP_RX_9_CURRENT1     (DMA_BASE+0x0338)

#define DMAMP_TX_8_CONTROL      (DMA_BASE+0x0340)
#define DMAMP_TX_8_INTERRUPT    (DMA_BASE+0x0344)
#define DMAMP_TX_8_PPALLOC      (DMA_BASE+0x0348)
#define DMAMP_TX_8_STATUS       (DMA_BASE+0x034C)
#define DMAMP_TX_8_REMAIN       (DMA_BASE+0x0354)
#define DMAMP_TX_8_MAXCNT0      (DMA_BASE+0x0360)
#define DMAMP_TX_8_BASE0        (DMA_BASE+0x0364)
#define DMAMP_TX_8_CURRENT0     (DMA_BASE+0x0368)
#define DMAMP_TX_8_MAXCNT1      (DMA_BASE+0x0370)
#define DMAMP_TX_8_BASE1        (DMA_BASE+0x0374)
#define DMAMP_TX_8_CURRENT1     (DMA_BASE+0x0378)

#define DMA_ARBITRATION         (DMA_BASE+0x0380)
#define DMA_INTERRUPT           (DMA_BASE+0x03C0)


/*
 * DMA Register Base addresses and Offsets
 */
#define DMA_M2P_TX_0_BASE       DMAMP_TX_0_CONTROL
#define DMA_M2P_RX_1_BASE       DMAMP_RX_1_CONTROL
#define DMA_M2P_TX_2_BASE       DMAMP_TX_2_CONTROL
#define DMA_M2P_RX_3_BASE       DMAMP_RX_3_CONTROL
#define DMA_M2M_0_BASE          DMAMM_0_CONTROL   
#define DMA_M2M_1_BASE          DMAMM_1_CONTROL   
#define DMA_M2P_RX_5_BASE       DMAMP_RX_5_CONTROL
#define DMA_M2P_TX_4_BASE       DMAMP_TX_4_CONTROL
#define DMA_M2P_RX_7_BASE       DMAMP_RX_7_CONTROL
#define DMA_M2P_TX_6_BASE       DMAMP_TX_6_CONTROL
#define DMA_M2P_RX_9_BASE       DMAMP_RX_9_CONTROL
#define DMA_M2P_TX_8_BASE       DMAMP_TX_8_CONTROL

#define M2P_OFFSET_CONTROL          0x0000
#define M2P_OFFSET_INTERRUPT        0x0004
#define M2P_OFFSET_PPALLOC          0x0008
#define M2P_OFFSET_STATUS           0x000C       
#define M2P_OFFSET_REMAIN           0x0014
#define M2P_OFFSET_MAXCNT0          0x0020
#define M2P_OFFSET_BASE0            0x0024
#define M2P_OFFSET_CURRENT0         0x0028
#define M2P_OFFSET_MAXCNT1          0x0030
#define M2P_OFFSET_BASE1            0x0034
#define M2P_OFFSET_CURRENT1         0x0038

#define M2M_OFFSET_CONTROL          0x0000
#define M2M_OFFSET_INTERRUPT        0x0004
#define M2M_OFFSET_STATUS           0x000C
#define M2M_OFFSET_BCR0             0x0010
#define M2M_OFFSET_BCR1             0x0014
#define M2M_OFFSET_SAR_BASE0        0x0018
#define M2M_OFFSET_SAR_BASE1        0x001C
#define M2M_OFFSET_SAR_CURRENT0     0x0024
#define M2M_OFFSET_SAR_CURRENT1     0x0028
#define M2M_OFFSET_DAR_BASE0        0x002C
#define M2M_OFFSET_DAR_BASE1        0x0030
#define M2M_OFFSET_DAR_CURRENT0     0x0034
#define M2M_OFFSET_DAR_CURRENT1     0x003C


/* 8003_0000 - 8003_ffff: Raster */
#define RASTER_OFFSET           0x030000
#define RASTER_BASE             (EP93XX_AHB_VIRT_BASE|RASTER_OFFSET)
#define VLINESTOTAL             (RASTER_BASE+0x00)
#define VSYNCSTRTSTOP           (RASTER_BASE+0x04)
#define VACTIVESTRTSTOP         (RASTER_BASE+0x08)
#define VCLKSTRTSTOP            (RASTER_BASE+0x0C)
#define HCLKSTOTAL              (RASTER_BASE+0x10)
#define HSYNCSTRTSTOP           (RASTER_BASE+0x14)
#define HACTIVESTRTSTOP         (RASTER_BASE+0x18)
#define HCLKSTRTSTOP            (RASTER_BASE+0x1C)
#define BRIGHTNESS              (RASTER_BASE+0x20)
#define VIDEOATTRIBS            (RASTER_BASE+0x24)
#define VIDSCRNPAGE             (RASTER_BASE+0x28)
#define VIDSCRNHPG              (RASTER_BASE+0x2C)
#define SCRNLINES               (RASTER_BASE+0x30)
#define LINELENGTH              (RASTER_BASE+0x34)
#define VLINESTEP               (RASTER_BASE+0x38)
#define LINECARRY               (RASTER_BASE+0x3C)
#define BLINKRATE               (RASTER_BASE+0x40)
#define BLINKMASK               (RASTER_BASE+0x44)
#define BLINKPATTRN             (RASTER_BASE+0x48)
#define PATTRNMASK              (RASTER_BASE+0x4C)
#define BG_OFFSET               (RASTER_BASE+0x50)
#define PIXELMODE               (RASTER_BASE+0x54)
#define PARLLIFOUT              (RASTER_BASE+0x58)
#define PARLLIFIN               (RASTER_BASE+0x5C)
#define CURSOR_ADR_START        (RASTER_BASE+0x60)
#define CURSOR_ADR_RESET        (RASTER_BASE+0x64)
#define CURSORSIZE              (RASTER_BASE+0x68)
#define CURSORCOLOR1            (RASTER_BASE+0x6C)
#define CURSORCOLOR2            (RASTER_BASE+0x70)
#define CURSORXYLOC             (RASTER_BASE+0x74)
#define CURSOR_DHSCAN_LH_YLOC   (RASTER_BASE+0x78)
#define RASTER_SWLOCK           (RASTER_BASE+0x7C)
#define GS_LUT                  (RASTER_BASE+0x80)
#define RASTER_TCR              (RASTER_BASE+0x100)
#define RASTER_TISRA            (RASTER_BASE+0x104)
#define RASTER_TISRB            (RASTER_BASE+0x108)
#define CURSOR_TISR             (RASTER_BASE+0x10C)
#define RASTER_TOCRA            (RASTER_BASE+0x110)
#define RASTER_TOCRB            (RASTER_BASE+0x114)
#define FIFO_TOCRA              (RASTER_BASE+0x118)
#define FIFO_TOCRB              (RASTER_BASE+0x11C)
#define BLINK_TISR              (RASTER_BASE+0x120)
#define DAC_TISRA               (RASTER_BASE+0x124)
#define DAC_TISRB               (RASTER_BASE+0x128)
#define SHIFT_TISR              (RASTER_BASE+0x12C)
#define DACMUX_TOCRA            (RASTER_BASE+0x130)
#define DACMUX_TOCRB            (RASTER_BASE+0x134)
#define PELMUX_TOCR             (RASTER_BASE+0x138)
#define VIDEO_TOCRA             (RASTER_BASE+0x13C)
#define VIDEO_TOCRB             (RASTER_BASE+0x140)
#define YCRCB_TOCR              (RASTER_BASE+0x144)
#define CURSOR_TOCR             (RASTER_BASE+0x148)
#define VIDEO_TOCRC             (RASTER_BASE+0x14C)
#define SHIFT_TOCR              (RASTER_BASE+0x150)
#define BLINK_TOCR              (RASTER_BASE+0x154)
#define RASTER_TCER             (RASTER_BASE+0x180)
#define SIGVAL                  (RASTER_BASE+0x200)
#define SIGCTL                  (RASTER_BASE+0x204)
#define VSIGSTRTSTOP            (RASTER_BASE+0x208)
#define HSIGSTRTSTOP            (RASTER_BASE+0x20C)
#define SIGCLR                  (RASTER_BASE+0x210)
#define ACRATE                  (RASTER_BASE+0x214)
#define LUTCONT                 (RASTER_BASE+0x218)
#define VBLANKSTRTSTOP          (RASTER_BASE+0x228)
#define HBLANKSTRTSTOP          (RASTER_BASE+0x22C)
#define LUT                     (RASTER_BASE+0x400)
#define CURSORBLINK1            (RASTER_BASE+0x21C)
#define CURSORBLINK2            (RASTER_BASE+0x220)
#define CURSORBLINK             (RASTER_BASE+0x224)
#define EOLOFFSET               (RASTER_BASE+0x230)
#define FIFOLEVEL               (RASTER_BASE+0x234)
#define GS_LUT2                 (RASTER_BASE+0x280)
#define GS_LUT3                 (RASTER_BASE+0x300)
#define COLOR_LUT               (RASTER_BASE+0x400)
 
/* 8004_0000 - 8004_ffff: Graphics */
#define GRAPHICS_OFFSET         0x040000
#define GRAPHICS_BASE           (EP93XX_AHB_VIRT_BASE|GRAPHICS_OFFSET)
#define SRCPIXELSTRT            (GRAPHICS_BASE+0x00)
#define DESTPIXELSTRT           (GRAPHICS_BASE+0x04)
#define BLKSRCSTRT              (GRAPHICS_BASE+0x08)
#define BLKDSTSTRT              (GRAPHICS_BASE+0x0C)
#define BLKSRCWIDTH             (GRAPHICS_BASE+0x10)
#define SRCLINELENGTH           (GRAPHICS_BASE+0x14)
#define BLKDESTWIDTH            (GRAPHICS_BASE+0x18)
#define BLKDESTHEIGHT           (GRAPHICS_BASE+0x1C)
#define DESTLINELENGTH          (GRAPHICS_BASE+0x20)
#define BLOCKCTRL               (GRAPHICS_BASE+0x24)
#define TRANSPATTRN             (GRAPHICS_BASE+0x28)
#define BLOCKMASK               (GRAPHICS_BASE+0x2C)
#define BACKGROUND              (GRAPHICS_BASE+0x30)
#define LINEINC                 (GRAPHICS_BASE+0x34)
#define LINEINIT                (GRAPHICS_BASE+0x38)
#define LINEPATTRN              (GRAPHICS_BASE+0x3C)

/* 800B_0000 - 800B_FFFF: VIC 0 */
#define VIC0_OFFSET              0x0B0000
#define VIC0_BASE                (EP93XX_AHB_VIRT_BASE|VIC0_OFFSET)
#define VIC0                     (VIC0_BASE+0x000) 
#define VIC0IRQSTATUS            (VIC0_BASE+0x000) /* R   IRQ status register               */
#define VIC0FIQSTATUS            (VIC0_BASE+0x004) /* R   FIQ status register               */
#define VIC0RAWINTR              (VIC0_BASE+0x008) /* R   Raw interrupt status register     */
#define VIC0INTSELECT            (VIC0_BASE+0x00C) /* R/W Interrupt select register         */
#define VIC0INTENABLE            (VIC0_BASE+0x010) /* R/W Interrupt enable register         */
#define VIC0INTENCLEAR           (VIC0_BASE+0x014) /* W   Interrupt enable clear register   */
#define VIC0SOFTINT              (VIC0_BASE+0x018) /* R/W Software interrupt register       */
#define VIC0SOFTINTCLEAR         (VIC0_BASE+0x01C) /* R/W Software interrupt clear register */
#define VIC0PROTECTION           (VIC0_BASE+0x020) /* R/W Protection enable register        */
#define VIC0VECTADDR             (VIC0_BASE+0x030) /* R/W Vector address register           */
#define VIC0DEFVECTADDR          (VIC0_BASE+0x034) /* R/W Default vector address register   */
#define VIC0VECTADDR00           (VIC0_BASE+0x100) /* R/W Vector address 00 register        */
#define VIC0VECTADDR01           (VIC0_BASE+0x104) /* R/W Vector address 01 register        */
#define VIC0VECTADDR02           (VIC0_BASE+0x108) /* R/W Vector address 02 register        */
#define VIC0VECTADDR03           (VIC0_BASE+0x10C) /* R/W Vector address 03 register        */
#define VIC0VECTADDR04           (VIC0_BASE+0x110) /* R/W Vector address 04 register        */
#define VIC0VECTADDR05           (VIC0_BASE+0x114) /* R/W Vector address 05 register        */
#define VIC0VECTADDR06           (VIC0_BASE+0x118) /* R/W Vector address 06 register        */
#define VIC0VECTADDR07           (VIC0_BASE+0x11C) /* R/W Vector address 07 register        */
#define VIC0VECTADDR08           (VIC0_BASE+0x120) /* R/W Vector address 08 register        */
#define VIC0VECTADDR09           (VIC0_BASE+0x124) /* R/W Vector address 09 register        */
#define VIC0VECTADDR10           (VIC0_BASE+0x128) /* R/W Vector address 10 register        */
#define VIC0VECTADDR11           (VIC0_BASE+0x12C) /* R/W Vector address 11 register        */
#define VIC0VECTADDR12           (VIC0_BASE+0x130) /* R/W Vector address 12 register        */
#define VIC0VECTADDR13           (VIC0_BASE+0x134) /* R/W Vector address 13 register        */
#define VIC0VECTADDR14           (VIC0_BASE+0x138) /* R/W Vector address 14 register        */
#define VIC0VECTADDR15           (VIC0_BASE+0x13C) /* R/W Vector address 15 register        */
#define VIC0VECTCNTL00           (VIC0_BASE+0x200) /* R/W Vector control 00 register        */
#define VIC0VECTCNTL01           (VIC0_BASE+0x204) /* R/W Vector control 01 register        */
#define VIC0VECTCNTL02           (VIC0_BASE+0x208) /* R/W Vector control 02 register        */
#define VIC0VECTCNTL03           (VIC0_BASE+0x20C) /* R/W Vector control 03 register        */
#define VIC0VECTCNTL04           (VIC0_BASE+0x210) /* R/W Vector control 04 register        */
#define VIC0VECTCNTL05           (VIC0_BASE+0x214) /* R/W Vector control 05 register        */
#define VIC0VECTCNTL06           (VIC0_BASE+0x218) /* R/W Vector control 06 register        */
#define VIC0VECTCNTL07           (VIC0_BASE+0x21C) /* R/W Vector control 07 register        */
#define VIC0VECTCNTL08           (VIC0_BASE+0x220) /* R/W Vector control 08 register        */
#define VIC0VECTCNTL09           (VIC0_BASE+0x224) /* R/W Vector control 09 register        */
#define VIC0VECTCNTL10           (VIC0_BASE+0x228) /* R/W Vector control 10 register        */
#define VIC0VECTCNTL11           (VIC0_BASE+0x22C) /* R/W Vector control 11 register        */
#define VIC0VECTCNTL12           (VIC0_BASE+0x230) /* R/W Vector control 12 register        */
#define VIC0VECTCNTL13           (VIC0_BASE+0x234) /* R/W Vector control 13 register        */
#define VIC0VECTCNTL14           (VIC0_BASE+0x238) /* R/W Vector control 14 register        */
#define VIC0VECTCNTL15           (VIC0_BASE+0x23C) /* R/W Vector control 15 register        */
#define VIC0ITCR                 (VIC0_BASE+0x300) /* R/W Test control register             */
#define VIC0ITIP1                (VIC0_BASE+0x304) /* R   Test input register (nVICIRQIN/nVICFIQIN)*/
#define VIC0ITIP2                (VIC0_BASE+0x308) /* R   Test input register (VICVECTADDRIN)      */
#define VIC0ITOP1                (VIC0_BASE+0x30C) /* R   Test output register (nVICIRQ/nVICFIQ)   */
#define VIC0ITOP2                (VIC0_BASE+0x310) /* R   Test output register (VICVECTADDROUT)    */
#define VIC0PERIPHID0            (VIC0_BASE+0xFE0) /* R   Peripheral ID register bits 7:0   */
#define VIC0PERIPHID1            (VIC0_BASE+0xFE4) /* R   Peripheral ID register bits 15:8  */
#define VIC0PERIPHID2            (VIC0_BASE+0xFE8) /* R   Peripheral ID register bits 23:16 */
#define VIC0PERIPHID3            (VIC0_BASE+0xFEC) /* R   Peripheral ID register bits 31:24 */

/* 800C_0000 - 800C_FFFF: VIC 0 */
#define VIC1_OFFSET              0x0C0000
#define VIC1_BASE                (EP93XX_AHB_VIRT_BASE|VIC1_OFFSET)
#define VIC1                     (VIC1_BASE+0x000) 
#define VIC1IRQSTATUS            (VIC1_BASE+0x000) /* R   IRQ status register               */
#define VIC1FIQSTATUS            (VIC1_BASE+0x004) /* R   FIQ status register               */
#define VIC1RAWINTR              (VIC1_BASE+0x008) /* R   Raw interrupt status register     */
#define VIC1INTSELECT            (VIC1_BASE+0x00C) /* R/W Interrupt select register         */
#define VIC1INTENABLE            (VIC1_BASE+0x010) /* R/W Interrupt enable register         */
#define VIC1INTENCLEAR           (VIC1_BASE+0x014) /* W   Interrupt enable clear register   */
#define VIC1SOFTINT              (VIC1_BASE+0x018) /* R/W Software interrupt register       */
#define VIC1SOFTINTCLEAR         (VIC1_BASE+0x01C) /* R/W Software interrupt clear register */
#define VIC1PROTECTION           (VIC1_BASE+0x020) /* R/W Protection enable register        */
#define VIC1VECTADDR             (VIC1_BASE+0x030) /* R/W Vector address register           */
#define VIC1DEFVECTADDR          (VIC1_BASE+0x034) /* R/W Default vector address register   */
#define VIC1VECTADDR00           (VIC1_BASE+0x100) /* R/W Vector address 00 register        */
#define VIC1VECTADDR01           (VIC1_BASE+0x104) /* R/W Vector address 01 register        */
#define VIC1VECTADDR02           (VIC1_BASE+0x108) /* R/W Vector address 02 register        */
#define VIC1VECTADDR03           (VIC1_BASE+0x10C) /* R/W Vector address 03 register        */
#define VIC1VECTADDR04           (VIC1_BASE+0x110) /* R/W Vector address 04 register        */
#define VIC1VECTADDR05           (VIC1_BASE+0x114) /* R/W Vector address 05 register        */
#define VIC1VECTADDR06           (VIC1_BASE+0x118) /* R/W Vector address 06 register        */
#define VIC1VECTADDR07           (VIC1_BASE+0x11C) /* R/W Vector address 07 register        */
#define VIC1VECTADDR08           (VIC1_BASE+0x120) /* R/W Vector address 08 register        */
#define VIC1VECTADDR09           (VIC1_BASE+0x124) /* R/W Vector address 09 register        */
#define VIC1VECTADDR10           (VIC1_BASE+0x128) /* R/W Vector address 10 register        */
#define VIC1VECTADDR11           (VIC1_BASE+0x12C) /* R/W Vector address 11 register        */
#define VIC1VECTADDR12           (VIC1_BASE+0x130) /* R/W Vector address 12 register        */
#define VIC1VECTADDR13           (VIC1_BASE+0x134) /* R/W Vector address 13 register        */
#define VIC1VECTADDR14           (VIC1_BASE+0x138) /* R/W Vector address 14 register        */
#define VIC1VECTADDR15           (VIC1_BASE+0x13C) /* R/W Vector address 15 register        */
#define VIC1VECTCNTL00           (VIC1_BASE+0x200) /* R/W Vector control 00 register        */
#define VIC1VECTCNTL01           (VIC1_BASE+0x204) /* R/W Vector control 01 register        */
#define VIC1VECTCNTL02           (VIC1_BASE+0x208) /* R/W Vector control 02 register        */
#define VIC1VECTCNTL03           (VIC1_BASE+0x20C) /* R/W Vector control 03 register        */
#define VIC1VECTCNTL04           (VIC1_BASE+0x210) /* R/W Vector control 04 register        */
#define VIC1VECTCNTL05           (VIC1_BASE+0x214) /* R/W Vector control 05 register        */
#define VIC1VECTCNTL06           (VIC1_BASE+0x218) /* R/W Vector control 06 register        */
#define VIC1VECTCNTL07           (VIC1_BASE+0x21C) /* R/W Vector control 07 register        */
#define VIC1VECTCNTL08           (VIC1_BASE+0x220) /* R/W Vector control 08 register        */
#define VIC1VECTCNTL09           (VIC1_BASE+0x224) /* R/W Vector control 09 register        */
#define VIC1VECTCNTL10           (VIC1_BASE+0x228) /* R/W Vector control 10 register        */
#define VIC1VECTCNTL11           (VIC1_BASE+0x22C) /* R/W Vector control 11 register        */
#define VIC1VECTCNTL12           (VIC1_BASE+0x230) /* R/W Vector control 12 register        */
#define VIC1VECTCNTL13           (VIC1_BASE+0x234) /* R/W Vector control 13 register        */
#define VIC1VECTCNTL14           (VIC1_BASE+0x238) /* R/W Vector control 14 register        */
#define VIC1VECTCNTL15           (VIC1_BASE+0x23C) /* R/W Vector control 15 register        */
#define VIC1ITCR                 (VIC1_BASE+0x300) /* R/W Test control register             */
#define VIC1ITIP1                (VIC1_BASE+0x304) /* R   Test input register (nVICIRQIN/nVICFIQIN)*/
#define VIC1ITIP2                (VIC1_BASE+0x308) /* R   Test input register (VICVECTADDRIN)      */
#define VIC1ITOP1                (VIC1_BASE+0x30C) /* R   Test output register (nVICIRQ/nVICFIQ)   */
#define VIC1ITOP2                (VIC1_BASE+0x310) /* R   Test output register (VICVECTADDROUT)    */
#define VIC1PERIPHID0            (VIC1_BASE+0xFE0) /* R   Peripheral ID register bits 7:0   */
#define VIC1PERIPHID1            (VIC1_BASE+0xFE4) /* R   Peripheral ID register bits 15:8  */
#define VIC1PERIPHID2            (VIC1_BASE+0xFE8) /* R   Peripheral ID register bits 23:16 */
#define VIC1PERIPHID3            (VIC1_BASE+0xFEC) /* R   Peripheral ID register bits 31:24 */


///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////APB/////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////
/* 8081_0000 - 8081_ffff: Timers */
#define TIMERS_OFFSET           0x010000
#define TIMERS_BASE             (EP93XX_APB_VIRT_BASE|TIMERS_OFFSET)

#define TIMER1LOAD              (TIMERS_BASE+0x00)
#define TIMER1VALUE             (TIMERS_BASE+0x04)
#define TIMER1CONTROL           (TIMERS_BASE+0x08)
#define TIMER1CLEAR             (TIMERS_BASE+0x0C)
#define TIMER1TEST              (TIMERS_BASE+0x10)

#define TIMER2LOAD              (TIMERS_BASE+0x20)
#define TIMER2VALUE             (TIMERS_BASE+0x24)
#define TIMER2CONTROL           (TIMERS_BASE+0x28)
#define TIMER2CLEAR             (TIMERS_BASE+0x2C)
#define TIMER2TEST              (TIMERS_BASE+0x30)

#define TIMER3LOAD              (TIMERS_BASE+0x80)
#define TIMER3VALUE             (TIMERS_BASE+0x84)
#define TIMER3CONTROL           (TIMERS_BASE+0x88)
#define TIMER3CLEAR             (TIMERS_BASE+0x8C)
#define TIMER3TEST              (TIMERS_BASE+0x90)

#define TTIMERBZCONT            (TIMERS_BASE+0x40)

#define TIMER4VALUELOW          (TIMERS_BASE+0x60)
#define TIMER4VALUEHIGH         (TIMERS_BASE+0x64)

/* 8082_0000 - 8082_ffff: I2S */
#define I2S_OFFSET            0x020000
#define I2S_BASE              (EP93XX_APB_VIRT_BASE|I2S_OFFSET)
#define I2S_PHYS_BASE         (EP93XX_APB_PHYS_BASE + I2S_OFFSET)

#define I2STxClkCfg           (I2S_BASE+0x00) /* 8082.0000 R/W Transmitter clock config register  */
#define I2SRxClkCfg           (I2S_BASE+0x04) /* 8082.0004 R/W Receiver clock config register     */
#define I2SGlSts              (I2S_BASE+0x08) /* 8082.0008 R/W SAI Global Status register.        */
#define I2SGlCtrl             (I2S_BASE+0x0C) /* 8082.000C R/W SAI Global Control register        */

#define I2STX0Lft             (I2S_BASE+0x10) /* 8082.0010 R/W Left  TX data reg for channel 0    */
#define I2STX0Rt              (I2S_BASE+0x14) /* 8082.0014 R/W Right TX data reg for channel 0    */
#define I2STX1Lft             (I2S_BASE+0x18) /* 8082.0018 R/W Left  TX data reg for channel 1    */
#define I2STX1Rt              (I2S_BASE+0x1C) /* 8082.001C R/W Right TX data reg for channel 1    */
#define I2STX2Lft             (I2S_BASE+0x20) /* 8082.0020 R/W Left  TX data reg for channel 2    */
#define I2STX2Rt              (I2S_BASE+0x24) /* 8082.0024 R/W Right TX data reg for channel 2    */

#define I2STXLinCtrlData      (I2S_BASE+0x28) /* 8082.0028 R/W TX Line Control data register      */
#define I2STXCtrl             (I2S_BASE+0x2C) /* 8082.002C R/W TX Control register                */
#define I2STXWrdLen           (I2S_BASE+0x30) /* 8082.0030 R/W TX Word Length                     */
#define I2STX0En              (I2S_BASE+0x34) /* 8082.0034 R/W TX0 Channel Enable                 */
#define I2STX1En              (I2S_BASE+0x38) /* 8082.0038 R/W TX1 Channel Enable                 */
#define I2STX2En              (I2S_BASE+0x3C) /* 8082.003C R/W TX2 Channel Enable                 */

#define I2SRX0Lft             (I2S_BASE+0x40) /* 8082.0040 R   Left  RX data reg for channel 0    */
#define I2SRX0Rt              (I2S_BASE+0x44) /* 8082.0044 R   Right RX data reg for channel 0    */
#define I2SRX1Lft             (I2S_BASE+0x48) /* 8082.0048 R   Left  RX data reg for channel 1    */
#define I2SRX1Rt              (I2S_BASE+0x4C) /* 8082.004c R   Right RX data reg for channel 1    */
#define I2SRX2Lft             (I2S_BASE+0x50) /* 8082.0050 R   Left  RX data reg for channel 2    */
#define I2SRX2Rt              (I2S_BASE+0x54) /* 8082.0054 R   Right RX data reg for channel 2    */

#define I2SRXLinCtrlData      (I2S_BASE+0x58) /* 8082.0058 R/W RX Line Control data register      */
#define I2SRXCtrl             (I2S_BASE+0x5C) /* 8082.005C R/W RX Control register                */
#define I2SRXWrdLen           (I2S_BASE+0x60) /* 8082.0060 R/W RX Word Length                     */
#define I2SRX0En              (I2S_BASE+0x64) /* 8082.0064 R/W RX0 Channel Enable                 */
#define I2SRX1En              (I2S_BASE+0x68) /* 8082.0068 R/W RX1 Channel Enable                 */
#define I2SRX2En              (I2S_BASE+0x6C) /* 8082.006C R/W RX2 Channel Enable                 */

/* 8084_0000 - 8084_ffff: GPIO */
#define GPIO_OFFSET              0x040000
#define GPIO_BASE                (EP93XX_APB_VIRT_BASE|GPIO_OFFSET)
#define GPIO_PADR                (GPIO_BASE+0x00)
#define GPIO_PBDR                (GPIO_BASE+0x04)
#define GPIO_PCDR                (GPIO_BASE+0x08)
#define GPIO_PDDR                (GPIO_BASE+0x0C)
#define GPIO_PADDR               (GPIO_BASE+0x10)
#define GPIO_PBDDR               (GPIO_BASE+0x14)
#define GPIO_PCDDR               (GPIO_BASE+0x18)
#define GPIO_PDDDR               (GPIO_BASE+0x1C)
#define GPIO_PEDR                (GPIO_BASE+0x20)
#define GPIO_PEDDR               (GPIO_BASE+0x24)
// #define 0x8084.0028 Reserved
// #define 0x8084.002C Reserved
#define GPIO_PFDR                (GPIO_BASE+0x30) 
#define GPIO_PFDDR               (GPIO_BASE+0x34)
#define GPIO_PGDR                (GPIO_BASE+0x38)
#define GPIO_PGDDR               (GPIO_BASE+0x3C)
#define GPIO_PHDR                (GPIO_BASE+0x40)
#define GPIO_PHDDR               (GPIO_BASE+0x44)
// #define 0x8084.0048 RAZ RAZ                            
#define GPIO_FINTTYPE1           (GPIO_BASE+0x4C)
#define GPIO_FINTTYPE2           (GPIO_BASE+0x50)
#define GPIO_FEOI                (GPIO_BASE+0x54) /* WRITE ONLY - READ UNDEFINED */
#define GPIO_FINTEN              (GPIO_BASE+0x58)
#define GPIO_INTSTATUSF          (GPIO_BASE+0x5C)
#define GPIO_RAWINTSTASUSF       (GPIO_BASE+0x60) 
#define GPIO_FDB                 (GPIO_BASE+0x64)
#define GPIO_PAPINDR             (GPIO_BASE+0x68)
#define GPIO_PBPINDR             (GPIO_BASE+0x6C)
#define GPIO_PCPINDR             (GPIO_BASE+0x70)
#define GPIO_PDPINDR             (GPIO_BASE+0x74)
#define GPIO_PEPINDR             (GPIO_BASE+0x78)
#define GPIO_PFPINDR             (GPIO_BASE+0x7C)
#define GPIO_PGPINDR             (GPIO_BASE+0x80)
#define GPIO_PHPINDR             (GPIO_BASE+0x84)
#define GPIO_AINTTYPE1           (GPIO_BASE+0x90)
#define GPIO_AINTTYPE2           (GPIO_BASE+0x94)
#define GPIO_AEOI                (GPIO_BASE+0x98) /* WRITE ONLY - READ UNDEFINED */
#define GPIO_AINTEN              (GPIO_BASE+0x9C)
#define GPIO_INTSTATUSA          (GPIO_BASE+0xA0)
#define GPIO_RAWINTSTSTISA       (GPIO_BASE+0xA4)
#define GPIO_ADB                 (GPIO_BASE+0xA8)
#define GPIO_BINTTYPE1           (GPIO_BASE+0xAC)
#define GPIO_BINTTYPE2           (GPIO_BASE+0xB0)
#define GPIO_BEOI                (GPIO_BASE+0xB4) /* WRITE ONLY - READ UNDEFINED */
#define GPIO_BINTEN              (GPIO_BASE+0xB8)
#define GPIO_INTSTATUSB          (GPIO_BASE+0xBC)
#define GPIO_RAWINTSTSTISB       (GPIO_BASE+0xC0)
#define GPIO_BDB                 (GPIO_BASE+0xC4)
#define GPIO_EEDRIVE             (GPIO_BASE+0xC8)
//#define Reserved               (GPIO_BASE+0xCC)
#define GPIO_TCR                 (GPIO_BASE+0xD0) /* Test Registers */
#define GPIO_TISRA               (GPIO_BASE+0xD4) /* Test Registers */
#define GPIO_TISRB               (GPIO_BASE+0xD8) /* Test Registers */
#define GPIO_TISRC               (GPIO_BASE+0xDC) /* Test Registers */
#define GPIO_TISRD               (GPIO_BASE+0xE0) /* Test Registers */
#define GPIO_TISRE               (GPIO_BASE+0xE4) /* Test Registers */
#define GPIO_TISRF               (GPIO_BASE+0xE8) /* Test Registers */
#define GPIO_TISRG               (GPIO_BASE+0xEC) /* Test Registers */
#define GPIO_TISRH               (GPIO_BASE+0xF0) /* Test Registers */
#define GPIO_TCER                (GPIO_BASE+0xF4) /* Test Registers */

/* 8088_0000 - 8088_ffff: Ac97 Controller (AAC) */
#define AC97_OFFSET             0x080000
#define AC97_BASE               (EP93XX_APB_VIRT_BASE|AC97_OFFSET)
#define EP93XX_AC97_PHY_BASE    (EP93XX_APB_PHYS_BASE|AC97_OFFSET)
#define AC97DR1                 (AC97_BASE+0x00) /* 8088.0000 R/W Data read or written from/to FIFO1  */
#define AC97RXCR1               (AC97_BASE+0x04) /* 8088.0004 R/W Control register for receive        */
#define AC97TXCR1               (AC97_BASE+0x08) /* 8088.0008 R/W Control register for transmit       */
#define AC97SR1                 (AC97_BASE+0x0C) /* 8088.000C R   Status register                     */
#define AC97RISR1               (AC97_BASE+0x10) /* 8088.0010 R   Raw interrupt status register       */
#define AC97ISR1                (AC97_BASE+0x14) /* 8088.0014 R   Interrupt Status                    */
#define AC97IE1                 (AC97_BASE+0x18) /* 8088.0018 R/W Interrupt Enable                    */
                                                               /* 8088.001C Reserved - RAZ                          */
#define AC97DR2                 (AC97_BASE+0x20) /* 8088.0020 R/W Data read or written from/to FIFO2  */
#define AC97RXCR2               (AC97_BASE+0x24) /* 8088.0024 R/W Control register for receive        */
#define AC97TXCR2               (AC97_BASE+0x28) /* 8088.0028 R/W Control register for transmit       */
#define AC97SR2                 (AC97_BASE+0x2C) /* 8088.002C R   Status register                     */
#define AC97RISR2               (AC97_BASE+0x30) /* 8088.0030 R   Raw interrupt status register       */
#define AC97ISR2                (AC97_BASE+0x34) /* 8088.0034 R   Interrupt Status                    */
#define AC97IE2                 (AC97_BASE+0x38) /* 8088.0038 R/W Interrupt Enable                    */
                                                               /* 8088.003C Reserved - RAZ                          */
#define AC97DR3                 (AC97_BASE+0x40) /* 8088.0040 R/W Data read or written from/to FIFO3. */
#define AC97RXCR3               (AC97_BASE+0x44) /* 8088.0044 R/W Control register for receive        */
#define AC97TXCR3               (AC97_BASE+0x48) /* 8088.0048 R/W Control register for transmit       */
#define AC97SR3                 (AC97_BASE+0x4C) /* 8088.004C R   Status register                     */
#define AC97RISR3               (AC97_BASE+0x50) /* 8088.0050 R   Raw interrupt status register       */
#define AC97ISR3                (AC97_BASE+0x54) /* 8088.0054 R   Interrupt Status                    */
#define AC97IE3                 (AC97_BASE+0x58) /* 8088.0058 R/W Interrupt Enable                    */
                                                               /* 8088.005C Reserved - RAZ                          */
#define AC97DR2                 (AC97_BASE+0x20) /* 8088.0020 R/W Data read or written from/to FIFO2  */
#define AC97RXCR2               (AC97_BASE+0x24) /* 8088.0024 R/W Control register for receive        */
#define AC97TXCR2               (AC97_BASE+0x28) /* 8088.0028 R/W Control register for transmit       */
#define AC97SR2                 (AC97_BASE+0x2C) /* 8088.002C R   Status register                     */
#define AC97RISR2               (AC97_BASE+0x30) /* 8088.0030 R   Raw interrupt status register       */
#define AC97ISR2                (AC97_BASE+0x34) /* 8088.0034 R   Interrupt Status                    */
#define AC97IE2                 (AC97_BASE+0x38) /* 8088.0038 R/W Interrupt Enable                    */
                                                               /* 8088.003C Reserved - RAZ                          */
#define AC97DR3                 (AC97_BASE+0x40) /* 8088.0040 R/W Data read or written from/to FIFO3. */
#define AC97RXCR3               (AC97_BASE+0x44) /* 8088.0044 R/W Control register for receive        */
#define AC97TXCR3               (AC97_BASE+0x48) /* 8088.0048 R/W Control register for transmit       */
#define AC97SR3                 (AC97_BASE+0x4C) /* 8088.004C R   Status register                     */
#define AC97RISR3               (AC97_BASE+0x50) /* 8088.0050 R   Raw interrupt status register       */
#define AC97ISR3                (AC97_BASE+0x54) /* 8088.0054 R   Interrupt Status                    */
#define AC97IE3                 (AC97_BASE+0x58) /* 8088.0058 R/W Interrupt Enable                    */
                                                               /* 8088.005C Reserved - RAZ                          */
#define AC97DR4                 (AC97_BASE+0x60) /* 8088.0060 R/W Data read or written from/to FIFO4. */
#define AC97RXCR4               (AC97_BASE+0x64) /* 8088.0064 R/W Control register for receive        */
#define AC97TXCR4               (AC97_BASE+0x68) /* 8088.0068 R/W Control register for transmit       */
#define AC97SR4                 (AC97_BASE+0x6C) /* 8088.006C R   Status register                     */
#define AC97RISR4               (AC97_BASE+0x70) /* 8088.0070 R   Raw interrupt status register       */
#define AC97ISR4                (AC97_BASE+0x74) /* 8088.0074 R   Interrupt Status                    */
#define AC97IE4                 (AC97_BASE+0x78) /* 8088.0078 R/W Interrupt Enable                    */
                                                               /* 8088.007C Reserved - RAZ                          */
#define AC97S1DATA              (AC97_BASE+0x80) /* 8088.0080 R/W Data received/transmitted on SLOT1  */
#define AC97S2DATA              (AC97_BASE+0x84) /* 8088.0084 R/W Data received/transmitted on SLOT2  */
#define AC97S12DATA             (AC97_BASE+0x88) /* 8088.0088 R/W Data received/transmitted on SLOT12 */
#define AC97RGIS                (AC97_BASE+0x8C) /* 8088.008C R/W Raw Global interrupt status register*/
#define AC97GIS                 (AC97_BASE+0x90) /* 8088.0090 R   Global interrupt status register    */
#define AC97IM                  (AC97_BASE+0x94) /* 8088.0094 R/W Interrupt mask register             */
#define AC97EOI                 (AC97_BASE+0x98) /* 8088.0098 W   Interrupt clear register            */
#define AC97GCR                 (AC97_BASE+0x9C) /* 8088.009C R/W Main Control register               */
#define AC97RESET               (AC97_BASE+0xA0) /* 8088.00A0 R/W RESET control register.             */
#define AC97SYNC                (AC97_BASE+0xA4) /* 8088.00A4 R/W SYNC control register.              */
#define AC97GCIS                (AC97_BASE+0xA8) /* 8088.00A8 R  Global chan FIFO int status register */

/* 808A_0000 - 808A_ffff: SSP - (SPI) */
#define SSP_OFFSET             0x0A0000
#define SSP_BASE               (EP93XX_APB_VIRT_BASE|SSP_OFFSET)
#define SSPCR0                 (SSP_BASE+0x00)
#define SSPCR1                 (SSP_BASE+0x04)
#define SSPDR                  (SSP_BASE+0x08)
#define SSPSR                  (SSP_BASE+0x0c)
#define SSPCPSR                (SSP_BASE+0x10)
#define SSPIIR                 (SSP_BASE+0x14)

/*808B_0000 - 808B_ffff: IrDA */
#define IRDA_OFFSET             0x0B0000
#define IRDA_BASE               (EP93XX_APB_VIRT_BASE|IRDA_OFFSET)
#define IrEnable                (IRDA_BASE+0x00)
#define IrCtrl                  (IRDA_BASE+0x04)
#define IrAdrMatchVal           (IRDA_BASE+0x08)
#define IrFlag                  (IRDA_BASE+0x0C)
#define IrData                  (IRDA_BASE+0x10)
#define IrDataTail1             (IRDA_BASE+0x14)
#define IrDataTail2             (IRDA_BASE+0x18)
#define IrDataTail3             (IRDA_BASE+0x1c)
#define IrRIB                   (IRDA_BASE+0x20)
#define IrTR0                   (IRDA_BASE+0x24)
#define IrDMACR                 (IRDA_BASE+0x28)
#define SIRTR0                  (IRDA_BASE+0x30)
#define MISR                    (IRDA_BASE+0x80)
#define MIMR                    (IRDA_BASE+0x84)
#define MIIR                    (IRDA_BASE+0x88)
#define FISR                    (IRDA_BASE+0x180)
#define FIMR                    (IRDA_BASE+0x184)
#define FIIR                    (IRDA_BASE+0x188)

/* 808C_0000 - 808C_ffff: UART1 */
#define UART1_OFFSET            0x0C0000
#define UART1_BASE              (EP93XX_APB_VIRT_BASE|UART1_OFFSET)
#define UART1_BASE_VIRT         (EP93XX_APB_PHYS_BASE|UART1_OFFSET)
#define UART1DR                 (UART1_BASE+0x000)
#define UART1RSR                (UART1_BASE+0x004)
#define UART1ECR                (UART1_BASE+0x004)
#define UART1CR_H               (UART1_BASE+0x008)
#define UART1CR_M               (UART1_BASE+0x00C)
#define UART1CR_L               (UART1_BASE+0x010)
#define UART1CR                 (UART1_BASE+0x014)
#define UART1FR                 (UART1_BASE+0x018)
#define UART1IIR                (UART1_BASE+0x01C)
#define UART1ICR                (UART1_BASE+0x01C)
#define UART1ILPR               (UART1_BASE+0x020)
#define UART1DMACR              (UART1_BASE+0x028)
#define UART1TMR                (UART1_BASE+0x084)
#define UART1MCR                (UART1_BASE+0x100)
#define UART1MSR                (UART1_BASE+0x104)
#define UART1TCR                (UART1_BASE+0x108)
#define UART1TISR               (UART1_BASE+0x10C)
#define UART1TOCR               (UART1_BASE+0x110)
#define HDLC1CR                 (UART1_BASE+0x20c)
#define HDLC1AMV                (UART1_BASE+0x210)
#define HDLC1AMSK               (UART1_BASE+0x214)
#define HDLC1RIB                (UART1_BASE+0x218)
#define HDLC1SR                 (UART1_BASE+0x21c)

/* Offsets to the various UART registers */
#define UARTDR                  0x0000
#define UARTRSR                 0x0004
#define UARTECR                 0x0004
#define UARTCR_H                0x0008
#define UARTCR_M                0x000C
#define UARTCR_L                0x0010
#define UARTCR                  0x0014
#define UARTFR                  0x0018
#define UARTIIR                 0x001C
#define UARTICR                 0x001C
#define UARTMCR                 0x0100
#define UARTMSR                 0x0104

/* 808d_0000 - 808d_ffff: UART2 */
#define UART2_OFFSET            0x0D0000
#define UART2_BASE              (EP93XX_APB_VIRT_BASE|UART2_OFFSET)
#define UART2_BASE_VIRT         (EP93XX_APB_PHYS_BASE|UART2_OFFSET)
#define UART2DR                 (UART2_BASE+0x00)
#define UART2RSR                (UART2_BASE+0x04) /* Read */
#define UART2ECR                (UART2_BASE+0x04) /* Write */
#define UART2CR_H               (UART2_BASE+0x08)
#define UART2CR_M               (UART2_BASE+0x0C)
#define UART2CR_L               (UART2_BASE+0x10)
#define UART2CR                 (UART2_BASE+0x14)
#define UART2FR                 (UART2_BASE+0x18)
#define UART2IIR                (UART2_BASE+0x1C) /* Read */
#define UART2ICR                (UART2_BASE+0x1C) /* Write */
#define UART2ILPR               (UART2_BASE+0x20)
#define UART2DMACR              (UART2_BASE+0x28)
#define UART2TMR                (UART2_BASE+0x84)

/* 808e_0000 - 808e_ffff: UART3 */
#define UART3_OFFSET            0x0E0000
#define UART3_BASE              (EP93XX_APB_VIRT_BASE|UART3_OFFSET)
#define UART3_BASE_VIRT         (EP93XX_APB_PHYS_BASE|UART3_OFFSET)
#define UART3DR                 (UART3_BASE+0x00)
#define UART3RSR                (UART3_BASE+0x04) /* Read */
#define UART3ECR                (UART3_BASE+0x04) /* Write */
#define UART3CR_H               (UART3_BASE+0x08)
#define UART3CR_M               (UART3_BASE+0x0C)
#define UART3CR_L               (UART3_BASE+0x10)
#define UART3CR                 (UART3_BASE+0x14)
#define UART3FR                 (UART3_BASE+0x18)
#define UART3IIR                (UART3_BASE+0x1C) /* Read */
#define UART3ICR                (UART3_BASE+0x1C) /* Write */
#define UART3ILPR               (UART3_BASE+0x20)
#define UART3DMACR              (UART3_BASE+0x28)
#define UART3TCR                (UART3_BASE+0x80)
#define UART3TISR               (UART3_BASE+0x88)
#define UART3TOCR               (UART3_BASE+0x8C)
#define UART3TMR                (UART3_BASE+0x84)
#define UART3MCR                (UART3_BASE+0x100) /* Modem Control Reg */
#define UART3MSR                (UART3_BASE+0x104) /* Modem Status Reg */

#define UART3HDLCCR             (UART3_BASE+0x20C) /* HDLC Registers */
#define UART3HDLCAMV            (UART3_BASE+0x210) /* HDLC Registers */
#define UART3HDLCAMSK           (UART3_BASE+0x214) /* HDLC Registers */
#define UART3HDLCCRIB           (UART3_BASE+0x218) /* HDLC Registers */
#define UART3HDLCSR             (UART3_BASE+0x21C) /* HDLC Registers */

/* 808f_0000 - 808f_ffff: KEY Matrix */
#define KEY_OFFSET              0x0F0000
#define KEY_BASE                (EP93XX_APB_VIRT_BASE|KEY_OFFSET)
#define SCANINIT                (KEY_BASE+0x00)
#define KEY_DIAG                (KEY_BASE+0x04)
#define KEY_REG                 (KEY_BASE+0x08)
#define KEY_TCR                 (KEY_BASE+0x10)
#define KEY_TISR                (KEY_BASE+0x14)
#define KEY_TOCR                (KEY_BASE+0x18)

#define TOUCH_OFFSET            0x100000
#define TOUCH_BASE              (EP93XX_APB_VIRT_BASE|TOUCH_OFFSET)
#define TSSetup                 (TOUCH_BASE+0x00) /* R/W touchscreen controller setup control register.     */
#define TSXYMaxMin              (TOUCH_BASE+0x04) /* R/W touchscreen controller max/min register.           */
#define TSXYResult              (TOUCH_BASE+0x08) /* R   touchscreen controller result register.            */
#define TSDischarge             (TOUCH_BASE+0x0C) /* LOCKED R/W touchscreen Switch Matrix control register. */
#define TSXSample               (TOUCH_BASE+0x10) /* LOCKED R/W touchscreen Switch Matrix control register. */
#define TSYSample               (TOUCH_BASE+0x14) /* LOCKED R/W touchscreen Switch Matrix control register. */
#define TSDirect                (TOUCH_BASE+0x18) /* LOCKED R/W touchscreen Switch Matrix control register. */
#define TSDetect                (TOUCH_BASE+0x1C) /* LOCKED R/W touchscreen Switch Matrix control register. */
#define TSSWLock                (TOUCH_BASE+0x20) /*  NA    R/W touchscreen software lock register.         */
#define TSSetup2                (TOUCH_BASE+0x24) /* R/W touchscreen setup control register #2.             */

/* 8093_0000 - 8093_ffff: CSC/Syscon  PLL, clock control, & misc. stuff */
#define SYSCON_OFFSET           0x130000
#define SYSCON_BASE             ((EP93XX_APB_VIRT_BASE)|SYSCON_OFFSET)
#define SYSCON_PWRSR            (SYSCON_BASE+0x0000)
#define SYSCON_PWRCNT           (SYSCON_BASE+0x0004)
#define SYSCON_HALT             (SYSCON_BASE+0x0008)
#define SYSCON_STBY             (SYSCON_BASE+0x000c)
#define SYSCON_BLEOI            (SYSCON_BASE+0x0010)
#define SYSCON_MCEOI            (SYSCON_BASE+0x0014)
#define SYSCON_TEOI             (SYSCON_BASE+0x0018)
#define SYSCON_STFCLR           (SYSCON_BASE+0x001c)
#define SYSCON_CLKSET1          (SYSCON_BASE+0x0020)
#define SYSCON_CLKSET2          (SYSCON_BASE+0x0024)
#define SYSCON_RESV00           (SYSCON_BASE+0x0028)
#define SYSCON_RESV01           (SYSCON_BASE+0x002c)
#define SYSCON_RESV02           (SYSCON_BASE+0x0030)
#define SYSCON_RESV03           (SYSCON_BASE+0x0034)
#define SYSCON_RESV04           (SYSCON_BASE+0x0038)
#define SYSCON_RESV05           (SYSCON_BASE+0x003c)
#define SYSCON_SCRREG0          (SYSCON_BASE+0x0040)
#define SYSCON_SCRREG1          (SYSCON_BASE+0x0044)
#define SYSCON_CLKTEST          (SYSCON_BASE+0x0048)
#define SYSCON_USBRESET         (SYSCON_BASE+0x004c)
#define SYSCON_APBWAIT          (SYSCON_BASE+0x0050)
#define SYSCON_BMAR             (SYSCON_BASE+0x0054)
#define SYSCON_BOOTCLR          (SYSCON_BASE+0x0058)
#define SYSCON_DEVCFG           (SYSCON_BASE+0x0080)
#define SYSCON_VIDDIV           (SYSCON_BASE+0x0084)
#define SYSCON_MIRDIV           (SYSCON_BASE+0x0088)
#define SYSCON_I2SDIV           (SYSCON_BASE+0x008C)
#define SYSCON_KTDIV            (SYSCON_BASE+0x0090)
#define SYSCON_CHIPID           (SYSCON_BASE+0x0094)
#define SYSCON_TSTCR            (SYSCON_BASE+0x0098)
#define SYSCON_SYSCFG           (SYSCON_BASE+0x009C)
#define SYSCON_SWLOCK           (SYSCON_BASE+0x00C0)

#define SYSCON_DEVCFG_KEYS      0x00000002
#define SYSCON_DEVCFG_RasOnP3   0x00000010
#define SYSCON_DEVCFG_GONK      0x08000000

#define SYSCON_KTDIV_KEN        0x00008000

#define EP93XX_ETHERNET_BASE			(EP93XX_AHB_VIRT_BASE + 0x00010000)
#define EP93XX_ETHERNET_PHYS_BASE		(EP93XX_AHB_PHYS_BASE + 0x00010000)

#define EP93XX_USB_BASE					(EP93XX_AHB_VIRT_BASE + 0x00020000)
#define EP93XX_USB_PHYS_BASE			(EP93XX_AHB_PHYS_BASE + 0x00020000)

#define EP93XX_RASTER_BASE				(EP93XX_AHB_VIRT_BASE + 0x00030000)
#define EP93XX_RASTER_PHYS_BASE			(EP93XX_AHB_PHYS_BASE + 0x00030000)

#define EP93XX_GRAPHICS_ACCEL_BASE		(EP93XX_AHB_VIRT_BASE + 0x00040000)
#define EP93XX_GRAPHICS_ACCEL_PHYS_BASE (EP93XX_AHB_PHYS_BASE + 0x00040000


#define EP93XX_SDRAM_CONTROLLER_BASE	(EP93XX_AHB_VIRT_BASE + 0x00060000)

#define EP93XX_PCMCIA_CONTROLLER_BASE	(EP93XX_AHB_VIRT_BASE + 0x00080000)

#define EP93XX_BOOT_ROM_BASE			(EP93XX_AHB_VIRT_BASE + 0x00090000)

#define EP93XX_IDE_BASE					(EP93XX_AHB_VIRT_BASE + 0x000a0000)
#define EP93XX_IDE_REG(x)				(EP93XX_IDE_BASE + (x))
#define EP93XX_IDE_CTRL					EP93XX_IDE_REG(0x0000)
#define EP93XX_IDE_CFG					EP93XX_IDE_REG(0x0004)
#define EP93XX_IDE_DATAOUT				EP93XX_IDE_REG(0x0010)
#define EP93XX_IDE_DATAIN				EP93XX_IDE_REG(0x0014)

#define EP93XX_IDE_CTRL_CS0n			(1L << 0)
#define EP93XX_IDE_CTRL_CS1n			(1L << 1)
#define EP93XX_IDE_CTRL_DA_MASK			0x1C
#define EP93XX_IDE_CTRL_DA(x)			((x << 2) & EP93XX_IDE_CTRL_DA_MASK)
#define EP93XX_IDE_CTRL_DA_CS_MASK		(EP93XX_IDE_CTRL_DA_MASK | EP93XX_IDE_CTRL_CS0n | EP93XX_IDE_CTRL_CS1n)
#define EP93XX_IDE_CTRL_DA_CS(x)		(((x)) & EP93XX_IDE_CTRL_DA_CS_MASK)
#define EP93XX_IDE_CTRL_DIORn			(1L << 5)
#define EP93XX_IDE_CTRL_DIOWn			(1L << 6)
#define EP93XX_IDE_CTRL_DASPn			(1L << 7)
#define EP93XX_IDE_CTRL_DMARQ			(1L << 8)
#define EP93XX_IDE_CTRL_INTRQ			(1L << 9)
#define EP93XX_IDE_CTRL_IORDY			(1L << 10)

#define EP93XX_IDE_CFG_IDEEN			(1L << 0)
#define EP93XX_IDE_CFG_PIO				(1L << 1)
#define EP93XX_IDE_CFG_MDMA				(1L << 2)
#define EP93XX_IDE_CFG_UDMA				(1L << 3)
#define EP93XX_IDE_CFG_MODE(x)			((x & 0x0F) << 4)
#define EP93XX_IDE_CFG_WST(x)			((x & 0x03) << 8)

/* Olde IDE DMA defines */
/* 800A_0000 - 800A_ffff: IDE Interface  */
#define IDE_OFFSET              	0x0a0000
#define IDE_BASE                	(EP93XX_AHB_VIRT_BASE|IDE_OFFSET)
#define IDECR                   	(IDE_BASE+0x00)
#define IDECFG                  	(IDE_BASE+0x04)
#define IDEMDMAOP               	(IDE_BASE+0x08)
#define IDEUDMAOP               	(IDE_BASE+0x0C)
#define IDEDATAOUT              	(IDE_BASE+0x10)
#define IDEDATAIN               	(IDE_BASE+0x14)
#define IDEMDMADATAOUT          	(IDE_BASE+0x18)
#define IDEMDMADATAIN           	(IDE_BASE+0x1C)
#define IDEUDMADATAOUT          	(IDE_BASE+0x20)
#define IDEUDMADATAIN           	(IDE_BASE+0x24)
#define IDEUDMASTATUS           	(IDE_BASE+0x28)
#define IDEUDMADEBUG            	(IDE_BASE+0x2C)
#define IDEUDMAWFST             	(IDE_BASE+0x30)
#define IDEUDMARFST             	(IDE_BASE+0x34)

/*****************************************************************************
 *
 *  Bit definitions for use with assembly code for the ide control register.
 *
 ****************************************************************************/
#define IDECtrl_CS0n				0x00000001
#define IDECtrl_CS1n				0x00000002
#define IDECtrl_DA_MASK				0x0000001c
#define IDECtrl_DA_SHIFT			2
#define IDECtrl_DIORn				0x00000020
#define IDECtrl_DIOWn				0x00000040
#define IDECtrl_DASPn				0x00000080
#define IDECtrl_DMARQ				0x00000100
#define IDECtrl_INTRQ				0x00000200
#define IDECtrl_IORDY				0x00000400

#define IDECfg_IDEEN				0x00000001
#define IDECfg_PIO					0x00000002
#define IDECfg_MDMA					0x00000004
#define IDECfg_UDMA					0x00000008
#define IDECfg_MODE_MASK			0x000000f0
#define IDECfg_MODE_SHIFT			4
#define IDECfg_WST_MASK				0x00000300
#define IDECfg_WST_SHIFT			8

#define IDEMDMAOp_MEN				0x00000001
#define IDEMDMAOp_RWOP				0x00000002

#define IDEUDMAOp_UEN				0x00000001
#define IDEUDMAOp_RWOP				0x00000002

#define IDEUDMASts_CS0n				0x00000001
#define IDEUDMASts_CS1n				0x00000002
#define IDEUDMASts_DA_MASK			0x0000001c
#define IDEUDMASts_DA_SHIFT			2
#define IDEUDMASts_HSHD				0x00000020
#define IDEUDMASts_STOP				0x00000040
#define IDEUDMASts_DM				0x00000080
#define IDEUDMASts_DDOE				0x00000100
#define IDEUDMASts_DMARQ			0x00000200
#define IDEUDMASts_DSDD				0x00000400
#define IDEUDMASts_DMAide			0x00010000
#define IDEUDMASts_INTide			0x00020000
#define IDEUDMASts_SBUSY			0x00040000
#define IDEUDMASts_NDO				0x01000000
#define IDEUDMASts_NDI				0x02000000
#define IDEUDMASts_N4X				0x04000000

#define IDEUDMADebug_RWOE			0x00000001
#define IDEUDMADebug_RWPTR			0x00000002
#define IDEUDMADebug_RWDR			0x00000004
#define IDEUDMADebug_RROE			0x00000008
#define IDEUDMADebug_RRPTR			0x00000010
#define IDEUDMADebug_RRDR			0x00000020

#define IDEUDMAWrBufSts_HPTR_MASK	0x0000000f
#define IDEUDMAWrBufSts_HPTR_SHIFT	0
#define IDEUDMAWrBufSts_TPTR_MASK	0x000000f0
#define IDEUDMAWrBufSts_TPTR_SHIFT	4
#define IDEUDMAWrBufSts_EMPTY		0x00000100
#define IDEUDMAWrBufSts_HOM			0x00000200
#define IDEUDMAWrBufSts_NFULL		0x00000400
#define IDEUDMAWrBufSts_FULL		0x00000800
#define IDEUDMAWrBufSts_CRC_MASK	0xffff0000
#define IDEUDMAWrBufSts_CRC_SHIFT	16

#define IDEUDMARdBufSts_HPTR_MASK	0x0000000f
#define IDEUDMARdBufSts_HPTR_SHIFT	0
#define IDEUDMARdBufSts_TPTR_MASK	0x000000f0
#define IDEUDMARdBufSts_TPTR_SHIFT	4
#define IDEUDMARdBufSts_EMPTY		0x00000100
#define IDEUDMARdBufSts_HOM			0x00000200
#define IDEUDMARdBufSts_NFULL		0x00000400
#define IDEUDMARdBufSts_FULL		0x00000800
#define IDEUDMARdBufSts_CRC_MASK	0xffff0000
#define IDEUDMARdBufSts_CRC_SHIFT	16


/*----------------------------------old-------------------------------*/
#define EP93XX_VIC1_BASE			(EP93XX_AHB_VIRT_BASE + 0x000b0000)
#define EP93XX_VIC2_BASE			(EP93XX_AHB_VIRT_BASE + 0x000c0000)

/* APB peripherals */
#define EP93XX_TIMER_BASE			(EP93XX_APB_VIRT_BASE + 0x00010000)
#define EP93XX_TIMER_REG(x)			(EP93XX_TIMER_BASE + (x))
#define EP93XX_TIMER1_LOAD			EP93XX_TIMER_REG(0x00)
#define EP93XX_TIMER1_VALUE			EP93XX_TIMER_REG(0x04)
#define EP93XX_TIMER1_CONTROL		EP93XX_TIMER_REG(0x08)
#define EP93XX_TIMER1_CLEAR			EP93XX_TIMER_REG(0x0c)
#define EP93XX_TIMER2_LOAD			EP93XX_TIMER_REG(0x20)
#define EP93XX_TIMER2_VALUE			EP93XX_TIMER_REG(0x24)
#define EP93XX_TIMER2_CONTROL		EP93XX_TIMER_REG(0x28)
#define EP93XX_TIMER2_CLEAR			EP93XX_TIMER_REG(0x2c)
#define EP93XX_TIMER4_VALUE_LOW		EP93XX_TIMER_REG(0x60)
#define EP93XX_TIMER4_VALUE_HIGH	EP93XX_TIMER_REG(0x64)
#define EP93XX_TIMER3_LOAD			EP93XX_TIMER_REG(0x80)
#define EP93XX_TIMER3_VALUE			EP93XX_TIMER_REG(0x84)
#define EP93XX_TIMER3_CONTROL		EP93XX_TIMER_REG(0x88)
#define EP93XX_TIMER3_CLEAR			EP93XX_TIMER_REG(0x8c)

#define EP93XX_I2S_BASE				(EP93XX_APB_VIRT_BASE + 0x00020000)

#define EP93XX_SECURITY_BASE		(EP93XX_APB_VIRT_BASE + 0x00030000)

#define EP93XX_GPIO_BASE			(EP93XX_APB_VIRT_BASE + 0x00040000)
#define EP93XX_GPIO_REG(x)			(EP93XX_GPIO_BASE + (x))
#define EP93XX_GPIO_A_INT_TYPE1		EP93XX_GPIO_REG(0x90)
#define EP93XX_GPIO_A_INT_TYPE2		EP93XX_GPIO_REG(0x94)
#define EP93XX_GPIO_A_INT_ACK		EP93XX_GPIO_REG(0x98)
#define EP93XX_GPIO_A_INT_ENABLE	EP93XX_GPIO_REG(0x9c)
#define EP93XX_GPIO_A_INT_STATUS	EP93XX_GPIO_REG(0xa0)
#define EP93XX_GPIO_A_INT_DEBOUNCE	EP93XX_GPIO_REG(0xa8)
#define EP93XX_GPIO_B_INT_TYPE1		EP93XX_GPIO_REG(0xac)
#define EP93XX_GPIO_B_INT_TYPE2		EP93XX_GPIO_REG(0xb0)
#define EP93XX_GPIO_B_INT_ACK		EP93XX_GPIO_REG(0xb4)
#define EP93XX_GPIO_B_INT_ENABLE	EP93XX_GPIO_REG(0xb8)
#define EP93XX_GPIO_B_INT_STATUS	EP93XX_GPIO_REG(0xbc)
#define EP93XX_GPIO_B_INT_DEBOUNCE	EP93XX_GPIO_REG(0xc4)

#define EP93XX_GPIO_F_INT_TYPE1		EP93XX_GPIO_REG(0x4c)
#define EP93XX_GPIO_F_INT_TYPE2		EP93XX_GPIO_REG(0x50)
#define EP93XX_GPIO_F_INT_ACK		EP93XX_GPIO_REG(0x54)
#define EP93XX_GPIO_F_INT_ENABLE	EP93XX_GPIO_REG(0x58)
#define EP93XX_GPIO_F_INT_STATUS	EP93XX_GPIO_REG(0x5c)
/*-------------------------------------------------------------------------------*/

#define EP93XX_AAC_BASE				(EP93XX_APB_VIRT_BASE + 0x00080000)
#define EP93XX_SPI_BASE				(EP93XX_APB_VIRT_BASE + 0x000a0000)

#define EP93XX_IRDA_BASE			(EP93XX_APB_VIRT_BASE + 0x000b0000)

#define EP93XX_UART1_BASE			(EP93XX_APB_VIRT_BASE + 0x000c0000)
#define EP93XX_UART1_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000c0000)

#define EP93XX_UART2_BASE			(EP93XX_APB_VIRT_BASE + 0x000d0000)
#define EP93XX_UART2_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000d0000)

#define EP93XX_UART3_BASE			(EP93XX_APB_VIRT_BASE + 0x000e0000)
#define EP93XX_UART3_PHYS_BASE		(EP93XX_APB_PHYS_BASE + 0x000e0000)

#define EP93XX_KEY_MATRIX_BASE		(EP93XX_APB_VIRT_BASE + 0x000f0000)

#define EP93XX_ADC_BASE				(EP93XX_APB_VIRT_BASE + 0x00100000)
#define EP93XX_TOUCHSCREEN_BASE		(EP93XX_APB_VIRT_BASE + 0x00100000)

#define EP93XX_PWM_BASE				(EP93XX_APB_VIRT_BASE + 0x00110000)

#define EP93XX_RTC_BASE				(EP93XX_APB_VIRT_BASE + 0x00120000)

#define EP93XX_SYSCON_BASE				(EP93XX_APB_VIRT_BASE + 0x00130000)
#define EP93XX_SYSCON_REG(x)			(EP93XX_SYSCON_BASE + (x))
#define EP93XX_SYSCON_POWER_STATE		EP93XX_SYSCON_REG(0x00)
#define EP93XX_SYSCON_CLOCK_CONTROL		EP93XX_SYSCON_REG(0x04)
#define EP93XX_SYSCON_CLOCK_UARTBAUD	0x20000000
#define EP93XX_SYSCON_CLOCK_USH_EN		0x10000000
#define EP93XX_SYSCON_HALT				EP93XX_SYSCON_REG(0x08)
#define EP93XX_SYSCON_STANDBY			EP93XX_SYSCON_REG(0x0c)
#define EP93XX_SYSCON_CLKSET1           EP93XX_SYSCON_REG(0x20)
#define EP93XX_SYSCON_CLOCK_SET1		EP93XX_SYSCON_REG(0x20)
#define EP93XX_SYSCON_CLKSET2           EP93XX_SYSCON_REG(0x24)
#define EP93XX_SYSCON_CLOCK_SET2		EP93XX_SYSCON_REG(0x24)
#define EP93XX_SYSCON_DEVICE_CONFIG		EP93XX_SYSCON_REG(0x80)
#define EP93XX_SYSCON_DEVICE_CONFIG_CRUNCH_ENABLE	0x00800000

#define EP93XX_SYSCON_BMAR              EP93XX_SYSCON_REG(0x54)
#define EP93XX_SYSCON_CHIPID           	EP93XX_SYSCON_REG(0x94)
#define EP93XX_SYSCON_SWLOCK			EP93XX_SYSCON_REG(0xc0)

#define EP93XX_WATCHDOG_BASE			(EP93XX_APB_VIRT_BASE + 0x00140000)


#endif
