--TEST--
Random: Randomizer: readonly numbergenerator
--FILE--
<?php

$one = new \Random\Randomizer();
$two = new \Random\Randomizer();

$one_ng_clone = clone $one->numberGenerator;
if ($one->numberGenerator->generate() !== $one_ng_clone->generate()) {
    die('invalid result');
}

try {
    $one->numberGenerator = $one_ng_clone;
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

die('success')
?>
--EXPECT--
Cannot modify readonly property Random\Randomizer::$numberGenerator
success
