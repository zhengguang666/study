/*
 * Copyright 2011-2012 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or later as published by the Free Software Foundation.
 */

#include <common.h>
#include <i2c.h>
#include <hwconfig.h>
#include <asm/mmu.h>
#include <asm/fsl_ddr_sdram.h>
#include <asm/fsl_ddr_dimm_params.h>
#include <asm/fsl_law.h>

DECLARE_GLOBAL_DATA_PTR;

dimm_params_t ddr_raw_timing = {
	.n_ranks = 2,
	.rank_density = 2147483648u,
	.capacity = 4294967296u,
	.primary_sdram_width = 64,
	.ec_sdram_width = 8,
	.registered_dimm = 0,
	.mirrored_dimm = 1,
	.n_row_addr = 15,
	.n_col_addr = 10,
	.n_banks_per_sdram_device = 8,
	.edc_config = 2,	/* ECC */
	.burst_lengths_bitmask = 0x0c,

	.tCKmin_X_ps = 1071,
	.caslat_X = 0x2fe << 4,	/* 5,6,7,8,9,10,11,13 */
	.tAA_ps = 13910,
	.tWR_ps = 15000,
	.tRCD_ps = 13910,
	.tRRD_ps = 6000,
	.tRP_ps = 13910,
	.tRAS_ps = 34000,
	.tRC_ps = 48910,
	.tRFC_ps = 260000,
	.tWTR_ps = 7500,
	.tRTP_ps = 7500,
	.refresh_rate_ps = 7800000,
	.tFAW_ps = 35000,
};

int fsl_ddr_get_dimm_params(dimm_params_t *pdimm,
		unsigned int controller_number,
		unsigned int dimm_number)
{
	const char dimm_model[] = "RAW timing DDR";

	if ((controller_number == 0) && (dimm_number == 0)) {
		memcpy(pdimm, &ddr_raw_timing, sizeof(dimm_params_t));
		memset(pdimm->mpart, 0, sizeof(pdimm->mpart));
		memcpy(pdimm->mpart, dimm_model, sizeof(dimm_model) - 1);
	}

	return 0;
}

struct board_specific_parameters {
	u32 n_ranks;
	u32 datarate_mhz_high;
	u32 clk_adjust;
	u32 wrlvl_start;
	u32 wrlvl_ctl_2;
	u32 wrlvl_ctl_3;
	u32 cpo;
	u32 write_data_delay;
	u32 force_2T;
};

/*
 * This table contains all valid speeds we want to override with board
 * specific parameters. datarate_mhz_high values need to be in ascending order
 * for each n_ranks group.
 */
static const struct board_specific_parameters udimm0[] = {
	/*
	 * memory controller 0
	 *   num|  hi|  clk| wrlvl |   wrlvl   |  wrlvl | cpo  |wrdata|2T
	 * ranks| mhz|adjst| start |   ctl2    |  ctl3  |      |delay |
	 */
	{2,  1350,    4,     7, 0x09080807, 0x07060607,   0xff,    2,  0},
	{2,  1666,    4,     7, 0x09080806, 0x06050607,   0xff,    2,  0},
	{2,  1900,    3,     7, 0x08070706, 0x06040507,   0xff,    2,  0},
	{1,  1350,    4,     7, 0x09080807, 0x07060607,   0xff,    2,  0},
	{1,  1700,    4,     7, 0x09080806, 0x06050607,   0xff,    2,  0},
	{1,  1900,    3,     7, 0x08070706, 0x06040507,   0xff,    2,  0},
	{}
};

static const struct board_specific_parameters *udimms[] = {
	udimm0,
};

void fsl_ddr_board_options(memctl_options_t *popts,
				dimm_params_t *pdimm,
				unsigned int ctrl_num)
{
	const struct board_specific_parameters *pbsp, *pbsp_highest = NULL;
	ulong ddr_freq;

	if (ctrl_num > 2) {
		printf("Not supported controller number %d\n", ctrl_num);
		return;
	}
	if (!pdimm->n_ranks)
		return;

	pbsp = udimms[0];


	/* Get clk_adjust, cpo, write_data_delay,2T, according to the board ddr
	 * freqency and n_banks specified in board_specific_parameters table.
	 */
	ddr_freq = get_ddr_freq(0) / 1000000;
	while (pbsp->datarate_mhz_high) {
		if (pbsp->n_ranks == pdimm->n_ranks) {
			if (ddr_freq <= pbsp->datarate_mhz_high) {
				popts->cpo_override = pbsp->cpo;
				popts->write_data_delay =
					pbsp->write_data_delay;
				popts->clk_adjust = pbsp->clk_adjust;
				popts->wrlvl_start = pbsp->wrlvl_start;
				popts->wrlvl_ctl_2 = pbsp->wrlvl_ctl_2;
				popts->wrlvl_ctl_3 = pbsp->wrlvl_ctl_3;
				popts->twoT_en = pbsp->force_2T;
				goto found;
			}
			pbsp_highest = pbsp;
		}
		pbsp++;
	}

	if (pbsp_highest) {
		printf("Error: board specific timing not found "
			"for data rate %lu MT/s\n"
			"Trying to use the highest speed (%u) parameters\n",
			ddr_freq, pbsp_highest->datarate_mhz_high);
		popts->cpo_override = pbsp_highest->cpo;
		popts->write_data_delay = pbsp_highest->write_data_delay;
		popts->clk_adjust = pbsp_highest->clk_adjust;
		popts->wrlvl_start = pbsp_highest->wrlvl_start;
		popts->twoT_en = pbsp_highest->force_2T;
	} else {
		panic("DIMM is not supported by this board");
	}
found:
	/*
	 * Factors to consider for half-strength driver enable:
	 *	- number of DIMMs installed
	 */
	popts->half_strength_driver_enable = 0;
	/*
	 * Write leveling override
	 */
	popts->wrlvl_override = 1;
	popts->wrlvl_sample = 0xf;

	/*
	 * Rtt and Rtt_WR override
	 */
	popts->rtt_override = 0;

	/* Enable ZQ calibration */
	popts->zq_en = 1;

	/* DHC_EN =1, ODT = 75 Ohm */
	popts->ddr_cdr1 = DDR_CDR1_DHC_EN | DDR_CDR1_ODT(DDR_CDR_ODT_75ohm);
	popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_75ohm);
}

phys_size_t initdram(int board_type)
{
	phys_size_t dram_size;

	puts("Initializing....using SPD\n");

	dram_size = fsl_ddr_sdram();

	dram_size = setup_ddr_tlbs(dram_size / 0x100000);
	dram_size *= 0x100000;

	puts("    DDR: ");
	return dram_size;
}
