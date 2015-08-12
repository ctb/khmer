#! /usr/bin/env python
#
# This file is part of khmer, https://github.com/dib-lab/khmer/, and is
# Copyright (C) Michigan State University, 2009-2015. It is licensed under
# the three-clause BSD license; see LICENSE.
# Contact: khmer-project@idyll.org
#
# pylint: disable=invalid-name,missing-docstring
"""
Find an initial set of highly connected k-mers, to save on repartitioning time.

% python scripts/make-initial-stoptags.py <base>
"""
from __future__ import print_function

import sys
import textwrap
import khmer
from khmer import khmer_args
from khmer.khmer_args import (build_counting_args, info, sanitize_epilog)
from khmer.kfile import check_input_files

DEFAULT_SUBSET_SIZE = int(1e4)
DEFAULT_COUNTING_HT_SIZE = 3e6                # number of bytes
DEFAULT_COUNTING_HT_N = 4                     # number of counting hash tables

# Lump removal parameters.  Probably shouldn't be changed, but who knows?
#
# explanation:
#
# We will walk EXCURSION_DISTANCE out from each tag; if we find more than
# EXCURSION_KMER_THRESHOLD kmers within that range, this will be a "big"
# excursion and we will track all k-mers visited.  If we find that any
# k-mer has been visited more than EXCURSION_KMER_COUNT_THRESHOLD times,
# we will mark it as BAD and make it a stop tag for traversal.

# don't change these!
EXCURSION_DISTANCE = 40
EXCURSION_KMER_THRESHOLD = 200
EXCURSION_KMER_COUNT_THRESHOLD = 5


def get_parser():
    epilog = """
    Loads a k-mer nodegraph/tagset pair created by
    :program:`load-into-nodegraph.py`, and
    does a small set of traversals from graph waypoints; on these traversals,
    looks for k-mers that are repeatedly traversed in high-density regions of
    the graph, i.e. are highly connected. Outputs those k-mers as an initial
    set of stoptags, which can be fed into :program:`partition-graph.py`,
    :program:`find-knots.py`, and :program:`filter-stoptags.py`.

    The k-mer countgraph size options parameters are for a k-mer countgraph
    to keep track of repeatedly-traversed k-mers. The subset size option
    specifies the number of waypoints from which to traverse; for highly
    connected data sets, the default (1000) is probably ok.
    """
    parser = build_counting_args(
        descr="Find an initial set of highly connected k-mers.",
        epilog=textwrap.dedent(epilog))
    parser.add_argument('--subset-size', '-s', default=DEFAULT_SUBSET_SIZE,
                        dest='subset_size', type=float,
                        help='Set subset size (default 1e4 is prob ok)')
    parser.add_argument('--stoptags', '-S', metavar='filename', default='',
                        help="Use stoptags in this file during partitioning")
    parser.add_argument('graphbase', help='basename for input and output '
                        'filenames')
    parser.add_argument('-f', '--force', default=False, action='store_true',
                        help='Overwrite output file if it exists')
    return parser


def main():

    info('make-initial-stoptags.py', ['graph'])
    args = sanitize_epilog(get_parser()).parse_args()

    graphbase = args.graphbase

    # @RamRS: This might need some more work
    infiles = [graphbase, graphbase + '.tagset']
    if args.stoptags:
        infiles.append(args.stoptags)
    for _ in infiles:
        check_input_files(_, args.force)

    print('loading nodegraph %s.pt' % graphbase, file=sys.stderr)
    nodegraph = khmer.load_nodegraph(graphbase)

    # do we want to load stop tags, and do they exist?
    if args.stoptags:
        print('loading stoptags from', args.stoptags, file=sys.stderr)
        nodegraph.load_stop_tags(args.stoptags)

    print('loading tagset %s.tagset...' % graphbase, file=sys.stderr)
    nodegraph.load_tagset(graphbase + '.tagset')

    counting = khmer_args.create_countgraph(args)

    # divide up into SUBSET_SIZE fragments
    divvy = nodegraph.divide_tags_into_subsets(args.subset_size)

    # pick off the first one
    if len(divvy) == 1:
        start, end = 0, 0
    else:
        start, end = divvy[:2]

    # partition!
    print('doing pre-partitioning from', start, 'to', end, file=sys.stderr)
    subset = nodegraph.do_subset_partition(start, end)

    # now, repartition...
    print('repartitioning to find HCKs.', file=sys.stderr)
    nodegraph.repartition_largest_partition(subset, counting,
                                            EXCURSION_DISTANCE,
                                            EXCURSION_KMER_THRESHOLD,
                                            EXCURSION_KMER_COUNT_THRESHOLD)

    print('saving stop tags', file=sys.stderr)
    nodegraph.save_stop_tags(graphbase + '.stoptags')
    print('wrote to:', graphbase + '.stoptags', file=sys.stderr)

if __name__ == '__main__':
    main()
