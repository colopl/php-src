--TEST--
Random: NumberGenerator: XorShift128Plus: seed by int
--FILE--
<?php declare(strict_types = 1);

$xorshift128plus = new Random\NumberGenerator\XorShift128Plus(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));

try {
    $xorshift128plus = new Random\NumberGenerator\XorShift128Plus(1.0);
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

?>
--EXPECT--
Random\NumberGenerator\XorShift128Plus::__construct(): Argument #1 ($seed) must be of type string|int, float given
