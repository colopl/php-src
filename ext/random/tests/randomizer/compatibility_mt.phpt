--TEST--
Random: Randomizer: Compatibility: MT
--FILE--
<?php

$randomizer = new \Random\Randomizer(new \Random\Engine\MersenneTwister(1234, \MT_RAND_PHP));
\mt_srand(1234, \MT_RAND_PHP);
for ($i = 0; $i < 1000; $i++) {
    if ($randomizer->getInt() !== \mt_rand()) {
        die('failure');
    }
}

$randomizer = new \Random\Randomizer(new \Random\Engine\MersenneTwister(1234, \MT_RAND_MT19937));
\mt_srand(1234, \MT_RAND_MT19937);
for ($i = 0; $i < 1000; $i++) {
    if ($randomizer->getInt() !== \mt_rand()) {
        die('failure');
    }
}

die('success');
--EXPECT--
success
