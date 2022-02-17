--TEST--
Random: Engine: Xoshiro256StarStar: seed
--FILE--
<?php declare(strict_types = 1);

$engine = new \Random\Engine\Xoshiro256StarStar(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$engine = new \Random\Engine\Xoshiro256StarStar(\random_bytes(32));

try {
    $engine = new Random\Engine\Xoshiro256StarStar(1.0);
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

try {
    $engine = new Random\Engine\Xoshiro256StarStar('foobar');
} catch (\Throwable $e) {
    echo $e->getMessage() . PHP_EOL;
}

?>
--EXPECT--
Random\Engine\Xoshiro256StarStar::__construct(): Argument #1 ($seed) must be of type string|int, float given
Random\Engine\Xoshiro256StarStar::__construct(): Argument #1 ($seed) state strings must be 32 bytes
