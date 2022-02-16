--TEST--
Random: Engine: XorShift128Plus: seed
--FILE--
<?php declare(strict_types = 1);

$engine = new \Random\Engine\XorShift128Plus(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$engine = new \Random\Engine\XorShift128Plus(\random_bytes(16));

try {
    $engine = new Random\Engine\XorShift128Plus(1.0);
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

try {
    $engine = new Random\Engine\XorShift128Plus('foobar');
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

?>
--EXPECT--
Random\Engine\XorShift128Plus::__construct(): Argument #1 ($seed) must be of type string|int, float given
Random\Engine\XorShift128Plus::__construct(): Argument #1 ($seed) state strings must be 16 bytes
