/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include "bitops.hh"
#include "component_info.hh"
#include "uncompressed_components.hh"
#include "jpgcoder.hh"
#include "../io/SwitchableCompression.hh"
#include "simple_decoder.hh"

#include <algorithm>
SimpleComponentDecoder::SimpleComponentDecoder() {
    str_in = NULL;
    batch_size = 0;
    for (unsigned int i = 0; i < sizeof(cur_read_batch) / sizeof(cur_read_batch[0]); ++i) {
        cur_read_batch[i] = 0;
        target[i] = 0;
        started_scan[i] = false;
    }
}
void SimpleComponentDecoder::initialize(Sirikata::
                                        SwitchableDecompressionReader<Sirikata::SwitchableXZBase> *i) {
    str_in = i;
}

void SimpleComponentDecoder::simple_continuous_decoder(UncompressedComponents* colldata,
                                                       Sirikata::
                                                       SwitchableDecompressionReader<Sirikata::SwitchableXZBase> *i) {
    colldata->worker_wait_for_begin_signal();

    SimpleComponentDecoder scd;
    scd.initialize(i);
    while(scd.decode_chunk(colldata) == CODING_PARTIAL) {
    }
}

unsigned int get_cmp(int cur_read_batch[4], int target[4]) {
    unsigned int cmp = 0;
    double cmp_progress = cur_read_batch[cmp]/(double)target[cmp];
    for (unsigned int icmp = 1; icmp < 4; ++icmp) {
        if (target[cmp] && cur_read_batch[icmp] != target[icmp]) {
            double cprogress = cur_read_batch[icmp]/(double)target[icmp];
            if (cprogress < cmp_progress) {
                cmp = icmp;
                cmp_progress = cprogress;
            }
        }
    }
    return cmp;
}

CodingReturnValue SimpleComponentDecoder::decode_chunk(UncompressedComponents* colldata) {
    colldata->worker_update_coefficient_position_progress(64); // we are optimizing for baseline only atm
    colldata->worker_update_bit_progress(16); // we are optimizing for baseline only atm
	// read actual decompressed coefficient data from file
    char zero[sizeof(target)] = {0};
    if (memcmp(target, zero, sizeof(target)) == 0) {
        unsigned char bs[4] = {0};
        IOUtil::ReadFull(str_in, bs, sizeof(bs));
        batch_size = bs[3];
        batch_size <<= 8;
        batch_size |= bs[2];
        batch_size <<= 8;
        batch_size |= bs[1];
        batch_size <<= 8;
        batch_size |= bs[0];
        for (unsigned int cmp = 0; cmp < 4; ++cmp) {
            target[cmp] = colldata->component_size_in_blocks(cmp);
        }
    }
    unsigned int cmp = get_cmp(cur_read_batch, target);
    if (cmp == sizeof(cur_read_batch)/sizeof(cur_read_batch[0]) || cur_read_batch[cmp] == target[cmp]) {
        return CODING_DONE;
    }
    // read coefficient data from file
    signed short * start = colldata->full_component_write( cmp );
    while (cur_read_batch[cmp] < target[cmp]) {
        int cur_read_size = std::min((int)batch_size, target[cmp] - cur_read_batch[cmp]);
        size_t retval = IOUtil::ReadFull(str_in, start + cur_read_batch[cmp] * 64 , sizeof( short ) * 64 * cur_read_size);
        if (retval != sizeof( short) * 64 * cur_read_size) {
            sprintf( errormessage, "Wnexpected end of file blocks %ld !=  %d", retval, cur_read_size);
            errorlevel = 2;
            return CODING_ERROR;
        }
        cur_read_batch[cmp] += cur_read_size;
        colldata->worker_update_cmp_progress(cmp, cur_read_size);
        
        return CODING_PARTIAL;
    }
    assert(false && "UNREACHABLE");
    return CODING_PARTIAL;
}
SimpleComponentDecoder::~SimpleComponentDecoder() {

}