--TEST--
Random: NumberGenerator: inheritance
--FILE--
<?php
class UserXorShift128Plus extends Random\NumberGenerator\XorShift128Plus
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}

class UserMersenneTwister extends Random\NumberGenerator\MersenneTwister
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}

class UserCombinedLCG extends Random\NumberGenerator\CombinedLCG
{
    public function generate(): int
    {
        return parent::generate() - 1;
    }
}

foreach (['XorShift128Plus', 'MersenneTwister', 'CombinedLCG'] as $className) {
    $userClassName = 'User' . $className;
    $fullyClassName = '\\Random\\NumberGenerator\\' . $className;
    $seed = \random_int(\PHP_INT_MIN, \PHP_INT_MAX);
    $original = new $fullyClassName($seed);
    $user = new $userClassName($seed);

    if (($original->generate() - 1) !== $user->generate()) {
        die("failed: ${className}");
    }
}

die('success');
--EXPECT--
success
