--TEST--
Random: Engine: User: compatibility
--FILE--
<?php

$native_engine = new \Random\Engine\MersenneTwister(1234);
$user_engine = new class () implements \Random\Engine {
    public function __construct(private $engine = new \Random\Engine\MersenneTwister(1234))
    {
    }

    public function generate(): string
    {
        return $this->engine->generate();
    }
};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure MersenneTwister');
    }
}

$native_engine = new \Random\Engine\PCG64(1234);
$user_engine = new class () implements \Random\Engine {
    public function __construct(private $engine = new \Random\Engine\PCG64(1234))
    {
    }

    public function generate(): string
    {
        return $this->engine->generate();
    }
};

for ($i = 0; $i < 1000; $i++) {
    if ($native_engine->generate() !== $user_engine->generate()) {
        die('failure PCG64');
    }
}

die('success');
?>
--EXPECT--
success
