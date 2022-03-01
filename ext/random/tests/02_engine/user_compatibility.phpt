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

$native_engine = new \Random\Engine\XorShift128Plus(1234);
$user_engine = new class (1234) extends \Random\Engine\XorShift128Plus {};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure XorShift128Plus');
    }
}

$native_engine = new \Random\Engine\Xoshiro256StarStar(1234);
$user_engine = new class (1234) extends \Random\Engine\Xoshiro256StarStar {};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure Xoshiro256StarStar');
    }
}

die('success');
?>
--EXPECT--
success
