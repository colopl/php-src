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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_random.h"

#include "ext/spl/spl_exceptions.h"
#include "Zend/zend_exceptions.h"

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

static inline void mersennetwister_seed_state(php_random_status_state_mersennetwister *state, uint64_t seed)
{
	/* Initialize generator state with seed
	   See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	   In previous versions, most significant bits (MSBs) of the seed affect
	   only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto. */
	state->state[0] = seed & 0xffffffffU;
	for (state->count = 1; state->count < MT_N; state->count++) {
		state->state[state->count] = (1812433253U * (state->state[state->count - 1] ^ (state->state[state->count - 1] >> 30)) + state->count) & 0xffffffffU;
	}

	mersennetwister_reload(state);
}

static void mersennetwister_seed(php_random_status *status, uint64_t seed)
{
    mersennetwister_seed_state(status->state, seed);
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
		return php_random_range(&php_random_algo_mersennetwister, status, min, max);
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

/* {{{ php_random_mersennetwister_seed_default */
PHPAPI void php_random_mersennetwister_seed_default(php_random_status_state_mersennetwister *state)
{
	zend_long seed = 0;

	if (php_random_bytes_silent(&seed, sizeof(zend_long)) == FAILURE) {
		/* FIXME: This is a security risk. Shall I throw an E_WARNING? */
		seed = GENERATE_SEED();
	}

	mersennetwister_seed_state(state, (uint64_t) seed);
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
