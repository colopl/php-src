--TEST--
Random: NumberGenerator: clone
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
        $clone_generator = clone $generator;
    } catch (Throwable $e) {
        echo $e->getMessage() . PHP_EOL;
        continue;
    }

    echo $generator::class . ': ';
    if ($generator->generate() === $clone_generator->generate()) {
        echo "success\n";
    } else {
        echo "failure\n";
    }
}

?>
--EXPECTF--
Random\NumberGenerator\XorShift128Plus: success
Random\NumberGenerator\MersenneTwister: success
Random\NumberGenerator\MersenneTwister: success
Random\NumberGenerator\CombinedLCG: success
Trying to clone an uncloneable object of class Random\NumberGenerator\Secure
Random\NumberGenerator@anonymous%s: success
UserNumberGenerator: success
Random\NumberGenerator\XorShift128Plus@anonymous%s: success
UserXorShift128Plus: success
Random\NumberGenerator\MersenneTwister@anonymous%s: success
UserMersenneTwister: success
Random\NumberGenerator\CombinedLCG@anonymous%s: success
UserCombinedLCG: success
Trying to clone an uncloneable object of class Random\NumberGenerator\Secure@anonymous
Trying to clone an uncloneable object of class UserSecure
