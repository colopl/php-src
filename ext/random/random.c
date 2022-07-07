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

static zend_object *php_random_engine_combinedlcg_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_combinedlcg_object_handlers, &php_random_algo_combinedlcg)->std;
}

static zend_object *php_random_engine_mersennetwister_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_mersennetwister_object_handlers, &php_random_algo_mersennetwister)->std;
}

static zend_object *php_random_engine_pcg64_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_pcg64_object_handlers, &php_random_algo_pcg64s)->std;
}

static zend_object *php_random_engine_secure_new(zend_class_entry *ce)
{
	return &php_random_engine_common_init(ce, &random_engine_secure_object_handlers, &php_random_algo_secure)->std;
}

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
	php_random_engine *engine = php_random_engine_from_obj(object);

	if (engine->status) {
		php_random_status_free(engine->status);
	}

	zend_object_std_dtor(object);
}

PHPAPI zend_object *php_random_engine_common_clone_object(zend_object *object)
{
	php_random_engine *old_engine = php_random_engine_from_obj(object);
	php_random_engine *new_engine = php_random_engine_from_obj(old_engine->std.ce->create_object(old_engine->std.ce));

	new_engine->algo = old_engine->algo;
	if (old_engine->status) {
		new_engine->status = php_random_status_copy(old_engine->algo, old_engine->status, new_engine->status);
	}

	zend_objects_clone_members(&new_engine->std, &old_engine->std);

	return &new_engine->std;
}

/* {{{ php_random_range */
PHPAPI zend_long php_random_range(const php_random_algo *algo, php_random_status *status, zend_long min, zend_long max)
{
	zend_ulong umax = max - min;

	if (PHP_RANDOM_ALGO_IS_DYNAMIC(algo) || algo->generate_size > sizeof(uint32_t) || umax > UINT32_MAX) {
		return (zend_long) rand_range64(algo, status, umax) + min;
	}

	return (zend_long) rand_range32(algo, status, umax) + min;
}
/* }}} */

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
		php_random_mersennetwister_seed_default(status->state);
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
		php_random_combinedlcg_seed_default(status->state);
		RANDOM_G(combined_lcg) = status;
	}

	return php_random_algo_combinedlcg.generate(status) * 4.656613e-10;
}
/* }}} */

/* {{{ php_mt_srand */
PHPAPI void php_mt_srand(uint32_t seed)
{
	/* Seed the generator with a simple uint32 */
	php_random_algo_mersennetwister.seed(php_random_default_status(), (zend_long) seed);
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
		php_random_mersennetwister_seed_default(status->state);
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
		php_random_mersennetwister_seed_default(status->state);
	} else {
		php_random_algo_mersennetwister.seed(status, (uint64_t) seed);
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
	random_engine_combinedlcg_object_handlers.free_obj = php_random_engine_common_free_object;
	random_engine_combinedlcg_object_handlers.clone_obj = php_random_engine_common_clone_object;

	/* Random\Engine\PCG64 */
	random_ce_Random_Engine_PCG64 = register_class_Random_Engine_PCG64(random_ce_Random_SeedableEngine, random_ce_Random_SerializableEngine);
	random_ce_Random_Engine_PCG64->create_object = php_random_engine_pcg64_new;
	memcpy(&random_engine_pcg64_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_pcg64_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_pcg64_object_handlers.free_obj = php_random_engine_common_free_object;
	random_engine_pcg64_object_handlers.clone_obj = php_random_engine_common_clone_object;

	/* Random\Engine\MersenneTwister */
	random_ce_Random_Engine_MersenneTwister = register_class_Random_Engine_MersenneTwister(random_ce_Random_SeedableEngine, random_ce_Random_SerializableEngine);
	random_ce_Random_Engine_MersenneTwister->create_object = php_random_engine_mersennetwister_new;
	memcpy(&random_engine_mersennetwister_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_mersennetwister_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_mersennetwister_object_handlers.free_obj = php_random_engine_common_free_object;
	random_engine_mersennetwister_object_handlers.clone_obj = php_random_engine_common_clone_object;

	/* Random\Engine\Secure */
	random_ce_Random_Engine_Secure = register_class_Random_Engine_Secure(random_ce_Random_CryptoSafeEngine);
	random_ce_Random_Engine_Secure->create_object = php_random_engine_secure_new;
	memcpy(&random_engine_secure_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	random_engine_secure_object_handlers.offset = XtOffsetOf(php_random_engine, std);
	random_engine_secure_object_handlers.free_obj = php_random_engine_common_free_object;
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

	RANDOM_G(combined_lcg) = NULL;
	RANDOM_G(mersennetwister) = NULL;
	
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
	RANDOM_G(combined_lcg) = NULL;

	if (RANDOM_G(mersennetwister)) {
		php_random_status_free(RANDOM_G(mersennetwister));
	}
	RANDOM_G(mersennetwister) = NULL;

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
