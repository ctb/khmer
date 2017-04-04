# -*- coding: UTF-8 -*-
#
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

import itertools
import random

import khmer
from khmer.khmer_args import estimate_optimal_with_K_and_f as optimal_fp
from khmer import ReadParser
from khmer import reverse_complement as revcomp
from . import khmer_tst_utils as utils

import pytest
import screed

from .graph_features import *
from .graph_features import K


def teardown():
    utils.cleanup()


class TestLabeledAssembler:

    def test_beginning_to_end_across_tip(self, right_tip_structure):
        # assemble entire contig, ignoring branch point b/c of labels
        graph, contig, L, HDN, R, tip = right_tip_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)
        hdn = graph.find_high_degree_nodes(contig)
        # L, HDN, and R will be labeled with 1
        lh.label_across_high_degree_nodes(contig, hdn, 1)

        path = asm.assemble(contig[:K])

        assert len(path) == 1, "there should only be one path"
        path = path[0]  # @CTB

        assert len(path) == len(contig)
        assert utils._equals_rc(path, contig)

    def test_assemble_right_double_fork(self, right_double_fork_structure):
        # assemble two contigs from a double forked structure
        graph, contig, L, HDN, R, branch = right_double_fork_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        hdn = graph.find_high_degree_nodes(contig)
        hdn += graph.find_high_degree_nodes(branch)
        print(list(hdn))
        lh.label_across_high_degree_nodes(contig, hdn, 1)
        lh.label_across_high_degree_nodes(branch, hdn, 2)
        print(lh.get_tag_labels(list(hdn)[0]))

        paths = asm.assemble(contig[:K])
        print('Path lengths', [len(x) for x in paths])

        assert len(paths) == 2

        assert any(utils._equals_rc(path, contig) for path in paths)
        assert any(utils._equals_rc(path, branch) for path in paths)

    def test_assemble_right_triple_fork(self, right_triple_fork_structure):
        # assemble three contigs from a trip fork
        (graph, contig, L, HDN, R,
         top_sequence, bottom_sequence) = right_triple_fork_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        hdn = graph.find_high_degree_nodes(contig)
        hdn += graph.find_high_degree_nodes(top_sequence)
        hdn += graph.find_high_degree_nodes(bottom_sequence)
        print(list(hdn))
        lh.label_across_high_degree_nodes(contig, hdn, 1)
        lh.label_across_high_degree_nodes(top_sequence, hdn, 2)
        lh.label_across_high_degree_nodes(bottom_sequence, hdn, 3)
        print(lh.get_tag_labels(list(hdn)[0]))

        paths = asm.assemble(contig[:K])
        print([len(x) for x in paths])

        assert len(paths) == 3

        assert any(utils._equals_rc(path, contig) for path in paths)
        assert any(utils._equals_rc(path, top_sequence) for path in paths)
        assert any(utils._equals_rc(path, bottom_sequence) for path in paths)

    def test_assemble_left_double_fork(self, left_double_fork_structure):
        # assemble entire contig + branch points b/c of labels; start from end
        graph, contig, L, HDN, R, branch = left_double_fork_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        # first try without the labels
        paths = asm.assemble(contig[-K:])

        assert len(paths) == 1
        # without labels, should get the beginning of the HDN thru the end
        assert paths[0] == contig[HDN.pos:]

        # now add labels and check that we get two full length paths
        hdn = graph.find_high_degree_nodes(contig)
        hdn += graph.find_high_degree_nodes(branch)
        print(list(hdn))
        lh.label_across_high_degree_nodes(contig, hdn, 1)
        lh.label_across_high_degree_nodes(branch, hdn, 2)
        print(lh.get_tag_labels(list(hdn)[0]))

        paths = asm.assemble(contig[-K:])

        assert len(paths) == 2

        assert any(utils._equals_rc(path, contig) for path in paths)
        assert any(utils._equals_rc(path, branch) for path in paths)

    def test_assemble_snp_bubble_single(self, snp_bubble_structure):
        # assemble entire contig + one of two paths through a bubble
        graph, wildtype, mutant, HDN_L, HDN_R = snp_bubble_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        hdn = graph.find_high_degree_nodes(wildtype)
        assert len(hdn) == 2
        lh.label_across_high_degree_nodes(wildtype, hdn, 1)

        paths = asm.assemble(wildtype[:K])

        assert len(paths) == 1
        assert utils._equals_rc(paths[0], wildtype)

    def test_assemble_snp_bubble_both(self, snp_bubble_structure):
        # assemble entire contig + both paths
        graph, wildtype, mutant, HDN_L, HDN_R = snp_bubble_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        hdn = graph.find_high_degree_nodes(wildtype)
        hdn += graph.find_high_degree_nodes(mutant)
        assert len(hdn) == 2
        lh.label_across_high_degree_nodes(wildtype, hdn, 1)
        lh.label_across_high_degree_nodes(mutant, hdn, 2)

        paths = asm.assemble(wildtype[:K])

        assert len(paths) == 2

        assert any(utils._contains_rc(wildtype, path) for path in paths)
        assert any(utils._contains_rc(mutant, path) for path in paths)
        # assert all(path[:HDN_L.pos+K][-K:] == HDN_L for path in paths)
        # assert all(path[HDN_R.pos:][:K] == HDN_R for path in paths)
        # assert paths[0][:HDN_L.pos+K] == paths[1][:HDN_L.pos+K]
        # assert paths[0][HDN_R.pos:] == paths[1][HDN_R.pos:]

    def test_assemble_snp_bubble_stopbf(self, snp_bubble_structure):
        # assemble one side of bubble, blocked with stop_filter,
        # when labels on both branches
        # stop_filter should trip a filter failure, negating the label spanning
        graph, wildtype, mutant, HDN_L, HDN_R = snp_bubble_structure
        stop_filter = khmer.Nodegraph(K, 1e5, 4)
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)

        hdn = graph.find_high_degree_nodes(wildtype)
        hdn += graph.find_high_degree_nodes(mutant)
        assert len(hdn) == 2
        lh.label_across_high_degree_nodes(wildtype, hdn, 1)
        lh.label_across_high_degree_nodes(mutant, hdn, 2)

        # do the labeling, but block the mutant with stop_filter
        stop_filter.count(mutant[HDN_L.pos + 1:HDN_L.pos + K + 1])
        paths = asm.assemble(wildtype[:K], stop_filter)

        assert len(paths) == 1
        assert any(utils._equals_rc(path, wildtype) for path in paths)

    # @pytest.mark.skip(reason='destroys your computer and then the world')
    def test_assemble_tandem_repeats(self, tandem_repeat_structure):
        # assemble one copy of a tandem repeat
        graph, repeat, tandem_repeats = tandem_repeat_structure
        lh = khmer._GraphLabels(graph)
        asm = khmer.SimpleLabeledAssembler(lh)
        paths = asm.assemble(repeat[:K])

        assert len(paths) == 1
        # There are K-1 k-mers spanning the junction between
        # the beginning and end of the repeat
        assert len(paths[0]) == len(repeat) + K - 1


class TestJunctionCountAssembler:

    def test_beginning_to_end_across_tip(self, right_tip_structure):
        # assemble entire contig, ignoring branch point b/c of labels
        graph, contig, L, HDN, R, tip = right_tip_structure
        asm = khmer.JunctionCountAssembler(graph)
        asm.consume(contig)
        asm.consume(contig)
        asm.consume(contig)

        path = asm.assemble(contig[:K])
        print('P:', path[0])
        print('T:', tip)
        print('C:', contig)
        assert len(path) == 1, "there should only be one path"
        path = path[0]  # @CTB

        assert len(path) == len(contig)
        assert utils._equals_rc(path, contig)
