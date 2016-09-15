# This file is part of khmer, https://github.com/dib-lab/khmer/, and is
# Copyright (C) 2010-2015, Michigan State University.
# Copyright (C) 2015-2016, The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of the Michigan State University nor the names
#       of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written
#       permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Contact: khmer-project@idyll.org
# pylint: disable=missing-docstring,protected-access,no-member,invalid-name

from __future__ import print_function
from __future__ import absolute_import

import khmer
from khmer import ReadParser

import screed

import pytest

from . import khmer_tst_utils as utils


def teardown():
    utils.cleanup()


@pytest.mark.huge
def test_toobig():
    try:
        khmer.Nodegraph(32, 1e13, 1)
        assert 0, "This should fail"
    except MemoryError as err:
        print(str(err))


def test_update_from():
    nodegraph = khmer.Nodegraph(5, 1000, 4)
    other_nodegraph = khmer.Nodegraph(5, 1000, 4)

    assert nodegraph.get('AAAAA') == 0
    assert nodegraph.get('GCGCG') == 0
    assert nodegraph.n_occupied() == 0
    assert other_nodegraph.get('AAAAA') == 0
    assert other_nodegraph.get('GCGCG') == 0
    assert other_nodegraph.n_occupied() == 0

    other_nodegraph.count('AAAAA')

    assert nodegraph.get('AAAAA') == 0
    assert nodegraph.get('GCGCG') == 0
    assert nodegraph.n_occupied() == 0
    assert other_nodegraph.get('AAAAA') == 1
    assert other_nodegraph.get('GCGCG') == 0
    assert other_nodegraph.n_occupied() == 1

    nodegraph.count('GCGCG')

    assert nodegraph.get('AAAAA') == 0
    assert nodegraph.get('GCGCG') == 1
    assert nodegraph.n_occupied() == 1
    assert other_nodegraph.get('AAAAA') == 1
    assert other_nodegraph.get('GCGCG') == 0
    assert other_nodegraph.n_occupied() == 1

    nodegraph.update(other_nodegraph)

    assert nodegraph.get('AAAAA') == 1
    assert nodegraph.get('GCGCG') == 1
    assert nodegraph.n_occupied() == 2
    assert other_nodegraph.get('AAAAA') == 1
    assert other_nodegraph.get('GCGCG') == 0
    assert other_nodegraph.n_occupied() == 1


def test_update_from_2():

    ng1 = khmer.Nodegraph(20, 1000, 4)
    ng2 = khmer.Nodegraph(20, 1000, 4)

    filename = utils.get_test_data('random-20-a.fa')
    ng1.consume_fasta(filename)
    ng2.consume_fasta(filename)

    assert ng1.n_occupied() == ng2.n_occupied()
    ng1.update(ng2)

    assert ng1.n_occupied() == ng2.n_occupied()


def test_update_from_diff_ksize_2():
    nodegraph = khmer.Nodegraph(5, 1000, 4)
    other_nodegraph = khmer.Nodegraph(4, 1000, 4)

    try:
        nodegraph.update(other_nodegraph)
        assert 0, "should not be reached"
    except ValueError as err:
        print(str(err))

    try:
        other_nodegraph.update(nodegraph)
        assert 0, "should not be reached"
    except ValueError as err:
        print(str(err))


def test_update_from_diff_tablesize():
    nodegraph = khmer.Nodegraph(5, 100, 4)
    other_nodegraph = khmer.Nodegraph(5, 1000, 4)

    try:
        nodegraph.update(other_nodegraph)
        assert 0, "should not be reached"
    except ValueError as err:
        print(str(err))


def test_update_from_diff_num_tables():
    nodegraph = khmer.Nodegraph(5, 1000, 3)
    other_nodegraph = khmer.Nodegraph(5, 1000, 4)

    try:
        nodegraph.update(other_nodegraph)
        assert 0, "should not be reached"
    except ValueError as err:
        print(str(err))


def test_n_occupied_1():
    filename = utils.get_test_data('random-20-a.fa')

    ksize = 20  # size of kmer
    htable_size = 100000  # size of hashtable
    num_nodegraphs = 1  # number of hashtables

    # test modified c++ n_occupied code
    nodegraph = khmer.Nodegraph(ksize, htable_size, num_nodegraphs)

    for _, record in enumerate(screed.open(filename)):
        nodegraph.consume(record.sequence)

    # this number calculated independently
    assert nodegraph.n_occupied() == 3884, nodegraph.n_occupied()


def test_bloom_python_1():
    # test python code to count unique kmers using bloom filter
    filename = utils.get_test_data('random-20-a.fa')

    ksize = 20  # size of kmer
    htable_size = 100000  # size of hashtable
    num_nodegraphs = 3  # number of hashtables

    nodegraph = khmer.Nodegraph(ksize, htable_size, num_nodegraphs)

    n_unique = 0
    for _, record in enumerate(screed.open(filename)):
        sequence = record.sequence
        seq_len = len(sequence)
        for num in range(0, seq_len + 1 - ksize):
            kmer = sequence[num:num + ksize]
            if not nodegraph.get(kmer):
                n_unique += 1
            nodegraph.count(kmer)

    assert n_unique == 3960
    assert nodegraph.n_occupied() == 3884, nodegraph.n_occupied()

    # this number equals n_unique
    assert nodegraph.n_unique_kmers() == 3960, nodegraph.n_unique_kmers()


def test_bloom_c_1():
    # test c++ code to count unique kmers using bloom filter

    filename = utils.get_test_data('random-20-a.fa')

    ksize = 20  # size of kmer
    htable_size = 100000  # size of hashtable
    num_nodegraphs = 3  # number of hashtables

    nodegraph = khmer.Nodegraph(ksize, htable_size, num_nodegraphs)

    for _, record in enumerate(screed.open(filename)):
        nodegraph.consume(record.sequence)

    assert nodegraph.n_occupied() == 3884
    assert nodegraph.n_unique_kmers() == 3960


def test_n_occupied_2():  # simple one
    ksize = 4

    nodegraph = khmer._Nodegraph(ksize, [11])
    nodegraph.count('AAAA')  # 00 00 00 00 = 0
    assert nodegraph.n_occupied() == 1

    nodegraph.count('ACTG')  # 00 10 01 11 =
    assert nodegraph.n_occupied() == 2

    nodegraph.count('AACG')  # 00 00 10 11 = 11  # collision 1

    assert nodegraph.n_occupied() == 2
    nodegraph.count('AGAC')   # 00  11 00 10 # collision 2
    assert nodegraph.n_occupied() == 2, nodegraph.n_occupied()


def test_n_occupied_2_add_is_count():  # 'add' synonym for 'count'
    ksize = 4

    nodegraph = khmer._Nodegraph(ksize, [11])
    nodegraph.add('AAAA')  # 00 00 00 00 = 0
    assert nodegraph.n_occupied() == 1

    nodegraph.add('ACTG')  # 00 10 01 11 =
    assert nodegraph.n_occupied() == 2

    nodegraph.add('AACG')  # 00 00 10 11 = 11  # collision 1

    assert nodegraph.n_occupied() == 2
    nodegraph.add('AGAC')   # 00  11 00 10 # collision 2
    assert nodegraph.n_occupied() == 2, nodegraph.n_occupied()


def test_bloom_c_2():  # simple one
    ksize = 4

    # use only 1 hashtable, no bloom filter
    nodegraph = khmer._Nodegraph(ksize, [11])
    nodegraph.count('AAAA')  # 00 00 00 00 = 0
    nodegraph.count('ACTG')  # 00 10 01 11 =
    assert nodegraph.n_unique_kmers() == 2
    nodegraph.count('AACG')  # 00 00 10 11 = 11  # collision  with 1st kmer
    assert nodegraph.n_unique_kmers() == 2
    nodegraph.count('AGAC')   # 00  11 00 10 # collision  with 2nd kmer
    assert nodegraph.n_unique_kmers() == 2

    # use two hashtables with 11,13
    other_nodegraph = khmer._Nodegraph(ksize, [11, 13])
    other_nodegraph.count('AAAA')  # 00 00 00 00 = 0

    other_nodegraph.count('ACTG')  # 00 10 01 11 = 2*16 +4 +3 = 39
    assert other_nodegraph.n_unique_kmers() == 2
    # 00 00 10 11 = 11  # collision with only 1st kmer
    other_nodegraph.count('AACG')
    assert other_nodegraph.n_unique_kmers() == 3
    other_nodegraph.count('AGAC')
    # 00  11 00 10  3*16 +2 = 50
    # collision with both 2nd and 3rd kmers

    assert other_nodegraph.n_unique_kmers() == 3


def test_count_within_radius_simple():
    inpfile = utils.get_test_data('all-A.fa')
    nodegraph = khmer._Nodegraph(4, [3, 5])

    print(nodegraph.consume_fasta(inpfile))
    n = nodegraph.count_kmers_within_radius('AAAA', 1)
    assert n == 1

    n = nodegraph.count_kmers_within_radius('AAAA', 10)
    assert n == 1


def test_count_within_radius_big():
    inpfile = utils.get_test_data('random-20-a.fa')
    nodegraph = khmer.Nodegraph(20, 1e5, 4)

    nodegraph.consume_fasta(inpfile)
    n = nodegraph.count_kmers_within_radius('CGCAGGCTGGATTCTAGAGG', int(1e6))
    assert n == 3961, n

    nodegraph = khmer.Nodegraph(21, 1e5, 4)
    nodegraph.consume_fasta(inpfile)
    n = nodegraph.count_kmers_within_radius('CGCAGGCTGGATTCTAGAGGC', int(1e6))
    assert n == 39


def test_count_kmer_degree():
    inpfile = utils.get_test_data('all-A.fa')
    nodegraph = khmer._Nodegraph(4, [3, 5])
    nodegraph.consume_fasta(inpfile)

    assert nodegraph.kmer_degree('AAAA') == 2
    assert nodegraph.kmer_degree('AAAT') == 1
    assert nodegraph.kmer_degree('AATA') == 0
    assert nodegraph.kmer_degree('TAAA') == 1


def test_kmer_neighbors():
    inpfile = utils.get_test_data('all-A.fa')
    nodegraph = khmer._Nodegraph(4, [3, 5])
    nodegraph.consume_fasta(inpfile)

    h = khmer.forward_hash('AAAA', 4)
    print(type('AAAA'))
    assert nodegraph.neighbors(h) == [0, 0]       # AAAA on both sides
    assert nodegraph.neighbors('AAAA') == [0, 0]  # AAAA on both sides

    h = khmer.forward_hash('AAAT', 4)
    assert nodegraph.neighbors(h) == [0]          # AAAA on one side
    assert nodegraph.neighbors('AAAT') == [0]     # AAAA on one side

    h = khmer.forward_hash('AATA', 4)
    assert nodegraph.neighbors(h) == []           # no neighbors
    assert nodegraph.neighbors('AATA') == []      # AAAA on one side

    h = khmer.forward_hash('TAAA', 4)
    assert nodegraph.neighbors(h) == [0]          # AAAA on both sides
    assert nodegraph.neighbors('TAAA') == [0]     # AAAA on both sides


def test_kmer_neighbors_wrong_ksize():
    inpfile = utils.get_test_data('all-A.fa')
    nodegraph = khmer._Nodegraph(4, [3, 5])
    nodegraph.consume_fasta(inpfile)

    try:
        nodegraph.neighbors('AAAAA')
        assert 0, "neighbors() should fail with too long string"
    except ValueError:
        pass

    try:
        nodegraph.neighbors(b'AAAAA')
        assert 0, "neighbors() should fail with too long string"
    except ValueError:
        pass

    try:
        nodegraph.neighbors({})
        assert 0, "neighbors() should fail with non hash/str arg"
    except ValueError:
        pass


def test_get_ksize():
    kh = khmer._Nodegraph(22, [1])
    assert kh.ksize() == 22


def test_get_hashsizes():
    kh = khmer.Nodegraph(22, 100, 4)
    # Py2/3 hack, longify converts to long in py2, remove once py2 isn't
    # supported any longer.
    expected = utils.longify([97, 89, 83, 79])
    assert kh.hashsizes() == expected, kh.hashsizes()


def test_get_raw_tables():
    kh = khmer.Nodegraph(10, 1e6, 4)
    kh.consume('ATGGAGAGAC')
    kh.consume('AGTGGCGATG')
    kh.consume('ATAGACAGGA')
    tables = kh.get_raw_tables()

    for size, table in zip(kh.hashsizes(), tables):
        assert isinstance(table, memoryview)
        assert size == len(table)


def test_simple_median():
    hi = khmer.Nodegraph(6, 1e5, 2)

    (median, average, stddev) = hi.get_median_count("AAAAAA")
    print(median, average, stddev)
    assert median == 0
    assert average == 0.0
    assert stddev == 0.0

    hi.consume("AAAAAA")
    (median, average, stddev) = hi.get_median_count("AAAAAA")
    print(median, average, stddev)
    assert median == 1
    assert average == 1.0
    assert stddev == 0.0


def test_badget():
    hbts = khmer.Nodegraph(6, 1e6, 1)

    dna = "AGCTTTTCATTCTGACTGCAACGGGCAATATGTCTCTGTGTGGATTAAAAAAAGAGTGTCTGATAG"

    hbts.consume(dna)

    assert hbts.get("AGCTTT") == 1

    assert hbts.get("GATGAG") == 0

    try:
        hbts.get(b"AGCTT")
        assert 0, "this should fail"
    except ValueError as err:
        print(str(err))

    try:
        hbts.get(u"AGCTT")
        assert 0, "this should fail"
    except ValueError as err:
        print(str(err))


#


def test_load_notexist_should_fail():
    savepath = utils.get_temp_filename('tempnodegraphsave0.htable')

    hi = khmer._Countgraph(12, [1])
    try:
        hi.load(savepath)
        assert 0, "load should fail"
    except OSError:
        pass


def test_load_truncated_should_fail():
    inpath = utils.get_test_data('random-20-a.fa')
    savepath = utils.get_temp_filename('tempnodegraphsave0.ct')

    hi = khmer.Countgraph(12, 1000, 2)

    hi.consume_fasta(inpath)
    hi.save(savepath)

    fp = open(savepath, 'rb')
    data = fp.read()
    fp.close()

    fp = open(savepath, 'wb')
    fp.write(data[:1000])
    fp.close()

    hi = khmer._Countgraph(12, [1])
    try:
        hi.load(savepath)
        assert 0, "load should fail"
    except OSError as e:
        print(str(e))


def test_hashbits_file_version_check():
    nodegraph = khmer._Nodegraph(12, [1])

    inpath = utils.get_test_data('badversion-k12.htable')

    try:
        nodegraph.load(inpath)
        assert 0, "this should fail"
    except OSError as e:
        print(str(e))


def test_nodegraph_file_type_check():
    kh = khmer._Countgraph(12, [1])
    savepath = utils.get_temp_filename('tempcountingsave0.ct')
    kh.save(savepath)

    nodegraph = khmer._Nodegraph(12, [1])

    try:
        nodegraph.load(savepath)
        assert 0, "this should fail"
    except OSError as e:
        print(str(e))


def test_bad_primes_list():
    try:
        khmer._Nodegraph(31, ["a", "b", "c"], 1)
        assert 0, "Bad primes list should fail"
    except TypeError as e:
        print(str(e))


def test_consume_absentfasta_with_reads_parser():
    nodegraph = khmer._Nodegraph(31, [1])
    try:
        nodegraph.consume_fasta_with_reads_parser()
        assert 0, "this should fail"
    except TypeError as err:
        print(str(err))
    try:
        readparser = ReadParser(utils.get_test_data('empty-file'))
        nodegraph.consume_fasta_with_reads_parser(readparser)
        assert 0, "this should fail"
    except OSError as err:
        print(str(err))
    except ValueError as err:
        print(str(err))


def test_bad_primes():
    try:
        khmer._Nodegraph.__new__(
            khmer._Nodegraph, 6, ["a", "b", "c"])
        assert 0, "this should fail"
    except TypeError as e:
        print(str(e))


def test_n_occupied_save_load():
    filename = utils.get_test_data('random-20-a.fa')

    nodegraph = khmer.Nodegraph(20, 100000, 3)

    for _, record in enumerate(screed.open(filename)):
        nodegraph.consume(record.sequence)

    assert nodegraph.n_occupied() == 3884
    assert nodegraph.n_unique_kmers() == 3960

    savefile = utils.get_temp_filename('out')
    nodegraph.save(savefile)

    ng2 = khmer.load_nodegraph(savefile)
    assert ng2.n_occupied() == 3884, ng2.n_occupied()
    assert ng2.n_unique_kmers() == 0    # this is intended behavior, sigh.


def test_n_occupied_vs_countgraph():
    filename = utils.get_test_data('random-20-a.fa')

    nodegraph = khmer.Nodegraph(20, 100000, 3)
    countgraph = khmer.Countgraph(20, 100000, 3)

    assert nodegraph.n_occupied() == 0, nodegraph.n_occupied()
    assert countgraph.n_occupied() == 0, countgraph.n_occupied()

    assert nodegraph.n_unique_kmers() == 0, nodegraph.n_unique_kmers()
    assert countgraph.n_unique_kmers() == 0, countgraph.n_unique_kmers()

    for _, record in enumerate(screed.open(filename)):
        nodegraph.consume(record.sequence)
        countgraph.consume(record.sequence)

    assert nodegraph.hashsizes() == nodegraph.hashsizes()

    # these are all the same -- good :).
    assert nodegraph.n_occupied() == 3884, nodegraph.n_occupied()
    assert countgraph.n_occupied() == 3884, countgraph.n_occupied()

    assert nodegraph.n_unique_kmers() == 3960, nodegraph.n_unique_kmers()
    assert countgraph.n_unique_kmers() == 3960, countgraph.n_unique_kmers()


def test_n_occupied_vs_countgraph_another_size():
    filename = utils.get_test_data('random-20-a.fa')

    nodegraph = khmer.Nodegraph(20, 10000, 3)
    countgraph = khmer.Countgraph(20, 10000, 3)

    assert nodegraph.n_occupied() == 0, nodegraph.n_occupied()
    assert countgraph.n_occupied() == 0, countgraph.n_occupied()

    assert nodegraph.n_unique_kmers() == 0, nodegraph.n_unique_kmers()
    assert countgraph.n_unique_kmers() == 0, countgraph.n_unique_kmers()

    for _, record in enumerate(screed.open(filename)):
        nodegraph.consume(record.sequence)
        countgraph.consume(record.sequence)

    assert nodegraph.hashsizes() == nodegraph.hashsizes()

    # these are all the same -- good :).
    assert nodegraph.n_occupied() == 3269, nodegraph.n_occupied()
    assert countgraph.n_occupied() == 3269, countgraph.n_occupied()

    assert nodegraph.n_unique_kmers() == 3916, nodegraph.n_unique_kmers()
    assert countgraph.n_unique_kmers() == 3916, countgraph.n_unique_kmers()


def test_traverse_linear_path():
    contigfile = utils.get_test_data('simple-genome.fa')
    contig = list(screed.open(contigfile))[0].sequence

    K = 21

    nodegraph = khmer.Nodegraph(K, 1e5, 4)
    stopgraph = khmer.Nodegraph(K, 1e5, 4)

    nodegraph.consume(contig)

    degree_nodes = khmer.HashSet(K)
    size, conns, visited = nodegraph.traverse_linear_path(contig[:K],
                                                          degree_nodes,
                                                          stopgraph)
    assert size == 980
    assert len(conns) == 0
    assert len(visited) == 980


def test_find_high_degree_nodes():
    contigfile = utils.get_test_data('simple-genome.fa')
    contig = list(screed.open(contigfile))[0].sequence

    K = 21

    nodegraph = khmer.Nodegraph(K, 1e5, 4)
    stopgraph = khmer.Nodegraph(K, 1e5, 4)

    nodegraph.consume(contig)

    degree_nodes = nodegraph.find_high_degree_nodes(contig)
    assert len(degree_nodes) == 0


def test_find_high_degree_nodes_2():
    contigfile = utils.get_test_data('simple-genome.fa')
    contig = list(screed.open(contigfile))[0].sequence

    K = 21

    nodegraph = khmer.Nodegraph(K, 1e5, 4)

    nodegraph.consume(contig)
    nodegraph.count(contig[2:22] + 'G')   # will add another neighbor to 1:22
    print(nodegraph.neighbors(contig[1:22]))

    degree_nodes = nodegraph.find_high_degree_nodes(contig)
    assert len(degree_nodes) == 1
    assert nodegraph.hash(contig[1:22]) in degree_nodes


def test_traverse_linear_path_2():
    contigfile = utils.get_test_data('simple-genome.fa')
    contig = list(screed.open(contigfile))[0].sequence
    print('contig len', len(contig))

    K = 21

    nodegraph = khmer.Nodegraph(K, 1e5, 4)
    stopgraph = khmer.Nodegraph(K, 1e5, 4)

    nodegraph.consume(contig)
    nodegraph.count(contig[101:121] + 'G')  # will add another neighbor
    print(nodegraph.neighbors(contig[101:122]))

    degree_nodes = nodegraph.find_high_degree_nodes(contig)

    assert len(degree_nodes) == 1
    assert nodegraph.hash(contig[100:121]) in degree_nodes

    # traverse from start, should end at node 100:121
    size, conns, visited = nodegraph.traverse_linear_path(contig[0:21],
                                                          degree_nodes,
                                                          stopgraph)

    print(size, list(conns), list(visited))
    assert size == 100
    assert len(visited) == 100
    assert nodegraph.hash(contig[100:121]) in conns
    assert len(conns) == 1

    # traverse from immediately after 100:121, should end at the end
    size, conns, visited = nodegraph.traverse_linear_path(contig[101:122],
                                                          degree_nodes,
                                                          stopgraph)

    print(size, list(conns), list(visited))
    assert size == 879
    assert len(visited) == 879
    assert nodegraph.hash(contig[100:121]) in conns
    assert len(conns) == 1

    # traverse from end, should end at 100:121
    size, conns, visited = nodegraph.traverse_linear_path(contig[-21:],
                                                          degree_nodes,
                                                          stopgraph)

    print(size, list(conns), len(visited))
    assert size == 879
    assert len(visited) == 879
    assert nodegraph.hash(contig[100:121]) in conns
    assert len(conns) == 1


def test_traverse_linear_path_3_stopgraph():
    contigfile = utils.get_test_data('simple-genome.fa')
    contig = list(screed.open(contigfile))[0].sequence
    print('contig len', len(contig))

    K = 21

    nodegraph = khmer.Nodegraph(K, 1e5, 4)
    stopgraph = khmer.Nodegraph(K, 1e5, 4)

    nodegraph.consume(contig)
    nodegraph.count(contig[101:121] + 'G')  # will add another neighbor
    print(nodegraph.neighbors(contig[101:122]))

    degree_nodes = nodegraph.find_high_degree_nodes(contig)

    assert len(degree_nodes) == 1
    assert nodegraph.hash(contig[100:121]) in degree_nodes

    stopgraph.count(contig[101:122])      # stop traversal - only adj to start

    size, conns, visited = nodegraph.traverse_linear_path(contig[101:122],
                                                          degree_nodes,
                                                          stopgraph)

    print(size, list(conns), len(visited))
    assert size == 0
    assert len(visited) == 0
    assert len(conns) == 0
