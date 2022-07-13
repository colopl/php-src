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
#define mixBits(u, v) (hiBit(u) | loBits(v)) /* move hi bit of u to hi bit of v */

#define twist(m,u,v)  (m ^ (mixBits(u,v) >> 1) ^ ((uint32_t)(-(int32_t)(loBit(v))) & 0x9908b0dfU))
#define twist_php(m,u,v)  (m ^ (mixBits(u,v) >> 1) ^ ((uint32_t)(-(int32_t)(loBit(u))) & 0x9908b0dfU))

static inline void mt19937_reload(php_random_status_state_mt19937 *state) {
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

static inline void mt19937_seed_state(php_random_status_state_mt19937 *state, uint64_t seed)
{
	uint32_t i, prev_state;

	/* Initialize generator state with seed
	   See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
	   In previous versions, most significant bits (MSBs) of the seed affect
	   only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto. */
	state->state[0] = seed & 0xffffffffU;
	for (i = 1; i < MT_N; i++) {
		prev_state = state->state[i - 1];
		state->state[i] = (1812433253U * (prev_state  ^ (prev_state  >> 30)) + i) & 0xffffffffU;
	}
	state->count = i;

	mt19937_reload(state);
}

static void seed(php_random_status *status, uint64_t seed)
{
	mt19937_seed_state(status->state, seed);
}

static uint64_t generate(php_random_status *status)
{
	php_random_status_state_mt19937 *s = status->state;
	uint32_t s1;

	if (s->count >= MT_N) {
		mt19937_reload(s);
	}

	s1 = s->state[s->count++];
	s1 ^= (s1 >> 11);
	s1 ^= (s1 << 7) & 0x9d2c5680U;
	s1 ^= (s1 << 15) & 0xefc60000U;

	return (uint64_t) (s1 ^ (s1 >> 18));
}

static zend_long range(php_random_status *status, zend_long min, zend_long max)
{
	php_random_status_state_mt19937 *s = status->state;

	if (s->mode == MT_RAND_MT19937) {
		return php_random_range(&php_random_algo_mt19937, status, min, max);
	}

	uint64_t r = php_random_algo_mt19937.generate(status) >> 1;
	/* Legacy mode deliberately not inside php_mt_rand_range()
	 * to prevent other functions being affected */
	RAND_RANGE_BADSCALING(r, min, max, PHP_MT_RAND_MAX);
	return (zend_long) r;
}

static bool serialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_mt19937 *s = status->state;
	zval t;
	uint32_t i;

	for (i = 0; i< MT_N; i++) {
		ZVAL_STR(&t, php_random_bin2hex_le(&s->state[i], sizeof(uint32_t)));
		zend_hash_next_index_insert(data, &t);
	}
	ZVAL_LONG(&t, s->count);
	zend_hash_next_index_insert(data, &t);
	ZVAL_LONG(&t, s->mode);
	zend_hash_next_index_insert(data, &t);
	
	return true;
}

static bool unserialize(php_random_status *status, HashTable *data)
{
	php_random_status_state_mt19937 *s = status->state;
	zval *t;
	uint32_t i;

	for (i = 0; i < MT_N; i++) {
		t = zend_hash_index_find(data, i);
		if (!t || Z_TYPE_P(t) != IS_STRING || Z_STRLEN_P(t) != (2 * sizeof(uint32_t))) {
			return false;
		}
		if (!php_random_hex2bin_le(Z_STR_P(t), &s->state[i])) {
			return false;
		}
	}
	t = zend_hash_index_find(data, MT_N); 
	if (!t || Z_TYPE_P(t) != IS_LONG) {
		return false;
	}
	s->count = Z_LVAL_P(t);
	t = zend_hash_index_find(data, MT_N + 1);
	if (!t || Z_TYPE_P(t) != IS_LONG) {
		return false;
	}
	s->mode = Z_LVAL_P(t);

	return true;
}

const php_random_algo php_random_algo_mt19937 = {
	sizeof(uint32_t),
	sizeof(php_random_status_state_mt19937),
	seed,
	generate,
	range,
	serialize,
	unserialize
};

/* {{{ php_random_mt19937_seed_default */
PHPAPI void php_random_mt19937_seed_default(php_random_status_state_mt19937 *state)
{
	zend_long seed = 0;

	if (php_random_bytes_silent(&seed, sizeof(zend_long)) == FAILURE) {
		seed = GENERATE_SEED();
	}

	mt19937_seed_state(state, (uint64_t) seed);
}
/* }}} */

/* {{{ Random\Engine\Mt19937::__construct() */
PHP_METHOD(Random_Engine_Mt19937, __construct)
{
	php_random_engine *engine = Z_RANDOM_ENGINE_P(ZEND_THIS);
	php_random_status_state_mt19937 *state = engine->status->state;
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
			zend_throw_exception(spl_ce_RuntimeException, "Random number generation failed", 0);
			RETURN_THROWS();
		}
	}

	engine->algo->seed(engine->status, seed);
}
/* }}} */

/* {{{ Random\Engine\Mt19937::generate() */
PHP_METHOD(Random_Engine_Mt19937, generate)
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
		zend_throw_exception(spl_ce_RuntimeException, "Random number generation failed", 0);
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

/* {{{ Random\Engine\Mt19937::__serialize() */
PHP_METHOD(Random_Engine_Mt19937, __serialize)
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

/* {{{ Random\Engine\Mt19937::__unserialize() */
PHP_METHOD(Random_Engine_Mt19937, __unserialize)
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

/* {{{ Random\Engine\Mt19937::__debugInfo() */
PHP_METHOD(Random_Engine_Mt19937, __debugInfo)
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
