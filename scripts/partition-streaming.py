from __future__ import print_function

import sys
import screed
import os
import khmer
import textwrap
from khmer import khmer_args
from khmer.khmer_args import (build_counting_args, add_loadgraph_args,
                              report_on_config, info, calculate_graphsize,
                              sanitize_help, check_argument_range)
from khmer._oxli.partitioning import StreamingPartitioner
import argparse
from khmer.utils import (write_record, broken_paired_reader, ReadBundle)


def write_stats(partitioner, folder, n, sample):
    sample = os.path.basename(sample)
    filename = os.path.join(folder,
                            '{0}-{1}.stats.csv'.format(sample, n))
    print('# {0}: {1} tags, {2} components.'.format(n, partitioner.n_tags, 
                                                    partitioner.n_components))
    print('  writing results to file -> {0}'.format(filename))
    partitioner.write_components(filename)

def main():
    parser = build_counting_args()
    parser.add_argument('-stats-dir', default='component_stats')
    parser.add_argument('samples', nargs='+')
    parser.add_argument('-Z', dest='norm', default=10)
    parser.add_argument('--stats-interval', default=50000)
    args = parser.parse_args()

    graph = khmer_args.create_countgraph(args)
    partitioner = StreamingPartitioner(graph)

    try:
        os.mkdir(args.stats_dir)
    except OSError as e:
        pass

    for sample in args.samples:
        print('== Starting {0} =='.format(sample))
        for n, read in enumerate(screed.open(sample)):
            if n % 1000 == 0:
                print (n, '...', sep='')
            if args.stats_interval > 0 and n > 0 and n % args.stats_interval == 0:
                write_stats(partitioner, args.stats_dir, n, sample)
            cov, _, _ = graph.get_median_count(read.sequence)
            if cov < args.norm:
                graph.consume(read.sequence)
            else:
                seq, pos = graph.trim_on_abundance(read.sequence, 2)
                if len(seq) < args.ksize:
                    continue
                partitioner.consume_sequence(read.sequence)

        write_stats(partitioner, args.stats_dir, n, sample)

if __name__ == '__main__':
    main()
