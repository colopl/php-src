--TEST--
Random: Engine: User: compatibility
--FILE--
<?php

$native_engine = new \Random\Engine\CombinedLCG(1234);
$user_engine = new class (1234) extends \Random\Engine\CombinedLCG {};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure CombinedLCG');
    }
}

$native_engine = new \Random\Engine\MersenneTwister(1234);
$user_engine = new class (1234) extends \Random\Engine\MersenneTwister {};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure MersenneTwister');
    }
}

$native_engine = new \Random\Engine\PCG64(1234);
$user_engine = new class (1234) extends \Random\Engine\PCG64 {};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure PCG64');
    }
}

die('success');
?>
--EXPECT--
success
