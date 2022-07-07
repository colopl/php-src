--TEST--
Random: Randomizer: Compatibility: user
--FILE--
<?php

$native_randomizer = new \Random\Randomizer(new \Random\Engine\MersenneTwister(1234));
$user_randomizer = new \Random\Randomizer(new class () implements \Random\Engine {
    public function __construct(private $engine = new \Random\Engine\MersenneTwister(1234))
    {
    }

    public function generate(): string
    {
        return $this->engine->generate();
    }
});

for ($i = 0; $i < 1000; $i++) {
    $native = $native_randomizer->getInt();
    $user = $user_randomizer->getInt();
    if ($native !== $user) {
        die("failure MersenneTwister i: {$i} native: {$native} user: {$user}");
    }
}

try {
    $native_randomizer = new \Random\Randomizer(new \Random\Engine\PCG64(1234));
    $user_randomizer = new \Random\Randomizer(new class () implements \Random\Engine {
    public function __construct(private $engine = new \Random\Engine\PCG64(1234))
    {
    }

    public function generate(): string
    {
        return $this->engine->generate();
    }
});

    for ($i = 0; $i < 1000; $i++) {
        $native = $native_randomizer->getInt();
        $user = $user_randomizer->getInt();
        if ($native !== $user) {
            die("failure PCG64 i: {$i} native: {$native} user: {$user}");
        }
    }
} catch (\RuntimeException $e) {
    if (\PHP_INT_SIZE >= 8) {
        throw $e;
    }
    if ($e->getMessage() !== 'Generated value exceeds size of int') {
        throw $e;
    }
}

die('success');
?>
--EXPECT--
success
