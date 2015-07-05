//
// This file is part of khmer, https://github.com/dib-lab/khmer/, and is
// Copyright (C) Michigan State University, 2009-2015. It is licensed under
// the three-clause BSD license; see LICENSE.
// Contact: khmer-project@idyll.org
//

#ifndef KMER_HASH_HH
#define KMER_HASH_HH

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "khmer.hh"

// test validity
#ifdef KHMER_EXTRA_SANITY_CHECKS
#   define is_valid_dna(ch) ((toupper(ch)) == 'A' || (toupper(ch)) == 'C' || \
			    (toupper(ch)) == 'G' || (toupper(ch)) == 'T')
#else
// NOTE: Assumes data is already sanitized as it should be by parsers.
//	     This assumption eliminates 4 function calls.
#   define is_valid_dna(ch) ((ch) == 'A' || (ch) == 'C' || \
			     (ch) == 'G' || (ch) == 'T')
#endif

// bit representation of A/T/C/G.
#ifdef KHMER_EXTRA_SANITY_CHECKS
#   define twobit_repr(ch) ((toupper(ch)) == 'A' ? 0LL : \
			    (toupper(ch)) == 'T' ? 1LL : \
			    (toupper(ch)) == 'C' ? 2LL : 3LL)
#else
// NOTE: Assumes data is already sanitized as it should be by parsers.
//	     This assumption eliminates 4 function calls.
#   define twobit_repr(ch) ((ch) == 'A' ? 0LL : \
			    (ch) == 'T' ? 1LL : \
			    (ch) == 'C' ? 2LL : 3LL)
#endif

#define revtwobit_repr(n) ((n) == 0 ? 'A' : \
                           (n) == 1 ? 'T' : \
                           (n) == 2 ? 'C' : 'G')

#ifdef KHMER_EXTRA_SANITY_CHECKS
#   define twobit_comp(ch) ((toupper(ch)) == 'A' ? 1LL : \
			    (toupper(ch)) == 'T' ? 0LL : \
			    (toupper(ch)) == 'C' ? 3LL : 2LL)
#else
// NOTE: Assumes data is already sanitized as it should be by parsers.
//	     This assumption eliminates 4 function calls.
#   define twobit_comp(ch) ((ch) == 'A' ? 1LL : \
			    (ch) == 'T' ? 0LL : \
			    (ch) == 'C' ? 3LL : 2LL)
#endif

// choose wisely between forward and rev comp.
#ifndef NO_UNIQUE_RC
#define uniqify_rc(f, r) ((f) < (r) ? (f) : (r))
#else
#define uniqify_rc(f,r)(f)
#endif


namespace khmer
{
// two-way hash functions.
HashIntoType _hash(const char * kmer, const WordLength k);
HashIntoType _hash(const char * kmer, const WordLength k,
                   HashIntoType& h, HashIntoType& r);
HashIntoType _hash_forward(const char * kmer, WordLength k);

std::string _revhash(HashIntoType hash, WordLength k);

// two-way hash functions, MurmurHash3.
HashIntoType _hash_murmur(const std::string& kmer);
HashIntoType _hash_murmur(const std::string& kmer,
                          HashIntoType& h, HashIntoType& r);
HashIntoType _hash_murmur_forward(const std::string& kmer);

class Kmer
{

public:

    HashIntoType kmer_f, kmer_r, kmer_u;

    Kmer(HashIntoType f, HashIntoType r, HashIntoType u)
    {
        kmer_f = f;
        kmer_r = r;
        kmer_u = u;
    }
	Kmer()
	{
		kmer_f = kmer_r = kmer_u = 0;
	}

    operator HashIntoType() const { return kmer_u; }
	operator unsigned int() const { return kmer_u; }

    bool operator< (const Kmer &other) const {
        return kmer_u < other.kmer_u;
    }

    std::string get_string_rep(WordLength K) {
        return _revhash(kmer_u, K);
    }

    const char * get_char_rep(WordLength K) {
		std::string kmer_s = _revhash(kmer_u, K);
        return kmer_s.c_str();
    }
};

/*
 * At first, I had many contructors defined on Kmer itself. However, it quickly
 * became apparent that this would not work, as the need to pass in a uint
 * for K made the calls ambiguous. Then I moved it to Traverser, but that
 * really felt like I was shoehorning it in. So here we are, with a factory
 * object. All apologies.
 */
class KmerFactory
{
protected:
    WordLength _ksize;

public:

    KmerFactory(WordLength K): _ksize(K) {}

    Kmer build_kmer(HashIntoType kmer_u)
    {
        HashIntoType kmer_f, kmer_r;
		std:: string kmer_s = _revhash(kmer_u, _ksize);
        _hash(kmer_s.c_str(), _ksize, kmer_f, kmer_r);
        return Kmer(kmer_f, kmer_r, kmer_u);
    }

    Kmer build_kmer(HashIntoType kmer_f, HashIntoType kmer_r)
    {
        HashIntoType kmer_u = uniqify_rc(kmer_f, kmer_r);
        return Kmer(kmer_f, kmer_r, kmer_u);
    }

    Kmer build_kmer(std::string kmer_s)
    {
        HashIntoType kmer_f, kmer_r, kmer_u;
        kmer_u = _hash(kmer_s.c_str(), _ksize, kmer_f, kmer_r);
        return Kmer(kmer_f, kmer_r, kmer_u);
    }

    Kmer build_kmer(const char * kmer_c)
    {
        HashIntoType kmer_f, kmer_r, kmer_u;
        kmer_u = _hash(kmer_c, _ksize, kmer_f, kmer_r);
        return Kmer(kmer_f, kmer_r, kmer_u);
    }
};

//
// Sequence iterator class, test.  Not really a C++ iterator yet.
//

class KmerIterator: public KmerFactory
{
protected:
    const char * _seq;

    HashIntoType _kmer_f, _kmer_r;
    HashIntoType bitmask;
    unsigned int _nbits_sub_1;
    unsigned int index;
    size_t length;
    bool initialized;
public:
    KmerIterator(const char * seq, unsigned char k);

    Kmer first(HashIntoType& f, HashIntoType& r);

    Kmer next(HashIntoType& f, HashIntoType& r);

    Kmer first()
    {
        return first(_kmer_f, _kmer_r);
    }

    Kmer next()
    {
        return next(_kmer_f, _kmer_r);
    }

    bool done()
    {
        return index >= length;
    }

    unsigned int get_start_pos() const
    {
        return index - _ksize;
    }

    unsigned int get_end_pos() const
    {
        return index;
    }
}; // class KmerIterator

};

#endif // KMER_HASH_HH
