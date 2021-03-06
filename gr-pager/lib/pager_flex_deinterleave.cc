/*
 * Copyright 2004,2006,2010 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <pager_flex_deinterleave.h>
#include <pageri_bch3221.h>
#include <pageri_util.h>
#include <gr_io_signature.h>

pager_flex_deinterleave_sptr pager_make_flex_deinterleave()
{
    return gnuradio::get_initial_sptr(new pager_flex_deinterleave());
}

pager_flex_deinterleave::pager_flex_deinterleave() :
    gr_sync_decimator("flex_deinterleave",
    gr_make_io_signature(1, 1, sizeof(unsigned char)),
    gr_make_io_signature(1, 1, sizeof(gr_int32)), 32)
{
    set_output_multiple(8); // One FLEX block at a time
}

int pager_flex_deinterleave::work(int noutput_items,
    gr_vector_const_void_star &input_items,
    gr_vector_void_star &output_items)
{
    const unsigned char *in = (const unsigned char *)input_items[0];
    gr_int32 *out = (gr_int32 *)output_items[0];    

    // FLEX codewords are interleaved in blocks of 256 bits or 8, 32 bit
    // codes.  To deinterleave we parcel each incoming bit into the MSB
    // of each codeword, then switch to MSB-1, etc.  This is done by shifting
    // in the bits from the right on each codeword as the bits come in.
    // When we are done we have a FLEX block of eight codewords, ready for
    // conversion to data words.
    //
    // FLEX data words are recovered by reversing the bit order of the code
    // word, masking off the (reversed) ECC, and inverting the remainder of 
    // the bits (!).
    //
    // The data portion of a FLEX frame consists of 11 of these deinterleaved
    // and converted blocks.
    //
    // set_output_multiple garauntees we have output space for at least
    // eight data words, and 256 bits are supplied on input

    int i, j;
    for (i = 0; i < 32; i++) {
	for (j = 0; j < 8; j++) {
	    d_codewords[j] <<= 1;
	    d_codewords[j]  |= *in++;
	}
    }

    // Now convert code words into data words  
    for (j = 0; j < 8; j++) {
	gr_int32 codeword = d_codewords[j];
	
	// Apply BCH 32,21 error correction
	// TODO: mark dataword when codeword fails ECC
	pageri_bch3221(codeword);
	
	// Reverse bit order
	codeword = pageri_reverse_bits32(codeword);

	// Mask off ECC then invert lower 21 bits
	codeword = (codeword & 0x001FFFFF)^0x001FFFFF;

	*out++ = codeword;
    }
    
    return j;
}
