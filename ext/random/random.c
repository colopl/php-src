/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
   |          Zeev Suraski <zeev@php.net>                                 |
   |          Sascha Schumann <sascha@schumann.cx>                        |
   |          Pedro Melo <melo@ip.pt>                                     |
   |          Sterling Hughes <sterling@php.net>                          |
   |          Sammy Kaye Powers <me@sammyk.me>                            |
   |          Go Kudo <g-kudo@colopl.co.jp>                               |
   |                                                                      |
   | Based on code from: Richard J. Wagner <rjwagner@writeme.com>         |
   |                     Makoto Matsumoto <matumoto@math.keio.ac.jp>      |
   |                     Takuji Nishimura                                 |
   |                     Shawn Cokus <Cokus@math.washington.edu>          |
   |                     David Blackman                                   |
   |                     Sebastiano Vigna <vigna@acm.org>                 |
   |                     Melissa O'Neill <oneill@pcg-random.org>          |
   +----------------------------------------------------------------------+
*/

/*
	The following php_mt_...() functions are based on a C++ class MTRand by
	Richard J. Wagner. For more information see the web page at
	http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/VERSIONS/C-LANG/MersenneTwister.h

	Mersenne Twister random number generator -- a C++ class MTRand
	Based on code by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus
	Richard J. Wagner  v1.0  15 May 2003  rjwagner@writeme.com

	The Mersenne Twister is an algorithm for generating random numbers.  It
	was designed with consideration of the flaws in various other generators.
	The period, 2^19937-1, and the order of equidistribution, 623 dimensions,
	are far greater.  The generator is also fast; it avoids multiplication and
	division, and it benefits from caches and pipelines.  For more information
	see the inventors' web page at http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html

	Reference
	M. Matsumoto and T. Nishimura, "Mersenne Twister: A 623-Dimensionally
	Equidistributed Uniform Pseudo-Random Number Generator", ACM Transactions on
	Modeling and Computer Simulation, Vol. 8, No. 1, January 1998, pp 3-30.

	Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
	Copyright (C) 2000 - 2003, Richard J. Wagner
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	1. Redistributions of source code must retain the above copyright
	   notice, this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	3. The names of its contributors may not be used to endorse or promote
	   products derived from this software without specific prior written
	   permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
	A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>

#include "php.h"
#include "ext/standard/php_array.h"
#include "ext/standard/php_string.h"

#include "ext/spl/spl_exceptions.h"
#include "Zend/zend_exceptions.h"

#include "php_random.h"

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef PHP_WIN32
# include "win32/time.h"
# include "win32/winutil.h"
# include <process.h>
#else
# include <sys/time.h>
#endif

#ifdef __linux__
# include <sys/syscall.h>
#endif

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
# if (__FreeBSD__ && __FreeBSD_version > 1200000) || (__DragonFly__ && __DragonFly_version >= 500700) || defined(__sun)
#  include <sys/random.h>
# endif
#endif

#if __has_feature(memory_sanitizer)
# include <sanitizer/msan_interface.h>
#endif

#include "random_arginfo.h"

ZEND_DECLARE_MODULE_GLOBALS(random)

PHPAPI zend_class_entry *random_ce_Random_Engine;
PHPAPI zend_class_entry *random_ce_Random_CryptoSafeEngine;
PHPAPI zend_class_entry *random_ce_Random_SeedableEngine;
PHPAPI zend_class_entry *random_ce_Random_SerializableEngine;

PHPAPI zend_class_entry *random_ce_Random_Engine_CombinedLCG;
PHPAPI zend_class_entry *random_ce_Random_Engine_MersenneTwister;
PHPAPI zend_class_entry *random_ce_Random_Engine_PCG64;
PHPAPI zend_class_entry *random_ce_Random_Engine_Secure;
PHPAPI zend_class_entry *random_ce_Random_Randomizer;

static zend_object_handlers random_engine_combinedlcg_object_handlers;
static zend_object_handlers random_engine_mersennetwister_object_handlers;
static zend_object_handlers random_engine_pcg64_object_handlers;
static zend_object_handlers random_engine_secure_object_handlers;
static zend_object_handlers random_randomizer_object_handlers;

/* ---- COMMON BEGIN --- */
static inline uint32_t rand_range32(const php_random_algo *algo, php_random_status *status, uint32_t umax)
{
	uint32_t result, limit, r;
	size_t total_size = 0;
	uint32_t count = 0;

	result = algo->generate(status);
	total_size = status->last_generated_size;
	if (status->last_unsafe) {
		return 0;
	}
	if (total_size < sizeof(uint32_t)) {
		r = algo->generate(status);
		result = (result << status->last_generated_size) | r;
		total_size += status->last_generated_size;
		if (status->last_unsafe) {
			return 0;
		}
	}

	/* Special case where no modulus is required */
	if (UNEXPECTED(umax == UINT32_MAX)) {
		return true;
	}

	/* Increment the max so range is inclusive of max */
	umax++;

	/* Powers of two are not biased */
	if ((umax & (umax - 1)) == 0) {
		return result & (umax - 1);
	}

	/* Ceiling under which UINT32_MAX % max == 0 */
	limit = UINT32_MAX - (UINT32_MAX % umax) - 1;

	/* Discard numbers over the limit to avoid modulo bias */
	while (UNEXPECTED(result > limit)) {
		/* If the requirements cannot be met in a cycles, return fail */
		if (++count > 50) {
			status->last_unsafe = true;
			return 0;
		}
		
		result = algo->generate(status);
		total_size = status->last_generated_size;
		if (status->last_unsafe) {
			return 0;
		}
		while (total_size < sizeof(uint32_t)) {
			r = algo->generate(status);
			result = (result << status->last_generated_size) | r;
			total_size += status->last_generated_size;
			if (status->last_unsafe) {
				return 0;
			}
		}
	}

	return result % umax;
}

static inline uint64_t rand_range64(const php_random_algo *algo, php_random_status *status, uint64_t umax)
{
	uint64_t result, limit, r;
	size_t total_size = 0;
	uint32_t count = 0;

	result = algo->generate(status);
	total_size = status->last_generated_size;
	if (status->last_unsafe) {
		return 0;
	}
	if (total_size < sizeof(uint64_t)) {
		r = algo->generate(status);
		result = (result << status->last_generated_size) | r;
		total_size += status->last_generated_size;
		if (status->last_unsafe) {
			return 0;
		}
	}

	/* Special case where no modulus is required */
	if (UNEXPECTED(umax == UINT64_MAX)) {
		return result;
	}

	/* Increment the max so range is inclusive of max */
	umax++;

	/* Powers of two are not biased */
	if ((umax & (umax - 1)) == 0) {
		return result & (umax - 1);
	}

	/* Ceiling under which UINT64_MAX % max == 0 */
	limit = UINT64_MAX - (UINT64_MAX % umax) - 1;

	/* Discard numbers over the limit to avoid modulo bias */
	while (UNEXPECTED(result > limit)) {
		/* If the requirements cannot be met in a cycles, return fail */
		if (++count > 50) {
			status->last_unsafe = true;
			return 0;
		}
		
		result = algo->generate(status);
		total_size = status->last_generated_size;
		if (status->last_unsafe) {
			return 0;
		}
		while (total_size < sizeof(uint64_t)) {
			r = algo->generate(status);
			result = (result << status->last_generated_size) | r;
			total_size += status->last_generated_size;
			if (status->last_unsafe) {
				return 0;
			}
		}
	}

	return result % umax;
}

static inline zend_long common_range(const php_random_algo *algo, php_random_status *status, zend_long min, zend_long max)
{
	zend_ulong umax = max - min;

	if (PHP_RANDOM_ALGO_IS_DYNAMIC(algo) || algo->generate_size > sizeof(uint32_t) || umax > UINT32_MAX) {
		return (zend_long) rand_range64(algo, status, umax) + min;
	}

	return (zend_long) rand_range32(algo, status, umax) + min;

}

static void engine_common_free_obj(zend_object *obj)
{
	php_random_engine *engine = php_random_engine_from_obj(obj);

	if (engine->status) {
		php_random_status_free(engine->status);
	}

	zend_object_std_dtor(obj);
}

static zend_object *engine_common_clone_obj(zend_object *obj)
{
	php_random_engine *old_engine = php_random_engine_from_obj(obj);
	php_random_engine *new_engine = php_random_engine_from_obj(old_engine->std.ce->create_object(old_engine->std.ce));

	new_engine->algo = old_engine->algo;
	if (old_engine->status) {
		new_engine->status = php_random_status_copy(old_engine->algo, old_engine->status, new_engine->status);
	}

	zend_objects_clone_members(&new_engine->std, &old_engine->std);

	return &new_engine->std;
}
/* ---- COMMON END ---- */

/* ---- COMMON API BEGIN ---- */
PHPAPI php_random_status *php_random_status_allocate(const php_random_algo *algo)
{
	php_random_status *status = ecalloc(1, sizeof(php_random_status));

	status->last_generated_size = algo->generate_size;
	status->last_unsafe = false;
	status->state = algo->state_size > 0 ? ecalloc(1, algo->state_size) : NULL;

	return status;
}

PHPAPI php_random_status *php_random_status_copy(const php_random_algo *algo, php_random_status *old_status, php_random_status *new_status)
{
	new_status->last_generated_size = old_status->last_generated_size;
	new_status->last_unsafe = old_status->last_unsafe;
	new_status->state = memcpy(new_status->state, old_status->state, algo->state_size);

	return new_status;
}

PHPAPI void php_random_status_free(php_random_status *status)
{
	if (status->state) {
		efree(status->state);
	}
	efree(status);
}

PHPAPI php_random_engine *php_random_engine_common_init(zend_class_entry *ce, zend_object_handlers *handlers, const php_random_algo *algo)
{
	php_random_engine *engine = zend_object_alloc(sizeof(php_random_engine), ce);

	zend_object_std_init(&engine->std, ce);
	object_properties_init(&engine->std, ce);

	engine->algo = algo;
	engine->status = php_random_status_allocate(engine->algo);
	engine->std.handlers = handlers;

	return engine;
}

PHPAPI void php_random_engine_common_free_object(zend_object *object)
{
	engine_common_free_obj(object);
}

PHPAPI zend_object *php_random_engine_common_clone_object(zend_object *object)
{
	return engine_common_clone_obj(object);
}
/* ---- COMMON API END ---- */

/* ---- COMBINED LCG BEGIN ---- */
/*
 * combinedLCG() returns a pseudo random number in the range of (0, 1).
 * The function combines two CGs with periods of
 * 2^31 - 85 and 2^31 - 249. The period of this function
 * is equal to the product of both primes.
 */
#define MODMULT(a, b, c, m, s) q = s/a;s=b*(s-a*q)-c*q;if(s<0)s+=m

static inline void combinedlcg_seed_default(php_random_status *status)
{
	php_random_status_state_combinedlcg *s = status->state;
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == 0) {
		s->state[0] = tv.tv_usec ^ (tv.tv_usec << 11);
	} else {
		s->state[0] = 1;
	}

#ifdef ZTS
	s->state[1] = (zend_long) tsrm_thread_id();
#else
	s->state[1] = (zend_long) getpid();
#endif

	/* Add entropy to s2 by calling gettimeofday() again */
	if (gettimeofday(&tv, NULL) == 0) {
		s->state[1] ^= (tv.tv_usec << 11);
	}
}

static void combinedlcg_seed(php_random_status *status, uint64_t seed)
{
	php_random_status_state_combinedlcg *s = status->state;

	s->state[0] = seed & 0xffffffffU;
	s->state[1] = seed >> 32;
}

static uint64_t combinedlcg_generate(php_random_status *status)
{
	php_random_status_state_combinedlcg *s = status->state;
	int32_t q, z;

	MODMULT(53668, 40014, 12211, 2147483563L, s->state[0]);
	MODMULT(52774, 40692, 3791, 2147483399L, s->state[1]);

	z = s->state[0] - s->state[1];
	if (z < 1) {
		z += 2147483562;
	}

	return (uint64_t) z;
}

static zend_long combinedlcg_range(php_random_status *status, zend_long min, zend_long max)
{
	return common_range(&php_random_algo_combinedlcg, status, min, max);
}

static bool combinedlcg_serialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_combinedlcg *s = status->state;
	zval t;
	uint32_t i;

	for (i = 0; i < 2; i++) {
		ZVAL_LONG(&t, s->state[i]);
		zend_hash_next_index_insert(data, &t);
	}

	return true;
}

static bool combinedlcg_unserialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_combinedlcg *s = status->state;
	zval *t;
	uint32_t i;

	for (i = 0; i < 2; i++) {
		t = zend_hash_index_find(data, i);
		if (!t || Z_TYPE_P(t) != IS_LONG) {
			return false;
		}
		s->state[i] = Z_LVAL_P(t);
	}

	return true;
}

const php_random_algo php_random_algo_combinedlcg = {
	sizeof(uint32_t),
	sizeof(php_random_status_state_combinedlcg),
	combinedlcg_seed,
	combinedlcg_generate,
	combinedlcg_range,
	combinedlcg_serialize,
	combinedlcg_unserialize
};

static zend_object *php_random_engine_combinedlcg_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce,&random_engine_combinedlcg_object_handlers, &php_random_algo_combinedlcg)->std;
}
/* ---- COMBINED LCG END ---- */

/* ---- MT BEGIN ---- */
#define N             MT_N                 /* length of state vector */
#define M             (397)                /* a period parameter */
#define hiBit(u)      ((u) & 0x80000000U)  /* mask all but highest   bit of u */
#define loBit(u)      ((u) & 0x00000001U)  /* mask all but lowest    bit of u */
#define loBits(u)     ((u) & 0x7FFFFFFFU)  /* mask     the highest   bit of u */
#define mixBits(u, v) (hiBit(u)|loBits(v)) /* move hi bit of u to hi bit of v */

#define twist(m,u,v)  (m ^ (mixBits(u,v)>>1) ^ ((uint32_t)(-(int32_t)(loBit(v))) & 0x9908b0dfU))
#define twist_php(m,u,v)  (m ^ (mixBits(u,v)>>1) ^ ((uint32_t)(-(int32_t)(loBit(u))) & 0x9908b0dfU))

static inline void mersennetwister_reload(php_random_status_state_mersennetwister *state) {
	uint32_t *p = state->state;
	int i;

	if (state->mode == MT_RAND_MT19937) {
		for (i = N - M; i--; ++p) {
			*p = twist(p[M], p[0], p[1]);
		}
		for (i = M; --i; ++p) {
			*p = twist(p[M-N], p[0], p[1]);
		}
		*p = twist(p[M-N], p[0], state->state[0]);
	} else {
		for (i = N - M; i--; ++p) {
			*p = twist_php(p[M], p[0], p[1]);
		}
		for (i = M; --i; ++p) {
			*p = twist_php(p[M-N], p[0], p[1]);
		}
		*p = twist_php(p[M-N], p[0], state->state[0]);
	}

	state->count = 0;
}

static void mersennetwister_seed(php_random_status *status, uint64_t seed)
{
	/* Initialize generator state with seed
	   See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	   In previous versions, most significant bits (MSBs) of the seed affect
	   only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto. */
	php_random_status_state_mersennetwister *s = status->state;

	s->state[0] = seed & 0xffffffffU;
	for (s->count = 1; s->count < MT_N; s->count++) {
		s->state[s->count] = (1812433253U * (s->state[s->count - 1] ^ (s->state[s->count - 1] >> 30)) + s->count) & 0xffffffffU;
	}

	mersennetwister_reload(s);
}

static inline void mersennetwister_seed_default(php_random_status *status)
{
	zend_long seed = 0;

	if (php_random_bytes_silent(&seed, sizeof(zend_long)) == FAILURE) {
		/* FIXME: This is a security risk. Shall I throw an E_WARNING? */
		seed = GENERATE_SEED();
	}

	mersennetwister_seed(status, (uint64_t) seed);
}

static uint64_t mersennetwister_generate(php_random_status *status)
{
	php_random_status_state_mersennetwister *s = status->state;
	uint32_t s1;

	if (s->count >= MT_N) {
		mersennetwister_reload(s);
	}

	s1 = s->state[s->count++];
	s1 ^= (s1 >> 11);
	s1 ^= (s1 << 7) & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;

	return (uint64_t) (s1 ^ (s1 >> 18));
}

static zend_long mersennetwister_range(php_random_status *status, zend_long min, zend_long max)
{
	php_random_status_state_mersennetwister *s = status->state;

	if (s->mode == MT_RAND_MT19937) {
		return common_range(&php_random_algo_mersennetwister, status, min, max);
	}

	uint64_t r = php_random_algo_mersennetwister.generate(status) >> 1;
	/* Legacy mode deliberately not inside php_mt_rand_range()
	 * to prevent other functions being affected */
	RAND_RANGE_BADSCALING(r, min, max, PHP_MT_RAND_MAX);
	return (zend_long) r;
}

static bool mersennetwister_serialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_mersennetwister *s = status->state;
	zval tmp;
	uint32_t i;

	for (i = 0; i< MT_N; i++) {
		ZVAL_LONG(&tmp, s->state[i]);
		zend_hash_next_index_insert(data, &tmp);
	}
	ZVAL_LONG(&tmp, s->count);
	zend_hash_next_index_insert(data, &tmp);
	ZVAL_LONG(&tmp, s->mode);
	zend_hash_next_index_insert(data, &tmp);
	
	return true;
}

static bool mersennetwister_unserialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_mersennetwister *s = status->state;
	zval *tmp;
	uint32_t i;

	for (i = 0; i < MT_N; i++) {
		tmp = zend_hash_index_find(data, i);
		if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
			return false;
		}

		s->state[i] = Z_LVAL_P(tmp);
	}
	tmp = zend_hash_index_find(data, MT_N); 
	if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
		return false;
	}
	s->count = Z_LVAL_P(tmp);
	tmp = zend_hash_index_find(data, MT_N + 1);
	if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
		return false;
	}
	s->mode = Z_LVAL_P(tmp);

	return true;
}

const php_random_algo php_random_algo_mersennetwister = {
	sizeof(uint32_t),
	sizeof(php_random_status_state_mersennetwister),
	mersennetwister_seed,
	mersennetwister_generate,
	mersennetwister_range,
	mersennetwister_serialize,
	mersennetwister_unserialize
};

static zend_object *php_random_engine_mersennetwister_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_mersennetwister_object_handlers, &php_random_algo_mersennetwister)->std;
}
/* ---- MT END ---- */

/* ---- PCG64 BEGIN ---- */
static inline void pcg64s_advance(php_random_status_state_pcg64s *state, uint64_t advance) {	
	php_random_uint128_t
		cur_mult = php_random_uint128_constant(2549297995355413924ULL,4865540595714422341ULL),
		cur_plus = php_random_uint128_constant(6364136223846793005ULL,1442695040888963407ULL),
		acc_mult = php_random_uint128_constant(0ULL, 1ULL),
		acc_plus = php_random_uint128_constant(0ULL, 0ULL);

	while (advance > 0) {
		if (advance & 1) {
			acc_mult = php_random_uint128_multiply(acc_mult, cur_mult);
			acc_plus = php_random_uint128_add(php_random_uint128_multiply(acc_plus, cur_mult), cur_plus);
		}
		cur_plus = php_random_uint128_multiply(php_random_uint128_add(cur_mult, php_random_uint128_constant(0ULL, 1ULL)), cur_plus);
		cur_mult = php_random_uint128_multiply(cur_mult, cur_mult);
		advance /= 2;
	}

	state->state = php_random_uint128_add(php_random_uint128_multiply(acc_mult, state->state), acc_plus);
}

static inline void pcg64s_step(php_random_status_state_pcg64s *s) {
	s->state = php_random_uint128_add(
		php_random_uint128_multiply(s->state, php_random_uint128_constant(2549297995355413924ULL,4865540595714422341ULL)),
		php_random_uint128_constant(6364136223846793005ULL,1442695040888963407ULL)
	);
}

static inline void pcg64s_seed_128(php_random_status *status, php_random_uint128_t seed)
{
	php_random_status_state_pcg64s *s = status->state;
	s->state = php_random_uint128_constant(0ULL, 0ULL);
	pcg64s_step(s);
	s->state = php_random_uint128_add(s->state, seed);
	pcg64s_step(s);
}

static void pcg64s_seed(php_random_status *status, uint64_t seed)
{
	pcg64s_seed_128(status, php_random_uint128_constant(0ULL, seed));
}

static uint64_t pcg64s_generate(php_random_status *status)
{
	php_random_status_state_pcg64s *s = status->state;

	pcg64s_step(s);
	return php_random_pcg64s_rotr64(s->state);
}

static zend_long pcg64s_range(php_random_status *status, zend_long min, zend_long max)
{
	return common_range(&php_random_algo_pcg64s, status, min, max);
}

static bool pcg64s_serialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_pcg64s *s = status->state;
	uint64_t u;
	zval z;

	u = php_random_uint128_hi(s->state);
	ZVAL_STR(&z, zend_strpprintf(0, "%" PRIu64, u));
	zend_hash_next_index_insert(data, &z);
	
	u = php_random_uint128_lo(s->state);
	ZVAL_STR(&z, zend_strpprintf(0, "%" PRIu64, u));
	zend_hash_next_index_insert(data, &z);

	return true;
}

static bool pcg64s_unserialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_pcg64s *s = status->state;
	uint64_t u[2];
	zval *z;
	uint32_t i;

	for (i = 0; i < 2; i++) {
		z = zend_hash_index_find(data, i);
		if (!z || Z_TYPE_P(z) != IS_STRING) {
			return false;
		}
		u[i] = strtoull(ZSTR_VAL(Z_STR_P(z)), NULL, 10);
	}
	s->state = php_random_uint128_constant(u[0], u[1]);

	return true;
}

const php_random_algo php_random_algo_pcg64s = {
	sizeof(uint64_t),
	sizeof(php_random_status_state_pcg64s),
	pcg64s_seed,
	pcg64s_generate,
	pcg64s_range,
	pcg64s_serialize,
	pcg64s_unserialize
};

static zend_object *php_random_engine_pcg64_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_pcg64_object_handlers, &php_random_algo_pcg64s)->std;
}
/* ---- PCG64 END ---- */

/* ---- SECURE BEGIN ---- */
static uint64_t secure_generate(php_random_status *status)
{
	zend_ulong r = 0;

	if (php_random_bytes_silent(&r, sizeof(zend_ulong)) == FAILURE) {
		status->last_unsafe = true;
	}

	return r;
}

static zend_long secure_range(php_random_status *status, zend_long min, zend_long max)
{
	zend_long result;

	if (php_random_int_silent(min, max, &result) == FAILURE) {
		status->last_unsafe = true;
	}

	return result;
}

const php_random_algo php_random_algo_secure = {
	sizeof(zend_ulong),
	0,
	NULL,
	secure_generate,
	secure_range,
	NULL,
	NULL
};

static zend_object *php_random_engine_secure_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_secure_object_handlers, &php_random_algo_secure)->std;
}
/* ---- SECURE END ---- */

/* ---- USER BEGIN ---- */
static uint64_t user_generate(php_random_status *status)
{
	php_random_status_state_user *s = status->state;
	uint64_t result = 0;
	size_t size;
	zval retval;
	uint32_t i;

	zend_call_known_instance_method_with_0_params(s->generate_method, s->object, &retval);

	/* Store generated size in a state */
	size = Z_STRLEN(retval);

	/* Guard for over 64-bit results */
	if (size > sizeof(uint64_t)) {
		size = sizeof(uint64_t);
	}
	status->last_generated_size = size;

	if (size > 0) {
		/* Endianness safe copy */
		for (i = 0; i < size; i++) {
			result += ((uint64_t) (unsigned char) Z_STRVAL(retval)[i]) << (8 * i);
		}
	} else {
		status->last_unsafe = true;
		return 0;
	}

	zval_ptr_dtor(&retval);

	return result;
}

static zend_long user_range(php_random_status *status, zend_long min, zend_long max)
{
	return common_range(&php_random_algo_user, status, min, max);
}

const php_random_algo php_random_algo_user = {
	0,
	sizeof(php_random_status_state_user),
	NULL,
	user_generate,
	user_range,
	NULL,
	NULL,
};
/* ---- USER END ---- */

/* ---- Randomizer BEGIN ---- */
static zend_object *php_random_randomizer_new(zend_class_entry *ce)
{
	php_random_randomizer *randomizer = zend_object_alloc(sizeof(php_random_randomizer), ce);

	zend_object_std_init(&randomizer->std, ce);
	object_properties_init(&randomizer->std, ce);

	randomizer->std.handlers = &random_randomizer_object_handlers;

	return &randomizer->std;
}

static void randomizer_free_obj(zend_object *object) {
	php_random_randomizer *randomizer = php_random_randomizer_from_obj(object);

	if (randomizer->is_userland_algo && randomizer->status) {
		php_random_status_free(randomizer->status);
	}

	zend_object_std_dtor(&randomizer->std);
}

static void randomizer_common_init(php_random_randomizer *randomizer, zend_object *engine_object) {
	if (engine_object->ce->type == ZEND_INTERNAL_CLASS) {
		/* Internal classes always php_random_engine struct */
		php_random_engine *engine = php_random_engine_from_obj(engine_object);
		
		/* Copy engine pointers */
		randomizer->algo = engine->algo;
		randomizer->status = engine->status;
	} else {
		/* Self allocation */
		randomizer->status = php_random_status_allocate(&php_random_algo_user);
		php_random_status_state_user *state = randomizer->status->state;
		zend_string *mname;
		zend_function *generate_method;

		mname = zend_string_init("generate", sizeof("generate") - 1, 0);
		generate_method = zend_hash_find_ptr(&engine_object->ce->function_table, mname);
		zend_string_release(mname);

		/* Create compatible state */
		state->object = engine_object;
		state->generate_method = generate_method;

		/* Copy common pointers */
		randomizer->algo = &php_random_algo_user;

		/* Mark self-allocated for memory management */
		randomizer->is_userland_algo = true;
	}
}
/* ---- Randomizer END ---- */

/* ---- INTERNAL API BEGIN ---- */
/* {{{ php_random_default_algo */
PHPAPI const php_random_algo *php_random_default_algo(void)
{
	return &php_random_algo_mersennetwister;
}
/* }}} */

/* {{{ php_random_default_status */
PHPAPI php_random_status *php_random_default_status(void)
{
	php_random_status *status = RANDOM_G(mersennetwister);

	if (!status) {
		status = php_random_status_allocate(&php_random_algo_mersennetwister);
		mersennetwister_seed_default(status);
		RANDOM_G(mersennetwister) = status;
	}

	return RANDOM_G(mersennetwister);
}
/* }}} */

/* {{{ php_combined_lcg */
PHPAPI double php_combined_lcg(void)
{
	php_random_status *status = RANDOM_G(combined_lcg);

	if (!status) {
		status = php_random_status_allocate(&php_random_algo_combinedlcg);
		combinedlcg_seed_default(status);
		RANDOM_G(combined_lcg) = status;
	}

	return php_random_algo_combinedlcg.generate(status) * 4.656613e-10;
}
/* }}} */

/* {{{ php_mt_srand */
PHPAPI void php_mt_srand(uint32_t seed)
{
	/* Seed the generator with a simple uint32 */
	mersennetwister_seed(php_random_default_status(), (zend_long) seed);
}
/* }}} */

/* {{{ php_mt_rand */
PHPAPI uint32_t php_mt_rand(void)
{
	return (uint32_t) php_random_algo_mersennetwister.generate(php_random_default_status());
}
/* }}} */

/* {{{ php_mt_rand_range */
PHPAPI zend_long php_mt_rand_range(zend_long min, zend_long max)
{
	return php_random_algo_mersennetwister.range(php_random_default_status(), min, max);
}
/* }}} */

/* {{{ php_mt_rand_common
 * rand() allows min > max, mt_rand does not */
PHPAPI zend_long php_mt_rand_common(zend_long min, zend_long max)
{
	return php_mt_rand_range(min, max);
}
/* }}} */

/* {{{ php_srand */
PHPAPI void php_srand(zend_long seed)
{
	php_mt_srand((uint32_t) seed);
}
/* }}} */

/* {{{ php_rand */
PHPAPI zend_long php_rand(void)
{
	return php_mt_rand();
}
/* }}} */

/* {{{ php_random_bytes */
PHPAPI int php_random_bytes(void *bytes, size_t size, bool should_throw)
{
#ifdef PHP_WIN32
	/* Defer to CryptGenRandom on Windows */
	if (php_win32_get_random_bytes(bytes, size) == FAILURE) {
		if (should_throw) {
			zend_throw_exception(zend_ce_exception, "Could not gather sufficient random data", 0);
		}
		return FAILURE;
	}
#elif HAVE_DECL_ARC4RANDOM_BUF && ((defined(__OpenBSD__) && OpenBSD >= 201405) || (defined(__NetBSD__) && __NetBSD_Version__ >= 700000001) || defined(__APPLE__))
	arc4random_buf(bytes, size);
#else
	size_t read_bytes = 0;
	ssize_t n;
#if (defined(__linux__) && defined(SYS_getrandom)) || (defined(__FreeBSD__) && __FreeBSD_version >= 1200000) || (defined(__DragonFly__) && __DragonFly_version >= 500700) || defined(__sun)
	/* Linux getrandom(2) syscall or FreeBSD/DragonFlyBSD getrandom(2) function*/
	/* Keep reading until we get enough entropy */
	while (read_bytes < size) {
		/* Below, (bytes + read_bytes)  is pointer arithmetic.

		   bytes   read_bytes  size
		     |      |           |
		    [#######=============] (we're going to write over the = region)
		             \\\\\\\\\\\\\
		              amount_to_read

		*/
		size_t amount_to_read = size - read_bytes;
#if defined(__linux__)
		n = syscall(SYS_getrandom, bytes + read_bytes, amount_to_read, 0);
#else
		n = getrandom(bytes + read_bytes, amount_to_read, 0);
#endif

		if (n == -1) {
			if (errno == ENOSYS) {
				/* This can happen if PHP was compiled against a newer kernel where getrandom()
				 * is available, but then runs on an older kernel without getrandom(). If this
				 * happens we simply fall back to reading from /dev/urandom. */
				ZEND_ASSERT(read_bytes == 0);
				break;
			} else if (errno == EINTR || errno == EAGAIN) {
				/* Try again */
				continue;
			} else {
			    /* If the syscall fails, fall back to reading from /dev/urandom */
				break;
			}
		}

#if __has_feature(memory_sanitizer)
		/* MSan does not instrument manual syscall invocations. */
		__msan_unpoison(bytes + read_bytes, n);
#endif
		read_bytes += (size_t) n;
	}
#endif
	if (read_bytes < size) {
		int    fd = RANDOM_G(random_fd);
		struct stat st;

		if (fd < 0) {
#if HAVE_DEV_URANDOM
			fd = open("/dev/urandom", O_RDONLY);
#endif
			if (fd < 0) {
				if (should_throw) {
					zend_throw_exception(zend_ce_exception, "Cannot open source device", 0);
				}
				return FAILURE;
			}
			/* Does the file exist and is it a character device? */
			if (fstat(fd, &st) != 0 ||
# ifdef S_ISNAM
					!(S_ISNAM(st.st_mode) || S_ISCHR(st.st_mode))
# else
					!S_ISCHR(st.st_mode)
# endif
			) {
				close(fd);
				if (should_throw) {
					zend_throw_exception(zend_ce_exception, "Error reading from source device", 0);
				}
				return FAILURE;
			}
			RANDOM_G(random_fd) = fd;
		}

		for (read_bytes = 0; read_bytes < size; read_bytes += (size_t) n) {
			n = read(fd, bytes + read_bytes, size - read_bytes);
			if (n <= 0) {
				break;
			}
		}

		if (read_bytes < size) {
			if (should_throw) {
				zend_throw_exception(zend_ce_exception, "Could not gather sufficient random data", 0);
			}
			return FAILURE;
		}
	}
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ php_random_int */
PHPAPI int php_random_int(zend_long min, zend_long max, zend_long *result, bool should_throw)
{
	zend_ulong umax;
	zend_ulong trial;

	if (min == max) {
		*result = min;
		return SUCCESS;
	}

	umax = (zend_ulong) max - (zend_ulong) min;

	if (php_random_bytes(&trial, sizeof(trial), should_throw) == FAILURE) {
		return FAILURE;
	}

	/* Special case where no modulus is required */
	if (umax == ZEND_ULONG_MAX) {
		*result = (zend_long)trial;
		return SUCCESS;
	}

	/* Increment the max so the range is inclusive of max */
	umax++;

	/* Powers of two are not biased */
	if ((umax & (umax - 1)) != 0) {
		/* Ceiling under which ZEND_LONG_MAX % max == 0 */
		zend_ulong limit = ZEND_ULONG_MAX - (ZEND_ULONG_MAX % umax) - 1;

		/* Discard numbers over the limit to avoid modulo bias */
		while (trial > limit) {
			if (php_random_bytes(&trial, sizeof(trial), should_throw) == FAILURE) {
				return FAILURE;
			}
		}
	}

	*result = (zend_long)((trial % umax) + min);
	return SUCCESS;
}
/* }}} */
/* ---- INTERNAL API END ---- */

/* ---- API: PCG64 BEGIN ---- */
/* {{{ php_random_pcg64s_advance */
PHPAPI void php_random_pcg64s_advance(php_random_status_state_pcg64s *status, uint64_t advance)
{
	pcg64s_advance(status, advance);
}
/* }}} */
/* ---- API: PCG64 END ---- */

/* ---- PHP FUNCTION BEGIN ---- */
/* {{{ Returns a value from the combined linear congruential generator */
PHP_FUNCTION(lcg_value)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_DOUBLE(php_combined_lcg());
}
/* }}} */

/* {{{ Seeds Mersenne Twister random number generator */
PHP_FUNCTION(mt_srand)
{
	zend_long seed = 0;
	zend_long mode = MT_RAND_MT19937;
	php_random_status *status = RANDOM_G(mersennetwister);
	php_random_status_state_mersennetwister *state;

	if (!status) {
		status = php_random_status_allocate(&php_random_algo_mersennetwister);
		mersennetwister_seed_default(status);
		RANDOM_G(mersennetwister) = status;
	}

	state = status->state;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(seed)
		Z_PARAM_LONG(mode)
	ZEND_PARSE_PARAMETERS_END();

	state->mode = mode;

	if (ZEND_NUM_ARGS() == 0) {
		mersennetwister_seed_default(status);
	} else {
		mersennetwister_seed(status, (uint64_t) seed);;
	}
}
/* }}} */

/* {{{ Returns a random number from Mersenne Twister */
PHP_FUNCTION(mt_rand)
{
	zend_long min, max;
	int argc = ZEND_NUM_ARGS();

	if (argc == 0) {
		/* genrand_int31 in mt19937ar.c performs a right shift */
		RETURN_LONG(php_mt_rand() >> 1);
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(min)
		Z_PARAM_LONG(max)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(max < min)) {
		zend_argument_value_error(2, "must be greater than or equal to argument #1 ($min)");
		RETURN_THROWS();
	}

	RETURN_LONG(php_mt_rand_common(min, max));
}
/* }}} */

/* {{{ Returns the maximum value a random number from Mersenne Twister can have */
PHP_FUNCTION(mt_getrandmax)
{
	ZEND_PARSE_PARAMETERS_NONE();

	/*
	 * Melo: it could be 2^^32 but we only use 2^^31 to maintain
	 * compatibility with the previous php_rand
	 */
	RETURN_LONG(PHP_MT_RAND_MAX); /* 2^^31 */
}
/* }}} */

/* {{{ Returns a random number from Mersenne Twister */
PHP_FUNCTION(rand)
{
	zend_long min, max;
	int argc = ZEND_NUM_ARGS();

	if (argc == 0) {
		/* genrand_int31 in mt19937ar.c performs a right shift */
		RETURN_LONG(php_mt_rand() >> 1);
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(min)
		Z_PARAM_LONG(max)
	ZEND_PARSE_PARAMETERS_END();

	if (max < min) {
		RETURN_LONG(php_mt_rand_common(max, min));
	}

	RETURN_LONG(php_mt_rand_common(min, max));
}
/* }}} */

/* {{{ Return an arbitrary length of pseudo-random bytes as binary string */
PHP_FUNCTION(random_bytes)
{
	zend_long size;
	zend_string *bytes;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(size)
	ZEND_PARSE_PARAMETERS_END();

	if (size < 1) {
		zend_argument_value_error(1, "must be greater than 0");
		RETURN_THROWS();
	}

	bytes = zend_string_alloc(size, 0);

	if (php_random_bytes_throw(ZSTR_VAL(bytes), size) == FAILURE) {
		zend_string_release_ex(bytes, 0);
		RETURN_THROWS();
	}

	ZSTR_VAL(bytes)[size] = '\0';

	RETURN_STR(bytes);
}
/* }}} */

/* {{{ Return an arbitrary pseudo-random integer */
PHP_FUNCTION(random_int)
{
	zend_long min, max, result;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(min)
		Z_PARAM_LONG(max)
	ZEND_PARSE_PARAMETERS_END();

	if (min > max) {
		zend_argument_value_error(1, "must be less than or equal to argument #2 ($max)");
		RETURN_THROWS();
	}

	if (php_random_int_throw(min, max, &result) == FAILURE) {
		RETURN_THROWS();
	}

	RETURN_LONG(result);
}
/* }}} */
/* ---- PHP FUNCTION END ---- */

/* ---- PHP METHOD BEGIN ---- */
/* {{{ Random\Engine\CombinedLCG::__construct */
PHP_METHOD(Random_Engine_CombinedLCG, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_status *status = engine->status;
	php_random_status_state_combinedlcg *state = status->state;
	zend_long seed;
	bool seed_is_null = true;
	
	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL;
		Z_PARAM_LONG_OR_NULL(seed, seed_is_null);
	ZEND_PARSE_PARAMETERS_END();

	if (seed_is_null) {
		int i;

		for (i = 0; i < 2; i++) {
			if (php_random_bytes_silent(&state->state[i], sizeof(int32_t)) == FAILURE) {
				zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
				RETURN_THROWS();
			}
		}
	} else {
		engine->algo->seed(status, seed);
	}
}
/* }}} */

/* {{{ Random\Engine\CombinedLCG::generate() */
PHP_METHOD(Random_Engine_CombinedLCG, generate)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	uint64_t generated;
	size_t size;
	zend_string *bytes;
	int i;

	ZEND_PARSE_PARAMETERS_NONE();

	generated = engine->algo->generate(engine->status);
	size = engine->status->last_generated_size;
	if (engine->status->last_unsafe) {
		zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
		RETURN_THROWS();
	}

	bytes = zend_string_alloc(size, false);

	/* Endianness safe copy */
	for (i = 0; i < size; i++) {
		ZSTR_VAL(bytes)[i] = (generated >> (i * 8)) & 0xff;
	}
	ZSTR_VAL(bytes)[size] = '\0';

	RETURN_STR(bytes);
}
/* }}} */

/* {{{ Random\Engine\CombinedLCG::__serialize() */
PHP_METHOD(Random_Engine_CombinedLCG, __serialize)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	zval t;

	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* members */
	ZVAL_ARR(&t, zend_std_get_properties(&engine->std));
	Z_TRY_ADDREF(t);
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &t);

	/* state */
	array_init(&t);
	if (!engine->algo->serialize(engine->status, Z_ARRVAL(t))) {
		zend_throw_exception(NULL, "Engine serialize failed", 0);
		RETURN_THROWS();
	}
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &t);
}
/* }}} */

/* {{{ Random\Engine\CombinedLCG::__unserialize() */
PHP_METHOD(Random_Engine_CombinedLCG, __unserialize)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	HashTable *d;
	zval *t;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(d);
	ZEND_PARSE_PARAMETERS_END();

	/* members */
	t = zend_hash_index_find(d, 0);
	if (!t || Z_TYPE_P(t) != IS_ARRAY) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}
	object_properties_load(&engine->std, Z_ARRVAL_P(t));

	/* state */
	t = zend_hash_index_find(d, 1);
	if (!t || Z_TYPE_P(t) != IS_ARRAY) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}
	if (!engine->algo->unserialize(engine->status, Z_ARRVAL_P(t))) {
		zend_throw_exception(NULL, "Engine unserialize failed", 0);
		RETURN_THROWS();
	}
}
/* }}} */

/* {{{ Random\Engine\CombinedLCG::__debugInfo() */
PHP_METHOD(Random_Engine_CombinedLCG, __debugInfo)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	zval t;

	ZEND_PARSE_PARAMETERS_NONE();

	if (!engine->std.properties) {
		rebuild_object_properties(&engine->std);
	}
	ZVAL_ARR(return_value, zend_array_dup(engine->std.properties));

	if (engine->algo->serialize) {
		array_init(&t);
		if (!engine->algo->serialize(engine->status, Z_ARRVAL(t))) {
			zend_throw_exception(NULL, "Engine serialize failed", 0);
			RETURN_THROWS();
		}
		zend_hash_str_add(Z_ARR_P(return_value), "__states", sizeof("__states") - 1, &t);
	}
}
/* }}} */

/* {{{ Random\Engine\MersenneTwister::__construct() */
PHP_METHOD(Random_Engine_MersenneTwister, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_status_state_mersennetwister *state = engine->status->state;
	zend_long seed, mode = MT_RAND_MT19937;
	bool seed_is_null = true;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL;
		Z_PARAM_LONG_OR_NULL(seed, seed_is_null);
		Z_PARAM_LONG(mode);
	ZEND_PARSE_PARAMETERS_END();

	switch (mode) {
		case MT_RAND_PHP:
			state->mode = MT_RAND_PHP;
			break;
		default:
			state->mode = MT_RAND_MT19937;
	}

	if (seed_is_null) {
		/* MT19937 has a very large state, uses CSPRNG for seeding only */
		if (php_random_bytes_silent(&seed, sizeof(zend_long)) == FAILURE) {
			zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
			RETURN_THROWS();
		}
	}

	engine->algo->seed(engine->status, seed);
}
/* }}} */

/* {{{ Random\Engine\PCG64::__construct */
PHP_METHOD(Random_Engine_PCG64, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_status_state_pcg64s *state = engine->status->state;
	zend_string *str_seed = NULL;
	zend_long int_seed = 0;
	bool seed_is_null = true;
	int i, j;
	uint64_t t[2];

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL;
		Z_PARAM_STR_OR_LONG_OR_NULL(str_seed, int_seed, seed_is_null);
	ZEND_PARSE_PARAMETERS_END();

	if (seed_is_null) {
		if (php_random_bytes_silent(&state->state, sizeof(php_random_uint128_t)) == FAILURE) {
			zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
			RETURN_THROWS();
		}
	} else {
		if (str_seed) {
			/* char (8 bit) * 16 = 128 bits */
			if (ZSTR_LEN(str_seed) == 16) {
				/* Endianness safe copy */
				for (i = 0; i < 2; i++) {
					t[i] = 0;
					for (j = 0; j < 8; j++) {
						t[i] += ((uint64_t) (unsigned char) ZSTR_VAL(str_seed)[(i * 8) + j]) << (j * 8);
					}
				}
				pcg64s_seed_128(engine->status, php_random_uint128_constant(t[0], t[1]));
			} else {
				zend_argument_value_error(1, "state strings must be 16 bytes");
				RETURN_THROWS();
			}
		} else {
			engine->algo->seed(engine->status, int_seed);
		}
	}
}
/* }}} */

/* {{{ Random\Engine\PCG64::jump() */
PHP_METHOD(Random_Engine_PCG64, jump)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_status_state_pcg64s *state = engine->status->state;
	zend_long advance = 0;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(advance);
	ZEND_PARSE_PARAMETERS_END();

	pcg64s_advance(state, advance);
}
/* }}} */

/* {{{ Random\Randomizer::__construct() */
PHP_METHOD(Random_Randomizer, __construct)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_object *engine_object = NULL;
	zval zengine_object;
	zend_string *pname;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(engine_object, random_ce_Random_Engine);
	ZEND_PARSE_PARAMETERS_END();

	/* Create default RNG instance */
	if (!engine_object) {
		engine_object = php_random_engine_secure_new(random_ce_Random_Engine_Secure);

		/* No need self-refcount */
		GC_DELREF(engine_object);
	}

	ZVAL_OBJ(&zengine_object, engine_object);

	/* Write property */
	pname = zend_string_init("engine", sizeof("engine") - 1, 0);
	zend_std_write_property(&randomizer->std, pname, &zengine_object, NULL);
	zend_string_release(pname);

	randomizer_common_init(randomizer, engine_object);
}
/* }}} */

/* {{{ Generate random number in range */
PHP_METHOD(Random_Randomizer, getInt)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	uint64_t result;
	zend_long min, max;
	int argc = ZEND_NUM_ARGS();

	if (argc == 0) {
		result = randomizer->algo->generate(randomizer->status);
		if (randomizer->status->last_generated_size > sizeof(zend_long)) {
			zend_throw_exception(spl_ce_RuntimeException, "Generated value exceeds size of int", 0);
			RETURN_THROWS();
		}
		if (randomizer->status->last_unsafe) {
			zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
			RETURN_THROWS();
		}
		RETURN_LONG((zend_long) (result >> 1));
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(min)
		Z_PARAM_LONG(max)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(max < min)) {
		zend_argument_value_error(2, "must be greater than or equal to argument #1 ($min)");
		RETURN_THROWS();
	}

	result = randomizer->algo->range(randomizer->status, min, max);
	if (randomizer->status->last_unsafe) {
		zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
		RETURN_THROWS();
	}

	RETURN_LONG((zend_long) result);
}
/* }}} */

/* {{{ Generate random bytes string in ordered length */
PHP_METHOD(Random_Randomizer, getBytes)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_string *retval;
	zend_long length;
	uint64_t result;
	size_t total_size = 0, required_size;
	uint32_t i;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(length)
	ZEND_PARSE_PARAMETERS_END();

	if (length < 1) {
		zend_argument_value_error(1, "must be greater than 0");
		RETURN_THROWS();
	}

	retval = zend_string_alloc(length, 0);
	required_size = length;

	while (total_size < required_size) {
		result = randomizer->algo->generate(randomizer->status);
		if (randomizer->status->last_unsafe) {
			zend_string_free(retval);
			zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
			RETURN_THROWS();			
		}
		for (i = 0; i< randomizer->status->last_generated_size; i++) {
			ZSTR_VAL(retval)[total_size++] = (result >> (i * 8) & 0xff);
			if (total_size >= required_size) {
				break;
			}
		}
	}

	ZSTR_VAL(retval)[length] = '\0';
	RETURN_STR(retval);
}
/* }}} */

/* {{{ Shuffling array */
PHP_METHOD(Random_Randomizer, shuffleArray)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zval *array;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY(array)
	ZEND_PARSE_PARAMETERS_END();

	ZVAL_DUP(return_value, array);
	if (php_array_data_shuffle(randomizer->algo, randomizer->status, return_value) == FAILURE) {
		zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
		RETURN_THROWS();
	}
}
/* }}} */

/* {{{ Shuffling string */
PHP_METHOD(Random_Randomizer, shuffleString)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_string *string;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(string)
	ZEND_PARSE_PARAMETERS_END();

	if (ZSTR_LEN(string) < 2) {
		RETURN_STR_COPY(string);
	}

	RETVAL_STRINGL(ZSTR_VAL(string), ZSTR_LEN(string));
	if (php_string_shuffle(randomizer->algo, randomizer->status, Z_STRVAL_P(return_value), (zend_long) Z_STRLEN_P(return_value)) == FAILURE) {
		zend_throw_exception(spl_ce_RuntimeException, "Random number generate failed", 0);
		RETURN_THROWS();
	}
}
/* }}} */

/* {{{ Random\Randomizer::__serialize() */
PHP_METHOD(Random_Randomizer, __serialize)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zval t;

	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);
	ZVAL_ARR(&t, zend_std_get_properties(&randomizer->std));
	Z_TRY_ADDREF(t);
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &t);
}
/* }}} */

/* {{{ Random\Randomizer::__unserialize() */
PHP_METHOD(Random_Randomizer, __unserialize)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	HashTable *d;
	zval *members_zv;
	zval *zengine;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(d);
	ZEND_PARSE_PARAMETERS_END();

	members_zv = zend_hash_index_find(d, 0);
	if (!members_zv || Z_TYPE_P(members_zv) != IS_ARRAY) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}
	object_properties_load(&randomizer->std, Z_ARRVAL_P(members_zv));

	zengine = zend_read_property(randomizer->std.ce, &randomizer->std, "engine", sizeof("engine") - 1, 0, NULL);
	if (Z_TYPE_P(zengine) != IS_OBJECT || !instanceof_function(Z_OBJCE_P(zengine), random_ce_Random_Engine)) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}

	randomizer_common_init(randomizer, Z_OBJ_P(zengine));
}
/* }}} */

/* ---- PHP METHOD END ---- */

/* ---- PHP MODULE BEGIN ---- */
/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(random)
{
	/* Random\Engine */
	random_ce_Random_Engine = register_class_Random_Engine();

	/* Random\CryptoSafeEngine */
	random_ce_Random_CryptoSafeEngine = register_class_Random_CryptoSafeEngine(random_ce_Random_Engine);

	/* Random\SeedableEngine */
	random_ce_Random_SeedableEngine = register_class_Random_SeedableEngine(random_ce_Random_Engine);

	/* Random\SerializableEngine */
	random_ce_Random_SerializableEngine = register_class_Random_SerializableEngine(random_ce_Random_Engine);

	/* Random\Engine\CombinedLCG */
	random_ce_Random_Engine_CombinedLCG = register_class_Random_Engine_CombinedLCG(random_ce_Random_SeedableEngine, random_ce_Random_SerializableEngine);
	random_ce_Random_Engine_CombinedLCG->create_object = php_random_engine_combinedlcg_new;
	memcpy(&random_engine_combinedlcg_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_combinedlcg_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_combinedlcg_object_handlers.free_obj = engine_common_free_obj;
	random_engine_combinedlcg_object_handlers.clone_obj = engine_common_clone_obj;

	/* Random\Engine\PCG64 */
	random_ce_Random_Engine_PCG64 = register_class_Random_Engine_PCG64(random_ce_Random_SeedableEngine, random_ce_Random_SerializableEngine);
	random_ce_Random_Engine_PCG64->create_object = php_random_engine_pcg64_new;
	memcpy(&random_engine_pcg64_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_pcg64_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_pcg64_object_handlers.free_obj = engine_common_free_obj;
	random_engine_pcg64_object_handlers.clone_obj = engine_common_clone_obj;

	/* Random\Engine\MersenneTwister */
	random_ce_Random_Engine_MersenneTwister = register_class_Random_Engine_MersenneTwister(random_ce_Random_SeedableEngine, random_ce_Random_SerializableEngine);
	random_ce_Random_Engine_MersenneTwister->create_object = php_random_engine_mersennetwister_new;
	memcpy(&random_engine_mersennetwister_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_mersennetwister_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_mersennetwister_object_handlers.free_obj = engine_common_free_obj;
	random_engine_mersennetwister_object_handlers.clone_obj = engine_common_clone_obj;

	/* Random\Engine\Secure */
	random_ce_Random_Engine_Secure = register_class_Random_Engine_Secure(random_ce_Random_CryptoSafeEngine);
	random_ce_Random_Engine_Secure->create_object = php_random_engine_secure_new;
	memcpy(&random_engine_secure_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_secure_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_secure_object_handlers.free_obj = engine_common_free_obj;
	random_engine_secure_object_handlers.clone_obj = NULL;

	/* Random\Randomizer */
	random_ce_Random_Randomizer = register_class_Random_Randomizer();
	random_ce_Random_Randomizer->create_object = php_random_randomizer_new;
	memcpy(&random_randomizer_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_randomizer_object_handlers.offset = XtOffsetOf(php_random_randomizer, std);
	random_randomizer_object_handlers.free_obj = randomizer_free_obj;
	random_randomizer_object_handlers.clone_obj = NULL;

	REGISTER_LONG_CONSTANT("MT_RAND_MT19937", MT_RAND_MT19937, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MT_RAND_PHP",     MT_RAND_PHP, CONST_CS | CONST_PERSISTENT);

	RANDOM_G(random_fd) = -1;
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(random)
{
	if (RANDOM_G(random_fd) > 0) {
		close(RANDOM_G(random_fd));
		RANDOM_G(random_fd) = -1;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(random)
{
	if (RANDOM_G(combined_lcg)) {
		php_random_status_free(RANDOM_G(combined_lcg));
	}

	if (RANDOM_G(mersennetwister)) {
		php_random_status_free(RANDOM_G(mersennetwister));
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(random)
{
#if defined(ZTS) && defined(COMPILE_DL_RANDOM)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	RANDOM_G(combined_lcg) = NULL;
	RANDOM_G(mersennetwister) = NULL;

	return SUCCESS;
}
/* }}} */

/* {{{ random_module_entry */
zend_module_entry random_module_entry = {
	STANDARD_MODULE_HEADER,
	"random",					/* Extension name */
	ext_functions,				/* zend_function_entry */
	PHP_MINIT(random),			/* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(random),		/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(random),			/* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(random),		/* PHP_RSHUTDOWN - Request shutdown */
	NULL,						/* PHP_MINFO - Module info */
	PHP_VERSION,				/* Version */
	PHP_MODULE_GLOBALS(random),	/* ZTS Module globals */
	NULL,						/* PHP_GINIT - Global initialization */
	NULL,						/* PHP_GSHUTDOWN - Global shutdown */
	NULL,						/* Post deactivate */
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */
/* ---- PHP MODULE END ---- */

#ifdef COMPILE_DL_RANDOM
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(random)
#endif
