--TEST--
Random: NumberGenerator: XorShift128Plus: seed (state) by string
--FILE--
<?php

$xorshift128plus = new Random\NumberGenerator\XorShift128Plus(\random_bytes(16));

try {
    $xorshift128plus = new Random\NumberGenerator\XorShift128Plus('foobar');
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

?>
--EXPECT--
Random\NumberGenerator\XorShift128Plus::__construct(): Argument #1 ($seed) state strings must be 16 bytes
