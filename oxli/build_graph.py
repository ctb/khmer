#! /usr/bin/env python2
#
# This file is part of khmer, http://github.com/ged-lab/khmer/, and is
# Copyright (C) Michigan State University, 2009-2015. It is licensed under
# the three-clause BSD license; see doc/LICENSE.txt.
# Contact: khmer-project@idyll.org
#
# pylint: disable=invalid-name,missing-docstring
"""
Build a graph from the given sequences, save in <ptname>.

% python scripts/load-graph.py <ptname> <data1> [ <data2> <...> ]

Use '-h' for parameter help.
"""

import sys

import khmer
from khmer.khmer_args import build_hashbits_args
from khmer.khmer_args import (report_on_config, info, add_threading_args)
from khmer.kfile import check_file_status, check_space
from khmer.kfile import check_space_for_hashtable
from oxli import common


def add_args(parser):
    add_threading_args(parser)
    parser.add_argument('--no-build-tagset', '-n', default=False,
                        action='store_true', dest='no_build_tagset',
                        help='Do NOT construct tagset while loading sequences')
    parser.add_argument('output_filename',
                        metavar='output_presence_table_filename', help='output'
                        ' k-mer presence table filename.')
    parser.add_argument('input_filenames', metavar='input_sequence_filename',
                        nargs='+', help='input FAST[AQ] sequence filename')
    parser.add_argument('--report-total-kmers', '-t', action='store_true',
                        help="Prints the total number of k-mers to stderr")
    parser.add_argument('--write-fp-rate', '-w', action='store_true',
                        help="Write false positive rate into .info file")
    parser.add_argument('-f', '--force', default=False, action='store_true',
                        help='Overwrite output file if it exists')


def do_build_graph(output_filename, input_filenames, force=False,
                   no_build_tagset=False,
                   report_total_kmers=False, write_fp_rate=False, quiet=False,
                   ksize=common.get_env_ksize(),
                   n_tables=common.get_env_n_tables(),
                   min_tablesize=common.get_env_tablesize(),
                   threads=common.DEFAULT_N_THREADS):

    info('load-graph.py', ['graph'])
    # report_on_config( hashtype='hashbits')

    base = output_filename
    filenames = input_filenames

    for _ in input_filenames:
        check_file_status(_, force)

    check_space(input_filenames, force)
    check_space_for_hashtable(float(n_tables * min_tablesize) / 8., force)

    print >>sys.stderr, 'Saving k-mer presence table to %s' % base
    print >>sys.stderr, 'Loading kmers from sequences in %s' % repr(filenames)
    if no_build_tagset:
        print >>sys.stderr, 'We WILL NOT build the tagset.'
    else:
        print >>sys.stderr, 'We WILL build the tagset', \
                            ' (for partitioning/traversal).'

    # I don't know why these stopped working
    # config = khmer.get_config()
    # config.set_reads_input_buffer_size(threads * 64 * 1024)

    print >>sys.stderr, 'making k-mer presence table'
    htable = khmer.new_hashbits(ksize, min_tablesize, n_tables)

    if no_build_tagset:
        target_method = htable.consume_fasta_with_reads_parser
    else:
        target_method = htable.consume_fasta_and_tag_with_reads_parser

    for _, filename in enumerate(filenames):

        rparser = khmer.ReadParser(filename)
        print >>sys.stderr, 'consuming input', filename
        target_method(rparser)

    if report_total_kmers:
        print >> sys.stderr, 'Total number of unique k-mers: {0}'.format(
            htable.n_unique_kmers())

    print >>sys.stderr, 'saving k-mer presence table in', base + '.pt'
    htable.save(base + '.pt')

    if not no_build_tagset:
        print >>sys.stderr, 'saving tagset in', base + '.tagset'
        htable.save_tagset(base + '.tagset')

    info_fp = open(base + '.info', 'w')
    info_fp.write('%d unique k-mers' % htable.n_unique_kmers())

    fp_rate = khmer.calc_expected_collisions(htable)
    print >>sys.stderr, 'fp rate estimated to be %1.3f' % fp_rate
    if write_fp_rate:
        print >> info_fp, \
            '\nfalse positive rate estimated to be %1.3f' % fp_rate

    if fp_rate > 0.15:          # 0.18 is ACTUAL MAX. Do not change.
        print >> sys.stderr, "**"
        print >> sys.stderr, ("** ERROR: the graph structure is too small for "
                              "this data set. Increase table size/# tables.")
        print >> sys.stderr, "**"
        sys.exit(1)

    print >> sys.stderr, 'wrote to', base + '.info and', base + '.pt'
    if not no_build_tagset:
        print >> sys.stderr, 'and ' + base + '.tagset'

if __name__ == '__main__':
    main()

# vim: set ft=python ts=4 sts=4 sw=4 et tw=79:
