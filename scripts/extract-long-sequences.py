#! /usr/bin/env python2
#
# This file is part of khmer, http://github.com/ged-lab/khmer/, and is
# Copyright (C) Michigan State University, 2009-2014. It is licensed under
# the three-clause BSD license; see doc/LICENSE.txt.
# Contact: khmer-project@idyll.org
#
# pylint: disable=invalid-name,missing-docstring

"""
Write out lines of FASTQ and FASTA files that exceed an argument-specified
length.

% python scripts/extract-long-sequences.py [ -l ] [ -o ] <input_file_name(s)>

Use '-h' for parameter help.
"""
import argparse
import screed
import sys


def get_parser():
    parser = argparse.ArgumentParser(
        description='Extracts FASTQ or FASTA sequences longer than argument' 
        ' specified length.',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    parser.add_argument('input_filenames', help='Input FAST[A/Q]'
                        ' sequence filename.', nargs='+')
    parser.add_argument('-o', '--output', help='The name of the output'
                        ' sequence file.', default="/dev/stdout")
    parser.add_argument('-l', '--length', help='The minimum length of'
                        ' the sequence file. Required argument.', 
                        type=int, required=True)
    return parser


def main():
    args = get_parser().parse_args()
    outfp = open(args.output, 'w')
    for file in args.input_filenames:
        for record in screed.open(file):
            if len(record['sequence']) >= args.length:
                # FASTQ
                if hasattr(record, 'accuracy'):
                    outfp.write(
                        '@{name}\n{seq}\n'
                        '+\n{acc}\n'.format(name=record.name,
                                            seq=record.sequence,
                                            acc=record.accuracy))

                #FASTA
                else:
                    outfp.write(
                        '>{name}\n{seq}\n'.format(name=record.name,
                                                seq=record.sequence))
                                                

if __name__ == '__main__':
    main()
