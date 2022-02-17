--TEST--
Random: Randomizer: serialize
--FILE--
<?php

$engines = [];
$engines[] = new Random\Engine\XorShift128Plus(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$engines[] = new Random\Engine\MersenneTwister(\random_int(\PHP_INT_MIN, \PHP_INT_MAX), MT_RAND_MT19937);
$engines[] = new Random\Engine\MersenneTwister(\random_int(\PHP_INT_MIN, \PHP_INT_MAX), MT_RAND_PHP);
$engines[] = new Random\Engine\CombinedLCG(\random_int(\PHP_INT_MIN, \PHP_INT_MAX));
$engines[] = new Random\Engine\Secure(); 
$engines[] = new class () implements Random\Engine {
    public function generate(): string
    {
        return '�}Q��R';
    }
};
class UserEngine implements Random\Engine
{
    public function generate(): string
    {
        return '�}Q��R';
    }
}
$engines[] = new UserEngine();

foreach ($engines as $engine) {
    $randomizer = new Random\Randomizer($engine);
    $randomizer->getInt(\PHP_INT_MIN, \PHP_INT_MAX);
    try {
        $randomizer2 = unserialize(serialize($randomizer));
    } catch (\Exception $e) {
        echo $e->getMessage() . PHP_EOL;
        continue;
    }

    if ($randomizer->getInt(\PHP_INT_MIN, \PHP_INT_MAX) !== $randomizer2->getInt(\PHP_INT_MIN, \PHP_INT_MAX)) {
        die($engine::class . ': failure.');
    }

    echo $engine::class . ': success' . PHP_EOL;
}

die('success');
?>
--EXPECTF--
Random\Engine\XorShift128Plus: success
Random\Engine\MersenneTwister: success
Random\Engine\MersenneTwister: success
Random\Engine\CombinedLCG: success
Serialization of 'Random\Engine\Secure' is not allowed
Serialization of 'Random\Engine@anonymous' is not allowed
UserEngine: success
success
