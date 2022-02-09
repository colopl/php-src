--TEST--
Random: NumberGenerator: serialize error pattern
--FILE--
<?php

$generators = [];
$generators[] = new Random\NumberGenerator\XorShift128Plus(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$generators[] = new Random\NumberGenerator\MersenneTwister(\random_int(\PHP_INT_MIN, \PHP_INT_MAX), MT_RAND_MT19937);
$generators[] = new Random\NumberGenerator\MersenneTwister(\random_int(\PHP_INT_MIN, \PHP_INT_MAX), MT_RAND_PHP);
$generators[] = new Random\NumberGenerator\CombinedLCG(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$generators[] = new Random\NumberGenerator\Secure(); 
$generators[] = new class () implements Random\NumberGenerator {
    private int $count = 0;

    public function generate(): int
    {
        return ++$this->count;
    }
};
class UserNumberGenerator implements Random\NumberGenerator
{
    private int $count = 0;

    public function generate(): int
    {
        return ++$this->count;
    }
}
$generators[] = new UserNumberGenerator();
$generators[] = new class (\random_int(\PHP_INT_MIN, \PHP_INT_MAX)) extends Random\NumberGenerator\XorShift128Plus {
    public function generate(): int
    {
        return parent::generate() - 1;
    }
};
class UserXorShift128Plus extends Random\NumberGenerator\XorShift128Plus
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}
$generators[] = new UserXorShift128Plus(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$generators[] = new class (\random_int(\PHP_INT_MIN, \PHP_INT_MAX)) extends Random\NumberGenerator\MersenneTwister {
    public function generate(): int
    {
        return parent::generate() - 1;
    }
};
class UserMersenneTwister extends Random\NumberGenerator\MersenneTwister
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}
$generators[] = new UserMersenneTwister(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$generators[] = new class (\random_int(\PHP_INT_MIN, \PHP_INT_MAX)) extends Random\NumberGenerator\CombinedLCG {
    public function generate(): int
    {
        return parent::generate() - 1;
    }
};
class UserCombinedLCG extends Random\NumberGenerator\CombinedLCG
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}
$generators[] = new UserCombinedLCG(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$generators[] = new class () extends Random\NumberGenerator\Secure {
    public function generate(): int
    {
        return parent::generate() - 1;
    }
};
class UserSecure extends Random\NumberGenerator\Secure
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}
$generators[] = new UserSecure();

foreach ($generators as $generator) {
    $generator->generate();

    try {
        $serialized_generator = \serialize($generator);
    } catch (Throwable $e) {
        echo $e->getMessage() . PHP_EOL;
        continue;
    }

    $broken_serialized_generator = preg_replace('/a\:[0-9]+\:\{.*?\}/', 'a:0:{}', $serialized_generator);

    try {
        $unserialized_generator = \unserialize($broken_serialized_generator);
    } catch (Throwable $e) {
        echo $generator::class . ': ' . $e->getMessage() . PHP_EOL;
    }
}

?>
--EXPECTF--
Random\NumberGenerator\XorShift128Plus: NumberGenerator unserialize failed
Random\NumberGenerator\MersenneTwister: NumberGenerator unserialize failed
Random\NumberGenerator\MersenneTwister: NumberGenerator unserialize failed
Random\NumberGenerator\CombinedLCG: NumberGenerator unserialize failed
Serialization of 'Random\NumberGenerator\Secure' is not allowed
Serialization of 'Random\NumberGenerator@anonymous' is not allowed
Serialization of 'Random\NumberGenerator\XorShift128Plus@anonymous' is not allowed
UserXorShift128Plus: NumberGenerator unserialize failed
Serialization of 'Random\NumberGenerator\MersenneTwister@anonymous' is not allowed
UserMersenneTwister: NumberGenerator unserialize failed
Serialization of 'Random\NumberGenerator\CombinedLCG@anonymous' is not allowed
UserCombinedLCG: NumberGenerator unserialize failed
Serialization of 'Random\NumberGenerator\Secure@anonymous' is not allowed
Serialization of 'UserSecure' is not allowed