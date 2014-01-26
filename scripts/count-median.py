#! /usr/bin/env python
#
# This file is part of khmer, http://github.com/ged-lab/khmer/, and is
# Copyright (C) Michigan State University, 2009-2013. It is licensed under
# the three-clause BSD license; see doc/LICENSE.txt. Contact: ctb@msu.edu
#
"""
Count the median/avg k-mer abundance for each sequence in the input file,
based on the k-mer counts in the given counting hash.  Can be used to
estimate expression levels (mRNAseq) or coverage (genomic/metagenomic).

% scripts/count-median.py <htname> <input seqs> <output counts>

Use '-h' for parameter help.

The output file contains sequence id, median, average, stddev, and seq length.

NOTE: All 'N's in the input sequences are converted to 'G's.
"""
import sys
import screed
import os
import khmer
import argparse

#  Import fileapi from sandbox - temporary arrangement
current_file_path = os.path.realpath(__file__)
current_folder = os.path.dirname(current_file_path)
parent_folder = os.path.dirname(current_folder)
sandbox_folder = os.path.join(parent_folder, 'sandbox')
sys.path.append(sandbox_folder)

import fileApi
#


def main():
    parser = argparse.ArgumentParser(
        description='Count k-mers summary stats for sequences')

    parser.add_argument('htfile')
    parser.add_argument('input')
    parser.add_argument('output')

    args = parser.parse_args()

    htfile = args.htfile
    input_filename = args.input
    output_filename = args.output

    # Check if input files exist
    infiles = [htfile, input_filename]
    for infile in infiles:
        fileApi.check_file_status(infile)
    
    # Check free space
    fileApi.check_space(infiles)

    print 'loading counting hash from', htfile
    ht = khmer.load_counting_hash(htfile)
    K = ht.ksize()

    print 'writing to', output_filename
    output = open(output_filename, 'w')

    for record in screed.open(input_filename):
        seq = record.sequence.upper()
        if 'N' in seq:
            seq = seq.replace('N', 'G')

        if K <= len(seq):
            a, b, c = ht.get_median_count(seq)
            print >>output, record.name, a, b, c, len(seq)

if __name__ == '__main__':
    main()
