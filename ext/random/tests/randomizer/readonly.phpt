--TEST--
Random: Randomizer: readonly numbergenerator
--FILE--
<?php

$one = new \Random\Randomizer();

$one_ng_clone = clone $one->numberGenerator;
if ($one->numberGenerator->generate() !== $one_ng_clone->generate()) {
    die('invalid result');
}

try {
    $one->numberGenerator = $one_ng_clone;
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

$two = new \Random\Randomizer(
    new \Random\NumberGenerator\Secure()
);

try {
    $two_ng_clone = clone $two->numberGenerator;
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

try {
    $two->numberGenerator = $one_ng_clone;
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

die('success')
?>
--EXPECT--
Cannot modify readonly property Random\Randomizer::$numberGenerator
Trying to clone an uncloneable object of class Random\NumberGenerator\Secure
Cannot modify readonly property Random\Randomizer::$numberGenerator
success
