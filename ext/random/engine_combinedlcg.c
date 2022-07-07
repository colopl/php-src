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

/*
 * combinedLCG() returns a pseudo random number in the range of (0, 1).
 * The function combines two CGs with periods of
 * 2^31 - 85 and 2^31 - 249. The period of this function
 * is equal to the product of both primes.
 */
#define MODMULT(a, b, c, m, s) q = s/a;s=b*(s-a*q)-c*q;if(s<0)s+=m

static void seed(php_random_status *status, uint64_t seed)
{
	php_random_status_state_combinedlcg *s = status->state;

	s->state[0] = seed & 0xffffffffU;
	s->state[1] = seed >> 32;
}

static uint64_t generate(php_random_status *status)
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

static zend_long range(php_random_status *status, zend_long min, zend_long max)
{
	return php_random_range(&php_random_algo_combinedlcg, status, min, max);
}

static bool serialize(php_random_status *status, HashTable *data)
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

static bool unserialize(php_random_status *status, HashTable *data)
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
	seed,
	generate,
	range,
	serialize,
	unserialize
};

/* {{{ php_random_combinedlcg_seed_default */
PHPAPI void php_random_combinedlcg_seed_default(php_random_status_state_combinedlcg *state)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == 0) {
		state->state[0] = tv.tv_usec ^ (tv.tv_usec << 11);
	} else {
		state->state[0] = 1;
	}

#ifdef ZTS
	state->state[1] = (zend_long) tsrm_thread_id();
#else
	state->state[1] = (zend_long) getpid();
#endif

	/* Add entropy to s2 by calling gettimeofday() again */
	if (gettimeofday(&tv, NULL) == 0) {
		state->state[1] ^= (tv.tv_usec << 11);
	}
}
/* }}} */

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
