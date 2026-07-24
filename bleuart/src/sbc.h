/*
 *
 *  Bluetooth low-complexity, subband codec (SBC) library
 *
 *  Copyright (C) 2008-2010  Nokia Corporation
 *  Copyright (C) 2012-2014  Intel Corporation
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2004-2005  Henryk Ploetz <henryk@ploetzli.ch>
 *  Copyright (C) 2005-2006  Brad Midgley <bmidgley@xmission.com>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __SBC_H
#define __SBC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "sys/types.h"

/* If the toolchain already provides these, keep its values. */
#ifndef EIO
#define EIO 5
#endif

#ifndef ENOMEM
#define ENOMEM 12
#endif

#ifndef EINVAL
#define EINVAL 22
#endif

#ifndef ENOSPC
#define ENOSPC 28
#endif


/* sampling frequency */
#define SBC_FREQ_16000			0x00	//0000 0000
#define SBC_FREQ_32000			0x01	//0000 0001
#define SBC_FREQ_44100			0x02	//0000 0010
#define SBC_FREQ_48000			0x03	//0000 0011

/* blocks */
#define SBC_BLK_4				0x00	//0000 0000
#define SBC_BLK_8				0x01	//0000 0001
#define SBC_BLK_12				0x02	//0000 0010
#define SBC_BLK_16				0x03	//0000 0011

/* channel mode */
#define SBC_MODE_MONO			0x00	//0000 0000
#define SBC_MODE_DUAL_CHANNEL	0x01	//0000 0001
#define SBC_MODE_STEREO			0x02	//0000 0010
#define SBC_MODE_JOINT_STEREO	0x03	//0000 0011

/* allocation method */
#define SBC_AM_LOUDNESS			0x00	//0000 0000
#define SBC_AM_SNR				0x01	//0000 0001

/* subbands */
#define SBC_SB_4				0x00	//0000 0000
#define SBC_SB_8				0x01	//0000 0001

/* data endianess */
#define SBC_LE					0x00	//0000 0000
#define SBC_BE					0x01	//0000 0001
struct sbc_struct {
	unsigned long flags;

	uint8_t frequency;
	uint8_t blocks;
	uint8_t subbands;
	uint8_t mode;
	uint8_t allocation;
	uint8_t bitpool;
	uint8_t endian;

	void *priv;
	void *priv_alloc_base;
};

typedef struct sbc_struct sbc_t;

int sbc_init(sbc_t *sbc, unsigned long flags);
int sbc_reinit(sbc_t *sbc, unsigned long flags);
int sbc_init_msbc(sbc_t *sbc, unsigned long flags);

/* Encodes ONE input block into ONE output block */
ssize_t sbc_encode(sbc_t *sbc, const void *input, size_t input_len,
			void *output, size_t output_len, ssize_t *written);

/* Returns the output block size in bytes */
size_t sbc_get_frame_length(sbc_t *sbc);

/* Returns the time one input/output block takes to play in msec*/
unsigned sbc_get_frame_duration(sbc_t *sbc);

/* Returns the input block size in bytes */
size_t sbc_get_codesize(sbc_t *sbc);

const char *sbc_get_implementation_info(sbc_t *sbc);
void sbc_finish(sbc_t *sbc);

#ifdef __cplusplus
}
#endif

#endif /* __SBC_H */
