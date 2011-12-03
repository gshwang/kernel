/*=======================================================================
 *
 *  FILE:       regs_smc.h
 *
 *  DESCRIPTION:    Static Memory Controller/PCMCIA Register Definition
 *
 *  Copyright Cirrus Logic, 2001-2004
 *
 *=======================================================================
 */
#ifndef _REGS_SMC_H_
#define _REGS_SMC_H_

/* Bit definitions */

//
// Bit fields for SMCBCR?
//
#define SMCBCR_IDCY_MASK		0x0000000f
#define SMCBCR_WST1_MASK		0x000003e0
#define SMCBCR_RBLE			0x00000400
#define SMCBCR_WST2_MASK		0x0000f800
#define SMCBCR_WPERR			0x02000000
#define SMCBCR_WP			0x04000000
#define SMCBCR_PME			0x08000000
#define SMCBCR_MW_MASK			0x30000000
#define SMCBCR_MW_8			0x00000000
#define SMCBCR_MW_16			0x10000000
#define SMCBCR_MW_32			0x20000000
#define SMCBCR_EBIBRKDIS		0x40000000
#define SMCBCR_IDCY_SHIFT		0
#define SMCBCR_WST1_SHIFT		5
#define SMCBCR_WST2_SHIFT		11
#define SMCBCR_MW_SHIFT			28

//
// Bit field for SMC_PCAttribute, SMC_PCCommon, and SMC_PCIO
//      
#define PCCONFIG_ADDRESSTIME_MASK   0x000000FF
#define PCCONFIG_HOLDTIME_MASK      0x00000F00
#define PCCONFIG_ACCESSTIME_MASK    0x00FF0000
#define PCCONFIG_MW_8BIT            0x00000000
#define PCCONFIG_MW_16BIT           0x80000000
#define PCCONFIG_ADDRESSTIME_SHIFT  0
#define PCCONFIG_HOLDTIME_SHIFT     8
#define PCCONFIG_ACCESSTIME_SHIFT   16

//
// Bit field for SMC_PCMCIACtrl
//
#define PCCONT_PC1EN                0x00000001
#define PCCONT_PC2EN                0x00000002
#define PCCONT_PC1RST               0x00000004
#define PCCONT_WEN                  0x00000010


#endif /* _REGS_SMC_H_ */
