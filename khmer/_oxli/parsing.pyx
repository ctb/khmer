# -*- coding: UTF-8 -*-
cimport cython
from cython.operator cimport dereference as deref
from libcpp cimport bool
from libcpp.string cimport string

import sys

from khmer._oxli.utils cimport _bstring, _ustring
from khmer._oxli.sequence cimport (Alphabets, Sequence, CpSequence,
                                   CpSequencePair, ReadBundle, is_valid,
                                   sanitize_sequence)



def print_error(msg):
    """Print the given message to 'stderr'."""

    print(msg, file=sys.stderr)


class UnpairedReadsError(ValueError):
    """ValueError with refs to the read pair in question."""

    def __init__(self, msg, r1, r2):
        r1_name = "<no read>"
        r2_name = "<no read>"
        if r1:
            r1_name = r1.name
        if r2:
            r2_name = r2.name

        msg = msg + '\n"{0}"\n"{1}"'.format(r1_name, r2_name)
        ValueError.__init__(self, msg)
        self.read1 = r1
        self.read2 = r2


cdef class FastxParser:

    def __cinit__(self, filename, *args, **kwargs):
        self._this = get_parser[CpFastxReader](_bstring(filename))
        if self.is_complete():
            raise RuntimeError('{0} has no sequences!'.format(filename))

    cdef Sequence _next(self):
        if not self.is_complete():
            seq = Sequence._wrap(deref(self._this).get_next_read())
            seq.clean()
            return seq
        else:
            return None

    cpdef bool is_complete(self):
        return deref(self._this).is_complete()

    def __iter__(self):
        cdef Sequence seq
        while not self.is_complete():
            seq = self._next()
            yield seq

    @property
    def num_reads(self):
        return deref(self._this).get_num_reads()


cdef class SanitizedFastxParser(FastxParser):

    def __cinit__(self, filename, alphabet='DNAN_SIMPLE',
                        bool convert_n=True):
        self.n_bad = 0
        self.convert_n = convert_n
        self._alphabet = Alphabets._get(alphabet)

    cdef Sequence _next(self):
        cdef Sequence seq
        cdef bool good

        if not self.is_complete():
            seq = FastxParser._next(self)
            good = sanitize_sequence(seq._obj.sequence,
                                     self._alphabet,
                                     self.convert_n)
            if not good:
                self.n_bad += 1
                return None
            else:
                seq._obj.cleaned_seq = seq._obj.sequence
                return seq
        else:
            return None

    def __iter__(self):
        cdef Sequence seq
        while not self.is_complete():
            seq = self._next()
            if seq is not None:
                yield seq


cdef class SplitPairedReader:

    def __cinit__(self, FastxParser left_parser,
                         FastxParser right_parser,
                         int min_length=-1,
                         bool force_name_match=False):

        self.left_parser = left_parser
        self.right_parser = right_parser
        self.min_length = min_length
        self.force_name_match = force_name_match

    def __iter__(self):
        cdef Sequence first, second
        cdef object err
        cdef read_num = 0
        cdef int found

        found, first, second, err = self._next()
        while found != 0:
            if err is not None:
                raise err

            if self.min_length > 0:
                if len(first) >= self.min_length and \
                   len(second) >= self.min_length:

                    yield read_num, True, first, second
            else:
                yield read_num, True, first, second

            read_num += found
            found, first, second, err = self._next()

    cdef tuple _next(self):
        cdef Sequence first = self.left_parser._next()
        cdef bool first_complete = self.left_parser.is_complete()

        cdef Sequence second = self.right_parser._next()
        cdef bool second_complete = self.right_parser.is_complete()


        if first_complete is not second_complete:
            err = UnpairedReadsError('Differing lengths of left '\
                                     'and right files!')
            return -1, None, None, err

        if first_complete:
            return 0, None, None, None

        if first is None or second is None:
            return 1, first, second, None

        if self.force_name_match:
            if _check_is_pair(first, second):
                return 2, first, second, None
            else:
                err =  UnpairedReadsError('', first, second)
                return -1, None, None, err
        else:
            return 2, first, second, None


cdef class BrokenPairedReader:

    def __cinit__(self, FastxParser parser,
                  int min_length=-1,
                  bool force_single=False,
                  bool require_paired=False):

        if force_single and require_paired:
            raise ValueError("force_single and require_paired cannot both be set!")

        self.parser = parser
        self.min_length = min_length
        self.force_single = force_single
        self.require_paired = require_paired

        self.record = None

    def __iter__(self):
        cdef Sequence first
        cdef Sequence second
        cdef object err
        cdef int found
        cdef int read_num = 0

        found, first, second, err = self._next()
        while (found != 0):
            if err is not None:
                raise err

            if self.min_length > 0:
                if first is not None and len(first) < self.min_length:
                    first = None
                    found -= 1
                if second is not None and len(second) < self.min_length:
                    second = None
                    found -= 1

            if self.force_single:
                if first is not None:
                    yield read_num, found == 2, first, None
                    read_num += found
                if second is not None:
                    yield read_num, found == 2, second, None
                    read_num += found
            elif self.require_paired:
                if first is not None and second is not None:
                    yield read_num, found == 2, first, second
                    read_num += found
            else:
                if first is not None or second is not None:
                    yield read_num, found == 2, first, second
                    read_num += found
            found, first, second, err = self._next()

    cdef tuple _next(self):
        cdef Sequence first, second
        cdef int is_pair

        if self.record is None:
            first = self.parser._next()
            if first is None:
                if self.parser.is_complete():
                    return 0, None, None, None
                else:
                    if self.require_paired:
                        err = UnpairedReadsError(
                            "Uneven number of reads when require_paired is set!",
                            first)
                        return -1, None, None, err
                    return 1, first, None, None
        else:
            first = self.record

        second = self.parser._next()

        # check if paired
        if second is not None and first is not None:
            is_pair = _check_is_pair(first, second)
            if is_pair == -1:
                err = ValueError("records must be same type (FASTA or FASTQ)")
                return -1, None, None, err
            if is_pair and not self.force_single:
                self.record = None
                return 2, first, second, None    # found 2 proper records
            else:   # orphan.
                if self.require_paired:
                    err = UnpairedReadsError(
                        "Unpaired reads when require_paired is set!",
                        first, second)
                    return -1, None, None, err
                self.record = second
                return 1, first, None, None
        elif self.parser.is_complete():
            # ran out of reads getting second, handle last record
            if self.require_paired:
                err =  UnpairedReadsError("Unpaired reads when require_paired "
                                          "is set!", first, None)
                return -1, None, None, err
            self.record = None
            return 1, first, second, None
        else: # one read was invalid, but that doesn't mean they were unpaired
            return 1, first, second, None


cpdef tuple _split_left_right(unicode s):
    cdef string cppstr = s.encode('UTF-8')
    return _cppstring_split_left_right(cppstr)


cdef tuple _cppstring_split_left_right(string& s):
    """Split record name at the first whitespace and return both parts.

    RHS is set to an empty string if not present.
    """
    cdef unsigned int i
    cdef const char * c_str = s.c_str()
    cdef unicode lhs, rhs
    lhs = u''
    rhs = u''
    for i in range(len(s)):
        if lhs == u'':
            if (c_str[i] == b' ' or c_str[i] == b'\t'):
                lhs = _ustring(c_str[0:i])
        else:
            if c_str[i] != b' ' or c_str[i] != b'\t':
                rhs = _ustring(c_str[i:len(s)])
                break
    lhs = _ustring(c_str[0:len(s)])  if lhs == u'' else lhs
    return lhs, rhs


cdef int _check_is_pair(Sequence first, Sequence second):
    """Check if the two sequence records belong to the same fragment.

    In an matching pair the records are left and right pairs
    of each other, respectively.  Returns True or False as appropriate.

    Handles both Casava formats: seq/1 and seq/2, and 'seq::... 1::...'
    and 'seq::... 2::...'.

    Also handles the default format of the SRA toolkit's fastq-dump:
    'Accession seq/1'
    """
    if first.quality is None or second.quality is None:
        if first.quality is not second.quality:
            return -1

    cdef unicode lhs1, rhs1, lhs2, rhs2
    lhs1, rhs1 = _cppstring_split_left_right(first._obj.name)
    lhs2, rhs2 = _cppstring_split_left_right(second._obj.name)

    # handle 'name/1'
    cdef unicode subpart1, subpart2
    if lhs1.endswith('/1') and lhs2.endswith('/2'):
        subpart1 = lhs1.split('/', 1)[0]
        subpart2 = lhs2.split('/', 1)[0]

        if subpart1 and subpart1 == subpart2:
            return 1

    # handle '@name 1:rst'
    elif lhs1 == lhs2 and rhs1.startswith('1:') and rhs2.startswith('2:'):
        return 1

    # handle @name seq/1
    elif lhs1 == lhs2 and rhs1.endswith('/1') and rhs2.endswith('/2'):
        subpart1 = rhs1.split('/', 1)[0]
        subpart2 = rhs2.split('/', 1)[0]

        if subpart1 and subpart1 == subpart2:
            return 1

    return 0


def check_is_pair(first, second):
    if type(first) is not Sequence:
        first = Sequence.from_screed_record(first)
    if type(second) is not Sequence:
        second = Sequence.from_screed_record(second)
    cdef int ret = _check_is_pair(first, second)
    if ret == -1:
        raise ValueError("both records must be same type (FASTA or FASTQ)")
    return ret == 1


cpdef bool check_is_left(s):
    """Check if the name belongs to a 'left' sequence (/1).

    Returns True or False.

    Handles both Casava formats: seq/1 and 'seq::... 1::...'
    """
    cdef unicode lhs, rhs
    lhs, rhs = _split_left_right(_ustring(s))
    if lhs.endswith('/1'):              # handle 'name/1'
        return True
    elif rhs.startswith('1:'):          # handle '@name 1:rst'
        return True

    elif rhs.endswith('/1'):            # handles '@name seq/1'
        return True

    return False


cpdef bool check_is_right(s):
    """Check if the name belongs to a 'right' sequence (/2).

    Returns True or False.

    Handles both Casava formats: seq/2 and 'seq::... 2::...'
    """
    cdef unicode lhs, rhs
    lhs, rhs = _split_left_right(_ustring(s))
    if lhs.endswith('/2'):              # handle 'name/2'
        return True
    elif rhs.startswith('2:'):          # handle '@name 2:rst'
        return True

    elif rhs.endswith('/2'):            # handles '@name seq/2'
        return True

    return False
