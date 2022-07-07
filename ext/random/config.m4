dnl
dnl Check for arc4random on BSD systems
dnl
AC_CHECK_DECLS([arc4random_buf])

dnl
dnl Setup extension
dnl
PHP_NEW_EXTENSION(random, random.c engine_combinedlcg.c engine_mersennetwister.c engine_pcg64s.c engine_secure.c engine_user.c randomizer.c, , , -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
PHP_INSTALL_HEADERS([ext/random])
