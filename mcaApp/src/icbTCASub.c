/*****************************************************************************
* This routine provides an interface between the EPICS subroutine record and
* the icb routines for the Canberra 2016 TCA.
* Author: Tim Mooney
* Modifications:
* .00  09/07/99    tmm    created.  Based partly on icbAdcSub by Ned Arnold
* .01  05/06/00    mlr    Changes to make routines endian-independent
* .02  07/23/00    mlr    Replaced calls to my_icb_output etc. which were
*		          internal to this code with icb_user_subs.c (new).
* .03  08/10/00    tmm    Removed call to nmc_initialize, added calls to
*                         icb_initialize.
*****************************************************************************/

/* 2016 Amp/Triple Channel Analyzer  */
/*
 *
 * Subroutine fields ...
 *
 * Control record
 * DESC - contains the address string of the ICB module, e.g. NI674:1
 * A - Output Polarity        (0=Normal, 1=Inverted)
 * B - PUR Enable             (0=No, 1=Yes)
 * C - ICR Manual Threshold   (0=No, 1=Yes)
 * D - Enable SCA's           (0=No, 1=Yes)
 * E - SCA 1 gate enable      (0=No, 1=Yes)
 * F - SCA 2 gate enable      (0=No, 1=Yes)
 * G - SCA 3 gate enable      (0=No, 1=Yes)
 * H - Select TCA             (0=No, 1=Yes)
 * I - Pileup rejection SCA 1 (0=?, 1=?)
 * J - Pileup rejection SCA 2 (0=?, 1=?)
 * K - Pileup rejection SCA 3 (0=?, 1=?)
 * L - Pileup rejection Amp   (0=?, 1=?)
 *
 * Discriminator record
 * DESC - contains the address string of the ICB module, e.g. NI674:1
 * A - LLD SCA 1
 * B - ULD SCA 1
 * C - LLD SCA 2
 * D - ULD SCA 2
 * E - LLD SCA 2
 * F - ULD SCA 3
 * L - Status (0=Off-line, 1=On-line, 2=On-line and requires attention)
 *
 *
 *
 */

#include        <vxWorks.h>
#include        <vme.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <stdlib.h>
#include        <string.h>

#include        <dbDefs.h>
#include        <subRecord.h>
#include        <taskwd.h>

#include "ndtypes.h"
#include "nmc_sys_defs.h"
#include "mcamsgdef.h"
#include "icb_user_subs.h"

#define ARG_MAX 12

#define LLD		0
#define ULD		0x20
#define SCA_1	0
#define SCA_2	0x40
#define SCA_3	0x80 

/* pointer to networked module info */
extern struct nmc_module_info_struct *nmc_module_info;

/* debug flag */
int icbTCADebug;

static void write_discrim(int ni_module, int icb_addr, double *percent,
		int discrim_spec)
{
	ULONG dac;
	unsigned char registers[2];
	int s;

	if (*percent < 0.0) *percent = 0.0;
	if (*percent > 100) *percent = 100;

	/* Convert to 12 bit DAC value & load into regs for write */
	dac = *percent * 40.95;
	registers[0] = dac & 0x000000ff;
	registers[1] = ((dac&0x0000ff00) >> 8) | discrim_spec;

	/* Send new LLD value to the ICB module */
	s = write_icb(ni_module, icb_addr, 3, 2, &registers[0]);
}


long icbTCASubI(psub)
struct subRecord    *psub;
{
	return(OK);
}
		

long icbTCACtrlSub(psub)
struct subRecord    *psub;
{
	int s;
	int ni_module, icb_addr;
	int             i;
	unsigned char reg, registers[14];

	static long	output_polarity;
	static long	PUR_enable;
	static long	ICR_manual_threshold;
	static long	enable_SCAs;
	static long	SCA1_gate_enable;
	static long	SCA2_gate_enable;
	static long	SCA3_gate_enable;
	static long	select_TCA;
	static long	PUR_SCA1;
	static long	PUR_SCA2;
	static long	PUR_SCA3;
	static long	PUR_Amp;


	if (psub->udf) {
		/* Initialize ICB data structures */
		if (icbTCADebug) printf("icb_initialize in icbTCASub\n");
		icb_initialize();
		psub->udf = 0;
	}

	s = parse_ICB_address(psub->desc, &ni_module, &icb_addr);
	if (s != OK) {
		if (icbTCADebug) printf("Error looking up ICB_TCA %s\n", psub->desc);
		psub->udf = 1;
		return(ERROR);
	}

	if (icbTCADebug > 10) {
		read_icb(ni_module, icb_addr, 0, 7, registers);
		for (i=0; i<7; i++) {
			printf("ICB %s (icb base %d)-- reg %d:%x\n",
				psub->desc, icb_addr, i, registers[i]);
		}
	}

	output_polarity			= psub->a;	/* register 2, bit 0 */
	PUR_enable				= psub->b;	/* register 2, bit 1 */
	ICR_manual_threshold	= psub->c;	/* register 2, bit 2 */
	enable_SCAs				= psub->d;	/* register 2, bit 3 */
	SCA1_gate_enable		= psub->e;	/* register 2, bit 4 */
	SCA2_gate_enable		= psub->f;	/* register 2, bit 5 */
	SCA3_gate_enable		= psub->g;	/* register 2, bit 6 */
	select_TCA				= psub->h;	/* register 2, bit 7 */
	PUR_SCA1				= psub->i;	/* register 6, bit 0 */
	PUR_SCA2				= psub->j;	/* register 6, bit 1 */
	PUR_SCA3				= psub->k;	/* register 6, bit 2 */
	PUR_Amp					= psub->l;	/* register 6, bit 3 */

	if ((psub->a != psub->la) || (psub->b != psub->lb) ||
		(psub->c != psub->lc) || (psub->d != psub->ld) ||
		(psub->e != psub->le) || (psub->f != psub->lf) ||
		(psub->g != psub->lg) || (psub->h != psub->lh)) {

		/* build up register 2 */
		reg = 0;
		if (select_TCA)				reg |= 0x80;
		if (SCA3_gate_enable)		reg |= 0x40;
		if (SCA2_gate_enable)		reg |= 0x20;
		if (SCA1_gate_enable)		reg |= 0x10;
		if (enable_SCAs)			reg |= 0x08;
		if (ICR_manual_threshold)	reg |= 0x04;
		if (PUR_enable)				reg |= 0x02;
		if (output_polarity)		reg |= 0x01;
		write_icb(ni_module, icb_addr, 2, 1, &reg);
	}

	if ((psub->i != psub->li) || (psub->j != psub->lj) ||
		(psub->k != psub->lk) || (psub->l != psub->ll)) {
		/* build up register 6 */
		reg = 0;
		if (PUR_Amp)	reg |= 0x08;
		if (PUR_SCA3)	reg |= 0x04;
		if (PUR_SCA2)	reg |= 0x02;
		if (PUR_SCA1)	reg |= 0x01;
		write_icb(ni_module, icb_addr, 6, 1, &reg);
	}

	/* Read parameters */
	read_icb(ni_module, icb_addr, 2, 1, &reg);
	select_TCA				= (reg & 0x80) ? 1 : 0;
	SCA3_gate_enable		= (reg & 0x40) ? 1 : 0;
	SCA2_gate_enable		= (reg & 0x20) ? 1 : 0;
	SCA1_gate_enable		= (reg & 0x10) ? 1 : 0;
	enable_SCAs				= (reg & 0x08) ? 1 : 0;
	ICR_manual_threshold	= (reg & 0x04) ? 1 : 0;
	PUR_enable				= (reg & 0x02) ? 1 : 0;
	output_polarity			= (reg & 0x01) ? 1 : 0;

	read_icb(ni_module, icb_addr, 6, 1, &reg);
	   
	PUR_Amp		= (reg & 0x08) ? 1 : 0;
	PUR_SCA3	= (reg & 0x04) ? 1 : 0;
	PUR_SCA2	= (reg & 0x02) ? 1 : 0;
	PUR_SCA1	= (reg & 0x01) ? 1 : 0;
	psub->a = output_polarity;
	psub->b = PUR_enable;
	psub->c = ICR_manual_threshold;
	psub->d = enable_SCAs;
	psub->e = SCA1_gate_enable;
	psub->f = SCA2_gate_enable;
	psub->g = SCA3_gate_enable;
	psub->h = select_TCA;
	psub->i = PUR_SCA1;
	psub->j = PUR_SCA2;
	psub->k = PUR_SCA3;
	psub->l	= PUR_Amp;
	if (icbTCADebug > 1) {
	   printf("icbTCA.c: register 6 = %x\n", reg);
	   printf("   PUR_SCA1=%ld, psub->i=%f\n", PUR_SCA1, psub->i);
	}

	return(OK);
}


long icbTCADiscrimSub(psub)
struct subRecord    *psub;
{
	int s;
	int ni_module, icb_addr;
	int             i;
	unsigned char reg, registers[14];


	if (psub->udf) {
		/* Initialize ICB data structures */
		if (icbTCADebug) printf("icb_initialize in icbTCASub\n");
		icb_initialize();
		psub->udf = 0;
	}

	s = parse_ICB_address(psub->desc, &ni_module, &icb_addr);
	if (s != OK) {
		if (icbTCADebug) printf("Error looking up ICB_TCA %s\n", psub->desc);
		psub->udf = 1;
		return(ERROR);
	}

	if (icbTCADebug > 10) {
		read_icb(ni_module, icb_addr, 0, 7, registers);
		for (i=0; i<7; i++) {
			printf("ICB %s (icb base %d)-- reg %d:%x\n",
				psub->desc, icb_addr, i, registers[i]);
		}
	}

	if (psub->a != psub->la) {
		write_discrim(ni_module, icb_addr, &psub->a, SCA_1 | LLD);
	}
	if (psub->b != psub->lb) {
		write_discrim(ni_module, icb_addr, &psub->b, SCA_1 | ULD);
	}
	if (psub->c != psub->lc) {
		write_discrim(ni_module, icb_addr, &psub->c, SCA_2 | LLD);
	}
	if (psub->d != psub->ld) {
		write_discrim(ni_module, icb_addr, &psub->d, SCA_2 | ULD);
	}
	if (psub->e != psub->le) {
		write_discrim(ni_module, icb_addr, &psub->e, SCA_3 | LLD);
	}
	if (psub->f != psub->lf) {
		write_discrim(ni_module, icb_addr, &psub->f, SCA_3 | ULD);
	}

	/* Read status of module */
	read_icb(ni_module, icb_addr, 0, 1, &reg);

	if (reg & 0x4)		/* module wants attention */ 
		psub->l = 2;
	else if (reg & 0x8)	/* module failed self test */
		psub->l = 1;
	else				/* module must be ok */
		psub->l = 0;

	return(OK);
}
