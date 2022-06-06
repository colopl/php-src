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

/*
 * combinedLCG() returns a pseudo random number in the range of (0, 1).
 * The function combines two CGs with periods of
 * 2^31 - 85 and 2^31 - 249. The period of this function
 * is equal to the product of both primes.
 */
#define MODMULT(a, b, c, m, s) q = s/a;s=b*(s-a*q)-c*q;if(s<0)s+=m

#define N             MT_N                 /* length of state vector */
#define M             (397)                /* a period parameter */
#define hiBit(u)      ((u) & 0x80000000U)  /* mask all but highest   bit of u */
#define loBit(u)      ((u) & 0x00000001U)  /* mask all but lowest    bit of u */
#define loBits(u)     ((u) & 0x7FFFFFFFU)  /* mask     the highest   bit of u */
#define mixBits(u, v) (hiBit(u)|loBits(v)) /* move hi bit of u to hi bit of v */

#define twist(m,u,v)  (m ^ (mixBits(u,v)>>1) ^ ((uint32_t)(-(int32_t)(loBit(v))) & 0x9908b0dfU))
#define twist_php(m,u,v)  (m ^ (mixBits(u,v)>>1) ^ ((uint32_t)(-(int32_t)(loBit(u))) & 0x9908b0dfU))

ZEND_DECLARE_MODULE_GLOBALS(random)

PHPAPI zend_class_entry *random_ce_Random_Engine;
PHPAPI zend_class_entry *random_ce_Random_Engine_CombinedLCG;
PHPAPI zend_class_entry *random_ce_Random_Engine_MersenneTwister;
PHPAPI zend_class_entry *random_ce_Random_Engine_Secure;
PHPAPI zend_class_entry *random_ce_Random_Engine_XorShift128Plus;
PHPAPI zend_class_entry *random_ce_Random_Engine_Xoshiro256StarStar;
PHPAPI zend_class_entry *random_ce_Random_Randomizer;

static zend_object_handlers random_engine_combinedlcg_object_handlers;
static zend_object_handlers random_engine_mersennetwister_object_handlers;
static zend_object_handlers random_engine_secure_object_handlers;
static zend_object_handlers random_engine_xorshift128plus_object_handlers;
static zend_object_handlers random_engine_xoshiro256starstar_object_handlers;
static zend_object_handlers random_randomizer_object_handlers;

static uint32_t rand_range32(const php_random_engine_algo *algo, void *state, uint32_t umax) {
	uint32_t result, limit;
	size_t generated_size;

	RANDOM_ENGINE_GENERATE_SIZE(algo, state, result, generated_size);
	while (generated_size < sizeof(uint32_t)) {
		uint32_t ret;
		size_t generate_size;

		RANDOM_ENGINE_GENERATE_SIZE(algo, state, ret, generate_size);
		result = (result << generate_size) | ret;

		generated_size += generate_size;
	}

	/* Special case where no modulus is required */
	if (UNEXPECTED(umax == UINT32_MAX)) {
		return result;
	}

	/* Increment the max so the range is inclusive of max */
	umax++;

	/* Powers of two are not biased */
	if ((umax & (umax - 1)) == 0) {
		return result & (umax - 1);
	}

	/* Ceiling under which UINT32_MAX % max == 0 */
	limit = UINT32_MAX - (UINT32_MAX % umax) - 1;

	/* Discard numbers over the limit to avoid modulo bias */
	while (UNEXPECTED(result > limit)) {
		RANDOM_ENGINE_GENERATE_SIZE(algo, state, result, generated_size);
		while (generated_size < sizeof(uint32_t)) {
			uint32_t ret;
			size_t generate_size;

			RANDOM_ENGINE_GENERATE_SIZE(algo, state, ret, generate_size);

			result = (result << generate_size) | ret;

			generated_size += generate_size;
		}
	}

	return result % umax;
}

#if ZEND_ULONG_MAX > UINT32_MAX
static uint64_t rand_range64(const php_random_engine_algo *algo, void *state, uint64_t umax) {
	uint64_t result, limit;
	size_t generated_size;

	RANDOM_ENGINE_GENERATE_SIZE(algo, state, result, generated_size);

	while (generated_size < sizeof(uint64_t)) {
		uint64_t ret;
		size_t generate_size;

		RANDOM_ENGINE_GENERATE_SIZE(algo, state, ret, generate_size);

		result = (result << generate_size) | ret;

		generated_size += generate_size;
	}

	/* Special case where no modulus is required */
	if (UNEXPECTED(umax == UINT64_MAX)) {
		return result;
	}

	/* Increment the max so the range is inclusive of max */
	umax++;

	/* Powers of two are not biased */
	if ((umax & (umax - 1)) == 0) {
		return result & (umax - 1);
	}

	/* Ceiling under which UINT64_MAX % max == 0 */
	limit = UINT64_MAX - (UINT64_MAX % umax) - 1;

	/* Discard numbers over the limit to avoid modulo bias */
	while (UNEXPECTED(result > limit)) {
		RANDOM_ENGINE_GENERATE_SIZE(algo, state, result, generated_size);

		while (generated_size < sizeof(uint64_t)) {
			uint64_t ret;
			size_t generate_size;

			RANDOM_ENGINE_GENERATE_SIZE(algo, state, ret, generate_size);

			result = (result << generate_size) | ret;

			generated_size += generate_size;
		}
	}

	return result % umax;
}
#endif

static inline uint64_t splitmix64(uint64_t *seed) {
	uint64_t r;

	r = (*seed += UINT64_C(0x9e3779b97f4a7c15));
	r = (r ^ (r >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	r = (r ^ (r >> 27)) * UINT64_C(0x94d049bb133111eb);
	return (r ^ (r >> 31));
}

static inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

static inline zend_object *php_random_engine_common_init(zend_class_entry *ce, const php_random_engine_algo *algo, const zend_object_handlers *handlers) {
	php_random_engine *engine = zend_object_alloc(sizeof(php_random_engine), ce);

	zend_object_std_init(&engine->std, ce);
	object_properties_init(&engine->std, ce);

	engine->algo = algo;
	if (engine->algo->state_size > 0) {
		engine->state = emalloc(engine->algo->state_size);
	}
	engine->std.handlers = handlers;

	return &engine->std;
}

static void php_random_engine_common_free_obj(zend_object *object) {
	php_random_engine *engine = php_random_engine_from_obj(object);

	if (engine->state) {
		efree(engine->state);
	}

	zend_object_std_dtor(object);
}

static zend_object *php_random_engine_common_clone_obj(zend_object *old_object) {
	php_random_engine *old_engine = php_random_engine_from_obj(old_object);
	php_random_engine *new_engine = php_random_engine_from_obj(old_object->ce->create_object(old_object->ce));

	zend_objects_clone_members(&new_engine->std, &old_engine->std);

	new_engine->algo = old_engine->algo;
	if (old_engine->state) {
		memcpy(new_engine->state, old_engine->state, old_engine->algo->state_size);
	}

	return &new_engine->std;
}

/* CombinedLCG begin */

static inline size_t combinedlcg_dynamic_generate_size(void *state) {
	return sizeof(uint32_t);
}

static uint64_t combinedlcg_generate(void *state) {
	int32_t q, z;
	php_random_engine_state_combinedlcg *s = (php_random_engine_state_combinedlcg *) state;

	MODMULT(53668, 40014, 12211, 2147483563L, s->s[0]);
	MODMULT(52774, 40692, 3791, 2147483399L, s->s[1]);

	z = s->s[0] - s->s[1];
	if (z < 1) {
		z += 2147483562;
	}

	return (uint64_t) z;
}

static void combinedlcg_seed(void *state, const uint64_t seed) {
	php_random_engine_state_combinedlcg *s = (php_random_engine_state_combinedlcg *) state;

	s->s[0] = seed & 0xffffffffU;
	s->s[1] = seed >> 32;

	/* Seed only once */
	s->seeded = true;
}

static void combinedlcg_seed_default(php_random_engine_state_combinedlcg *state) {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == 0) {
		state->s[0] = tv.tv_usec ^ (tv.tv_usec << 11);
	} else {
		state->s[0] = 1;
	}

#ifdef ZTS
	state->s[1] = (zend_long) tsrm_thread_id();
#else
	state->s[1] = (zend_long) getpid();
#endif

	/* Add entropy to s2 by calling gettimeofday() again */
	if (gettimeofday(&tv, NULL) == 0) {
		state->s[1] ^= (tv.tv_usec << 11);
	}

	/* Seed only once */
	state->seeded = true;
}

static int combinedlcg_serialize(void *state, HashTable *data) {
	php_random_engine_state_combinedlcg *s = (php_random_engine_state_combinedlcg *)state;
	zval tmp;
	int i;

	for (i = 0; i < 2; i++) {
		ZVAL_LONG(&tmp, s->s[i]);
		zend_hash_next_index_insert(data, &tmp);
	}
	ZVAL_LONG(&tmp, s->seeded);
	zend_hash_next_index_insert(data, &tmp);

	return SUCCESS;
}

static int combinedlcg_unserialize(void *state, HashTable *data) {
	php_random_engine_state_combinedlcg *s = (php_random_engine_state_combinedlcg *)state;
	zval *tmp;
	int i;

	for (i = 0; i < 2; i++) {
		tmp = zend_hash_index_find(data, i);
		if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
			return FAILURE;
		}
		s->s[i] = Z_LVAL_P(tmp);
	}
	tmp = zend_hash_index_find(data, 2);
	if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
		return FAILURE;
	}
	s->seeded = Z_LVAL_P(tmp);

	return SUCCESS;
}

static zend_object *php_random_engine_combinedlcg_new(zend_class_entry *ce) {
	return php_random_engine_common_init(ce, &php_random_engine_algo_combinedlcg, &random_engine_combinedlcg_object_handlers);
}

const php_random_engine_algo php_random_engine_algo_combinedlcg = {
	sizeof(uint32_t),
	combinedlcg_dynamic_generate_size,
	sizeof(php_random_engine_state_combinedlcg),
	combinedlcg_generate,
	combinedlcg_seed,
	combinedlcg_serialize,
	combinedlcg_unserialize
};

/* CombinedLCG end */

/* MersenneTwister begin */

static inline size_t mersennetwister_dynamic_generate_size(void *state) {
	return sizeof(uint32_t);
}

static inline void mersennetwister_reload(php_random_engine_state_mersennetwister *state) {
	uint32_t *p = state->s;
	int i;

	if (state->mode == MT_RAND_MT19937) {
		for (i = N - M; i--; ++p) {
			*p = twist(p[M], p[0], p[1]);
		}
		for (i = M; --i; ++p) {
			*p = twist(p[M-N], p[0], p[1]);
		}
		*p = twist(p[M-N], p[0], state->s[0]);
	} else {
		for (i = N - M; i--; ++p) {
			*p = twist_php(p[M], p[0], p[1]);
		}
		for (i = M; --i; ++p) {
			*p = twist_php(p[M-N], p[0], p[1]);
		}
		*p = twist_php(p[M-N], p[0], state->s[0]);
	}

	state->cnt = 0;
}

static uint64_t mersennetwister_generate(void *state) {
	php_random_engine_state_mersennetwister *s = (php_random_engine_state_mersennetwister *)state;
	uint32_t s1;

	if (!s->seeded) {
		zend_long bytes;
		if (php_random_bytes_silent(&bytes, sizeof(zend_long)) == FAILURE) {
			bytes = GENERATE_SEED();
		}
		php_mt_srand(bytes);
	}


	if (s->cnt >= MT_N) {
		mersennetwister_reload(s);
	}

	s1 = s->s[s->cnt++];
	s1 ^= (s1 >> 11);
	s1 ^= (s1 << 7) & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;
	return (uint64_t) (s1 ^ (s1 >> 18));
}

static void mersennetwister_seed(void *state, const uint64_t seed) {
	/* Initialize generator state with seed
	   See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	   In previous versions, most significant bits (MSBs) of the seed affect
	   only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto. */
	php_random_engine_state_mersennetwister *s = (php_random_engine_state_mersennetwister *)state;

	s->s[0] = seed & 0xffffffffU;
	for (s->cnt = 1; s->cnt < MT_N; s->cnt++) {
		s->s[s->cnt] = (1812433253U * (s->s[s->cnt - 1] ^ (s->s[s->cnt - 1] >> 30)) + s->cnt) & 0xffffffffU;
	}
	mersennetwister_reload(s);

	/* Seed only once */
	s->seeded = true;
}

static int mersennetwister_serialize(void *state, HashTable *data) {
	php_random_engine_state_mersennetwister *s = (php_random_engine_state_mersennetwister *)state;
	zval tmp;
	int i;

	for (i = 0; i< MT_N; i++) {
		ZVAL_LONG(&tmp, s->s[i]);
		zend_hash_next_index_insert(data, &tmp);
	}
	ZVAL_LONG(&tmp, s->cnt);
	zend_hash_next_index_insert(data, &tmp);
	ZVAL_LONG(&tmp, s->seeded);
	zend_hash_next_index_insert(data, &tmp);

	return SUCCESS;
}

static int mersennetwister_unserialize(void *state, HashTable *data) {
	php_random_engine_state_mersennetwister *s = (php_random_engine_state_mersennetwister *)state;
	zval *tmp;
	int i;

	for (i = 0; i < MT_N; i++) {
		tmp = zend_hash_index_find(data, i);
		if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
			return FAILURE;
		}

		s->s[i] = Z_LVAL_P(tmp);
	}
	tmp = zend_hash_index_find(data, MT_N); /* cnt */
	if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
		return FAILURE;
	}
	s->cnt = Z_LVAL_P(tmp);
	tmp = zend_hash_index_find(data, MT_N + 1); /* seeded */
	if (!tmp || Z_TYPE_P(tmp) != IS_LONG) {
		return FAILURE;
	}
	s->seeded = Z_LVAL_P(tmp);

	return SUCCESS;
}

static zend_object *php_random_engine_mersennetwister_new(zend_class_entry *ce) {
	return php_random_engine_common_init(ce, &php_random_engine_algo_mersennetwister, &random_engine_mersennetwister_object_handlers);
}

const php_random_engine_algo php_random_engine_algo_mersennetwister = {
	sizeof(uint32_t),
	mersennetwister_dynamic_generate_size,
	sizeof(php_random_engine_state_mersennetwister),
	mersennetwister_generate,
	mersennetwister_seed,
	mersennetwister_serialize,
	mersennetwister_unserialize
};

/* MersenneTwister end */

/* Secure begin */

static inline size_t secure_dynamic_generate_size(void *state) {
	return sizeof(uint64_t);
}

static uint64_t secure_generate(void *state) {
	uint64_t ret;

	php_random_bytes_silent(&ret, sizeof(uint64_t));

	return ret;
}

static zend_object *php_random_engine_secure_new(zend_class_entry *ce) {
	return php_random_engine_common_init(ce, &php_random_engine_algo_secure, &random_engine_secure_object_handlers);
}

const php_random_engine_algo php_random_engine_algo_secure = {
	sizeof(uint64_t),
	secure_dynamic_generate_size,
	0,
	secure_generate,
	NULL,
	NULL,
	NULL
};

/* Secure end */

/* User begin */

static inline size_t user_dynamic_generate_size(void *state) {
	php_random_engine_state_user *s = (php_random_engine_state_user *) state;

	return s->last_generate_size;
}

static uint64_t user_generate(void *state) {
	php_random_engine_state_user *s = (php_random_engine_state_user *) state;
	uint64_t result = 0;
	size_t size;
	zval retval;
	int i;

	zend_call_known_instance_method_with_0_params(s->generate_method, s->object, &retval);

	/* Store generated size in a state */
	size = Z_STRLEN(retval);

	/* Guard for over 64-bit results */
	if (size > sizeof(uint64_t)) {
		size = sizeof(uint64_t);
	}
	s->last_generate_size = size;

	if (size > 0) {
		/* Endianness safe copy */
		for (i = 0; i < size; i++) {
			result += ((uint64_t) (unsigned char) Z_STRVAL(retval)[i]) << (8 * i);
		}
	} else {
		result = 0;
	}

	zval_ptr_dtor(&retval);

	return result;
}

const php_random_engine_algo php_random_engine_algo_user = {
	0,										/* does not support static generate size */
	user_dynamic_generate_size,				/* always use dynamic_generate_size */
	sizeof(php_random_engine_state_user),
	user_generate,
	NULL,
	NULL,
	NULL
};

/* User end */

/* XorShift128Plus begin */

static inline size_t xorshift128plus_dynamic_generate_size(void *state) {
	return sizeof(uint64_t);
}

static uint64_t xorshift128plus_generate(void *state) {
	php_random_engine_state_xorshift128plus *s = (php_random_engine_state_xorshift128plus *)state;
	uint64_t s0, s1, r;

	s1 = s->s[0];
	s0 = s->s[1];
	r = s0 + s1;
	s->s[0] = s0;
	s1 ^= s1 << 23;
	s->s[1] = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5);

	return r;
}

static void xorshift128plus_seed(void *state, const uint64_t seed) {
	php_random_engine_state_xorshift128plus *s = (php_random_engine_state_xorshift128plus *) state;
	uint64_t sd = seed;

	s->s[0] = splitmix64(&sd);
	s->s[1] = splitmix64(&sd);
}

static int xorshift128plus_serialize(void *state, HashTable *data) {
	php_random_engine_state_xorshift128plus *s = (php_random_engine_state_xorshift128plus *) state;
	zval tmp;
	int i;

	for (i = 0; i < 2; i++) {
		ZVAL_STR(&tmp, zend_strpprintf(0, "%" PRIu64, s->s[i]));
		zend_hash_next_index_insert(data, &tmp);
	}

	return SUCCESS;
}

static int xorshift128plus_unserialize(void *state, HashTable *data) {
	php_random_engine_state_xorshift128plus *s = (php_random_engine_state_xorshift128plus *) state;
	zval *tmp;
	int i;

	for (i = 0; i < 2; i++) {
		tmp = zend_hash_index_find(data, i);
		if (!tmp || Z_TYPE_P(tmp) != IS_STRING) {
			return FAILURE;
		}

		s->s[i] = strtoull(ZSTR_VAL(Z_STR_P(tmp)), NULL, 10);
	}

	return SUCCESS;
}

static zend_object *php_random_engine_xorshift128plus_new(zend_class_entry *ce) {
	return php_random_engine_common_init(ce, &php_random_engine_algo_xorshift128plus, &random_engine_xorshift128plus_object_handlers);
}

const php_random_engine_algo php_random_engine_algo_xorshift128plus = {
	sizeof(uint64_t),
	xorshift128plus_dynamic_generate_size,
	sizeof(php_random_engine_state_xorshift128plus),
	xorshift128plus_generate,
	xorshift128plus_seed,
	xorshift128plus_serialize,
	xorshift128plus_unserialize
};

/* XorShift128Plus end */

/* Xoshiro256StarStar start */

static inline size_t xoshiro256starstar_dynamic_generate_size(void *state) {
	return sizeof(uint64_t);
}

static uint64_t xoshiro256starstar_generate(void *state) {
	php_random_engine_state_xoshiro256starstar *s = (php_random_engine_state_xoshiro256starstar *) state;
	const uint64_t result = rotl(s->s[1] * 5, 7) * 9;
	const uint64_t t = s->s[1] << 17;

	s->s[2] ^= s->s[0];
	s->s[3] ^= s->s[1];
	s->s[1] ^= s->s[2];
	s->s[0] ^= s->s[3];

	s->s[2] ^= t;

	s->s[3] = rotl(s->s[3], 45);

	return result;
}

static void xoshiro256starstar_seed(void *state, const uint64_t seed) {
	php_random_engine_state_xoshiro256starstar *s = (php_random_engine_state_xoshiro256starstar *) state;
	uint64_t sd = seed;

	s->s[0] = splitmix64(&sd);
	s->s[1] = splitmix64(&sd);
	s->s[2] = splitmix64(&sd);
	s->s[3] = splitmix64(&sd);
}

static int xoshiro256starstar_serialize(void *state, HashTable *data) {
	php_random_engine_state_xoshiro256starstar *s = (php_random_engine_state_xoshiro256starstar *) state;
	zval tmp;
	int i;

	for (i = 0; i < 4; i++) {
		ZVAL_STR(&tmp, zend_strpprintf(0, "%" PRIu64, s->s[i]));
		zend_hash_next_index_insert(data, &tmp);
	}

	return SUCCESS;
}

static int xoshiro256starstar_unserialize(void *state, HashTable *data) {
	php_random_engine_state_xoshiro256starstar *s = (php_random_engine_state_xoshiro256starstar *) state;
	zval *tmp;
	int i;

	for (i = 0; i < 4; i++) {
		tmp = zend_hash_index_find(data, i);
		if (!tmp || Z_TYPE_P(tmp) != IS_STRING) {
			return FAILURE;
		}

		s->s[i] = strtoull(ZSTR_VAL(Z_STR_P(tmp)), NULL, 10);
	}

	return SUCCESS;
}

static zend_object *php_random_engine_xoshiro256starstar_new(zend_class_entry *ce) {
	return php_random_engine_common_init(ce, &php_random_engine_algo_xoshiro256starstar, &random_engine_xoshiro256starstar_object_handlers);
}

const php_random_engine_algo php_random_engine_algo_xoshiro256starstar = {
	sizeof(uint64_t),
	xoshiro256starstar_dynamic_generate_size,
	sizeof(php_random_engine_state_xoshiro256starstar),
	xoshiro256starstar_generate,
	xoshiro256starstar_seed,
	xoshiro256starstar_serialize,
	xoshiro256starstar_unserialize
};

/* Xoshiro256StarStar end */

static zend_object *php_random_randomizer_new(zend_class_entry *ce) {
	php_random_randomizer *randomizer = zend_object_alloc(sizeof(php_random_randomizer), ce);

	zend_object_std_init(&randomizer->std, ce);
	object_properties_init(&randomizer->std, ce);

	randomizer->std.handlers = &random_randomizer_object_handlers;

	return &randomizer->std;
}

static void php_random_randomizer_free_obj(zend_object *object) {
	php_random_randomizer *randomizer = php_random_randomizer_from_obj(object);

	if (randomizer->self_allocate && randomizer->state) {
		efree(randomizer->state);
	}

	zend_object_std_dtor(&randomizer->std);
}

static void php_random_randomizer_common_init(php_random_randomizer *randomizer, zend_object *engine_object) {
	if (engine_object->ce->type == ZEND_INTERNAL_CLASS) {
		/* Internal classes always php_random_engine struct */
		php_random_engine *engine = php_random_engine_from_obj(engine_object);
		
		/* Copy engine pointers */
		randomizer->algo = engine->algo;
		randomizer->state = engine->state;

		/* Don't self allocate */
		randomizer->self_allocate = 0;
	} else {
		/* Self allocation */
		php_random_engine_state_user *state = emalloc(sizeof(php_random_engine_state_user));
		zend_string *mname;
		zend_function *generate_method;

		mname = zend_string_init("generate", sizeof("generate") - 1, 0);
		generate_method = zend_hash_find_ptr(&engine_object->ce->function_table, mname);
		zend_string_release(mname);

		/* Create compatible state */
		state->object = engine_object;
		state->generate_method = generate_method;

		/* Copy common pointers */
		randomizer->algo = &php_random_engine_algo_user;
		randomizer->state = state;

		/* Marks self-allocated for Memory management */
		randomizer->self_allocate = 1;
	}
}

/* {{{ php_combined_lcg */
PHPAPI double php_combined_lcg(void)
{
	if (!RANDOM_G(clcg).seeded) {
		combinedlcg_seed_default(&RANDOM_G(clcg));
	}

	return php_random_engine_algo_combinedlcg.generate(&RANDOM_G(clcg)) * 4.656613e-10;
}
/* }}} */

/* {{{ php_mt_srand */
PHPAPI void php_mt_srand(uint32_t seed)
{
	/* Seed the generator with a simple uint32 */
	mersennetwister_seed(&RANDOM_G(mt), (zend_long) seed);
}
/* }}} */

/* {{{ php_mt_rand */
PHPAPI uint32_t php_mt_rand(void)
{
	return php_random_engine_get_default_algo()->generate(php_random_engine_get_default_state());
}
/* }}} */

/* {{{ php_mt_rand_range */
PHPAPI zend_long php_mt_rand_range(zend_long min, zend_long max)
{
	return php_random_engine_range(
		php_random_engine_get_default_algo(),
		php_random_engine_get_default_state(),
		min,
		max
	);
}
/* }}} */

/* {{{ php_mt_rand_common
 * rand() allows min > max, mt_rand does not */
PHPAPI zend_long php_mt_rand_common(zend_long min, zend_long max)
{
	int64_t n;

	if (RANDOM_G(mt).mode == MT_RAND_MT19937) {
		return php_mt_rand_range(min, max);
	}

	/* Legacy mode deliberately not inside php_mt_rand_range()
	 * to prevent other functions being affected */
	n = (int64_t)php_mt_rand() >> 1;
	RAND_RANGE_BADSCALING(n, min, max, PHP_MT_RAND_MAX);

	return n;
}
/* }}} */

/* {{{ php_srand */
PHPAPI void php_srand(zend_long seed)
{
	php_mt_srand(seed);
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

/* {{{ php_random_engine_get_default_algo */
PHPAPI const php_random_engine_algo *php_random_engine_get_default_algo(void)
{
	return &php_random_engine_algo_mersennetwister;
}
/* }}} */

/* {{{ php_random_engine_get_default_state */
PHPAPI void *php_random_engine_get_default_state(void)
{
	return &RANDOM_G(mt);
}
/* }}} */

/* {{{ php_random_engine_range */
PHPAPI zend_long php_random_engine_range(const php_random_engine_algo *algo, void *state, zend_long min, zend_long max)
{
	zend_ulong umax = max - min;

#if ZEND_ULONG_MAX > UINT32_MAX
	/* user-land OR 64-bit RNG OR umax over 32-bit */
	if (algo->static_generate_size == 0 || algo->static_generate_size > sizeof(uint32_t) || umax > UINT32_MAX) {
		return (zend_long) rand_range64(algo, state, umax) + min;
	}
#endif

	return (zend_long) (rand_range32(algo, state, umax) + min);
}
/* }}} */

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

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(seed)
		Z_PARAM_LONG(mode)
	ZEND_PARSE_PARAMETERS_END();

	if (ZEND_NUM_ARGS() == 0) {
		if (php_random_bytes_silent(&seed, sizeof(zend_long)) == FAILURE) {
			seed = GENERATE_SEED();
		}
	}

	switch (mode) {
		case MT_RAND_PHP:
			RANDOM_G(mt).mode = MT_RAND_PHP;
			break;
		default:
			RANDOM_G(mt).mode = MT_RAND_MT19937;
	}

	php_mt_srand(seed);
}
/* }}} */

/* {{{ Returns a random number from Mersenne Twister */
PHP_FUNCTION(mt_rand)
{
	zend_long min, max;
	int argc = ZEND_NUM_ARGS();

	if (argc == 0) {
		// genrand_int31 in mt19937ar.c performs a right shift
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
		// genrand_int31 in mt19937ar.c performs a right shift
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

/* {{{ Construct object */
PHP_METHOD(Random_Engine_CombinedLCG, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	zend_long seed;
	
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(seed)
	ZEND_PARSE_PARAMETERS_END();

	engine->algo->seed(engine->state, seed);
}
/* }}} */

/* {{{ Generate a random number */
PHP_METHOD(Random_Engine_CombinedLCG, generate)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	size_t size;
	uint64_t generated;
	zend_string *bytes;
	int i;

	ZEND_PARSE_PARAMETERS_NONE();

	RANDOM_ENGINE_GENERATE_SIZE(engine->algo, engine->state, generated, size);

	bytes = zend_string_alloc(size, 0);

	/* Endianness safe copy */
	for (i = 0; i < size; i++) {
		ZSTR_VAL(bytes)[i] = (generated >> (i * 8)) & 0xff;
	}
	ZSTR_VAL(bytes)[size] = '\0';

	RETURN_STR(bytes);
}
/* }}} */

/* {{{ Serialize object */
PHP_METHOD(Random_Engine_CombinedLCG, __serialize)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	zval tmp;

	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);

	/* members */
	ZVAL_ARR(&tmp, zend_std_get_properties(&engine->std));
	Z_TRY_ADDREF(tmp);
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &tmp);

	/* state */
	array_init(&tmp);
	if (engine->algo->serialize(engine->state, Z_ARRVAL(tmp)) == FAILURE) {
		zend_throw_exception(NULL, "Engine serialize failed", 0);
		RETURN_THROWS();
	}
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &tmp);
}
/* }}} */

/* {{{ Unserialize object */
PHP_METHOD(Random_Engine_CombinedLCG, __unserialize)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	HashTable *data;
	zval *tmp;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(data);
	ZEND_PARSE_PARAMETERS_END();

	/* members */
	tmp = zend_hash_index_find(data, 0);
	if (!tmp || Z_TYPE_P(tmp) != IS_ARRAY) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}
	object_properties_load(&engine->std, Z_ARRVAL_P(tmp));

	/* state */
	tmp = zend_hash_index_find(data, 1);
	if (!tmp || Z_TYPE_P(tmp) != IS_ARRAY) {
		zend_throw_exception(NULL, "Incomplete or ill-formed serialization data", 0);
		RETURN_THROWS();
	}
	if (engine->algo->unserialize(engine->state, Z_ARRVAL_P(tmp)) == FAILURE) {
		zend_throw_exception(NULL, "Engine unserialize failed", 0);
		RETURN_THROWS();
	}
}
/* }}} */

/* {{{ DebugInfo */
PHP_METHOD(Random_Engine_CombinedLCG, __debugInfo)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	zval tmp;

	ZEND_PARSE_PARAMETERS_NONE();

	if (!engine->std.properties) {
		rebuild_object_properties(&engine->std);
	}
	ZVAL_ARR(return_value, zend_array_dup(engine->std.properties));

	if (engine->algo->serialize) {
		array_init(&tmp);
		if (engine->algo->serialize(engine->state, Z_ARRVAL(tmp)) == FAILURE) {
			zend_throw_exception(NULL, "Engine serialize failed", 0);
			RETURN_THROWS();
		}
		zend_hash_str_add(Z_ARR_P(return_value), "__states", sizeof("__states") - 1, &tmp);
	}
}
/* }}} */

/* {{{ Construct object */
PHP_METHOD(Random_Engine_MersenneTwister, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_engine_state_mersennetwister *state = engine->state;
	zend_long seed, mode = MT_RAND_MT19937;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_LONG(seed);
		Z_PARAM_OPTIONAL;
		Z_PARAM_LONG(mode)
	ZEND_PARSE_PARAMETERS_END();

	switch (mode) {
		case MT_RAND_PHP:
			state->mode = MT_RAND_PHP;
			break;
		default:
			state->mode = MT_RAND_MT19937;
	}

	engine->algo->seed(engine->state, seed);
}
/* }}} */

/* {{{ Construct object */
PHP_METHOD(Random_Engine_Secure, __construct)
{
	ZEND_PARSE_PARAMETERS_NONE();
}
/* }}} */

/* {{{ Construct object */
PHP_METHOD(Random_Engine_XorShift128Plus, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_engine_state_xorshift128plus *state = engine->state;
	zend_string *str_seed = NULL;
	zend_long int_seed = 0;
	int i, j;
	
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR_OR_LONG(str_seed, int_seed)
	ZEND_PARSE_PARAMETERS_END();
	
	if (str_seed) {
		/* char (8 bit) * 16 = 128 bits */
		if (str_seed->len == 16) {
			/* Endianness safe copy */
			for (i = 0; i < 2; i++) {
				state->s[i] = 0;
				for (j = 0; j < 8; j++) {
					state->s[i] += ((uint64_t) (unsigned char) ZSTR_VAL(str_seed)[(i * 8) + j]) << (j * 8);
				}
			}
		}  else {
			zend_argument_value_error(1, "state strings must be 16 bytes");
			RETURN_THROWS();
		}
	} else {
		engine->algo->seed(state, int_seed);
	}
}
/* }}} */

/* {{{ Construct object */
PHP_METHOD(Random_Engine_Xoshiro256StarStar, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_engine_state_xoshiro256starstar *state = engine->state;
	zend_string *str_seed = NULL;
	zend_long int_seed = 0;
	int i, j;
	
	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR_OR_LONG(str_seed, int_seed)
	ZEND_PARSE_PARAMETERS_END();
	
	if (str_seed) {
		/* char (8 bit) * 32 = 256 bits */
		if (str_seed->len == 32) {
			/* Endianness safe copy */
			for (i = 0; i < 4; i++) {
				state->s[i] = 0;
				for (j = 0; j < 8; j++) {
					state->s[i] += ((uint64_t) (unsigned char) ZSTR_VAL(str_seed)[(i * 8) + j]) << (j * 8);
				}
			}
		}  else {
			zend_argument_value_error(1, "state strings must be 32 bytes");
			RETURN_THROWS();
		}
	} else {
		engine->algo->seed(state, int_seed);
	}
}
/* }}} */

/* {{{ Construct object */
PHP_METHOD(Random_Randomizer, __construct)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_object *engine_object = 0;
	zval zengine_object;
	zend_string *pname;
	
	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_OBJ_OF_CLASS_OR_NULL(engine_object, random_ce_Random_Engine);
	ZEND_PARSE_PARAMETERS_END();

	/* Create default RNG instance */
	if (!engine_object) {
		engine_object = php_random_engine_secure_new(random_ce_Random_Engine_Secure);
		zend_call_known_instance_method_with_0_params(
			random_ce_Random_Engine_Secure->constructor,
			engine_object,
			NULL
		);

		/* No need self-refcount */
		GC_DELREF(engine_object);
	}

	ZVAL_OBJ(&zengine_object, engine_object);

	/* Write property */
	pname = zend_string_init("engine", sizeof("engine") - 1, 0);
	zend_std_write_property(&randomizer->std, pname, &zengine_object, NULL);
	zend_string_release(pname);

	php_random_randomizer_common_init(randomizer, engine_object);
}
/* }}} */

/* {{{ Generate random number in range */
PHP_METHOD(Random_Randomizer, getInt)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_long min, max;
	int argc = ZEND_NUM_ARGS();

	if (argc == 0) {
		// right shift for compatibility
		RETURN_LONG(((uint64_t) randomizer->algo->generate(randomizer->state)) >> 1);
	}

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_LONG(min)
		Z_PARAM_LONG(max)
	ZEND_PARSE_PARAMETERS_END();

	if (UNEXPECTED(max < min)) {
		zend_argument_value_error(2, "must be greater than or equal to argument #1 ($min)");
		RETURN_THROWS();
	}

	RETURN_LONG(php_random_engine_range(randomizer->algo, randomizer->state, min, max));
}
/* }}} */

/* {{{ Generate random bytes string in ordered length */
PHP_METHOD(Random_Randomizer, getBytes)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zend_string *retval;
	zend_long length;
	size_t total_size = 0;
	size_t required_size;
	int i;

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
		uint64_t result;
		size_t generated_size;

		RANDOM_ENGINE_GENERATE_SIZE(randomizer->algo, randomizer->state, result, generated_size);

		for (i = 0; i < generated_size; i++) {
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
	php_array_data_shuffle(randomizer->algo, randomizer->state, return_value);
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
	php_string_shuffle(randomizer->algo, randomizer->state, Z_STRVAL_P(return_value), (zend_long) Z_STRLEN_P(return_value));
}
/* }}} */

/* {{{ Serialize object */
PHP_METHOD(Random_Randomizer, __serialize)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	zval tmp;

	ZEND_PARSE_PARAMETERS_NONE();

	array_init(return_value);
	ZVAL_ARR(&tmp, zend_std_get_properties(&randomizer->std));
	Z_TRY_ADDREF(tmp);
	zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &tmp);
}
/* }}} */

/* {{{ Unserialize object */
PHP_METHOD(Random_Randomizer, __unserialize)
{
	php_random_randomizer *randomizer = Z_RANDOM_RANDOMIZER_P(ZEND_THIS);
	HashTable *data;
	zval *members_zv;
	zval *zengine;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(data);
	ZEND_PARSE_PARAMETERS_END();

	members_zv = zend_hash_index_find(data, 0);
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

	php_random_randomizer_common_init(randomizer, Z_OBJ_P(zengine));
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(random)
{
	/* Random\Engine */
	random_ce_Random_Engine = register_class_Random_Engine();

	/* Random\Engine\CombinedLCG */
	random_ce_Random_Engine_CombinedLCG = register_class_Random_Engine_CombinedLCG(random_ce_Random_Engine);
	random_ce_Random_Engine_CombinedLCG->create_object = php_random_engine_combinedlcg_new;
	memcpy(&random_engine_combinedlcg_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_combinedlcg_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_combinedlcg_object_handlers.free_obj = php_random_engine_common_free_obj;
	random_engine_combinedlcg_object_handlers.clone_obj = php_random_engine_common_clone_obj;

	/* Random\Engine\MersenneTwister */
	random_ce_Random_Engine_MersenneTwister = register_class_Random_Engine_MersenneTwister(random_ce_Random_Engine);
	random_ce_Random_Engine_MersenneTwister->create_object = php_random_engine_mersennetwister_new;
	memcpy(&random_engine_mersennetwister_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_mersennetwister_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_mersennetwister_object_handlers.free_obj = php_random_engine_common_free_obj;
	random_engine_mersennetwister_object_handlers.clone_obj = php_random_engine_common_clone_obj;

	/* Random\Engine\Secure */
	random_ce_Random_Engine_Secure = register_class_Random_Engine_Secure(random_ce_Random_Engine);
	random_ce_Random_Engine_Secure->create_object = php_random_engine_secure_new;
	memcpy(&random_engine_secure_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_secure_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_secure_object_handlers.free_obj = php_random_engine_common_free_obj;
	random_engine_secure_object_handlers.clone_obj = NULL;

	/* Random\Engine\XorShift128Plus */
	random_ce_Random_Engine_XorShift128Plus = register_class_Random_Engine_XorShift128Plus(random_ce_Random_Engine);
	random_ce_Random_Engine_XorShift128Plus->create_object = php_random_engine_xorshift128plus_new;
	memcpy(&random_engine_xorshift128plus_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_xorshift128plus_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_xorshift128plus_object_handlers.free_obj = php_random_engine_common_free_obj;
	random_engine_xorshift128plus_object_handlers.clone_obj = php_random_engine_common_clone_obj;

	/* Random\Engine\Xoshiro256StarStar */
	random_ce_Random_Engine_Xoshiro256StarStar = register_class_Random_Engine_Xoshiro256StarStar(random_ce_Random_Engine);
	random_ce_Random_Engine_Xoshiro256StarStar->create_object = php_random_engine_xoshiro256starstar_new;
	memcpy(&random_engine_xoshiro256starstar_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_xoshiro256starstar_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_xoshiro256starstar_object_handlers.free_obj = php_random_engine_common_free_obj;
	random_engine_xoshiro256starstar_object_handlers.clone_obj = php_random_engine_common_clone_obj;

	/* Random\Randomizer */
	random_ce_Random_Randomizer = register_class_Random_Randomizer();
	random_ce_Random_Randomizer->create_object = php_random_randomizer_new;
	memcpy(&random_randomizer_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_randomizer_object_handlers.offset = XtOffsetOf(php_random_randomizer, std);
	random_randomizer_object_handlers.free_obj = php_random_randomizer_free_obj;
	random_randomizer_object_handlers.clone_obj = NULL;

	REGISTER_LONG_CONSTANT("MT_RAND_MT19937", MT_RAND_MT19937, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("MT_RAND_PHP",     MT_RAND_PHP, CONST_CS | CONST_PERSISTENT);

	RANDOM_G(random_fd) = -1;
	memset(&RANDOM_G(mt), 0, sizeof(php_random_engine_state_mersennetwister));
	memset(&RANDOM_G(clcg), 0, sizeof(php_random_engine_state_combinedlcg));

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

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(random)
{
#if defined(ZTS) && defined(COMPILE_DL_RANDOM)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
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
	NULL,						/* PHP_RSHUTDOWN - Request shutdown */
	NULL,						/* PHP_MINFO - Module info */
	PHP_VERSION,				/* Version */
	PHP_MODULE_GLOBALS(random),	/* ZTS Module globals */
	NULL,						/* PHP_GINIT - Global initialization */
	NULL,						/* PHP_GSHUTDOWN - Global shutdown */
	NULL,						/* Post deactivate */
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_RANDOM
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(random)
#endif
