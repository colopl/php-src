--TEST--
Random: Randomizer: Compatibility: user
--FILE--
<?php

$native_randomizer = new \Random\Randomizer(new \Random\Engine\CombinedLCG(1234));
$user_randomizer = new \Random\Randomizer(new class (1234) extends \Random\Engine\CombinedLCG {});

for ($i = 0; $i < 1000; $i++) {
    if ($native_randomizer->getInt() !== $user_randomizer->getInt()) {
        die('failure CombinedLCG');
    }
}

$native_randomizer = new \Random\Randomizer(new \Random\Engine\MersenneTwister(1234));
$user_randomizer = new \Random\Randomizer(new class (1234) extends \Random\Engine\MersenneTwister {});

for ($i = 0; $i < 1000; $i++) {
    if ($native_randomizer->getInt() !== $user_randomizer->getInt()) {
        die('failure MersenneTwister');
    }
}

$native_randomizer = new \Random\Randomizer(new \Random\Engine\XorShift128Plus(1234));
$user_randomizer = new \Random\Randomizer(new class (1234) extends \Random\Engine\XorShift128Plus {});

for ($i = 0; $i < 1000; $i++) {
    if ($native_randomizer->getInt() !== $user_randomizer->getInt()) {
        die('failure XorShift128Plus');
    }
}

die('success');
?>
--EXPECT--
success
