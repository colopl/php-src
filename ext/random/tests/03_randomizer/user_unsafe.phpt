--TEST--
Random: Randomizer: User: Engine unsafe
--FILE--
<?php

// Empty generator
$randomizer = (new \Random\Randomizer(
    new class () implements \Random\Engine {
        public function generate(): string
        {
            return '';
        }
    }
));

try {
    $randomizer->getInt(\PHP_INT_MIN, \PHP_INT_MAX);
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->getBytes(1);
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->shuffleArray(\range(1, 10));
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->shuffleString('foobar');
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

// Infinite loop
$randomizer = (new \Random\Randomizer(
    new class () implements \Random\Engine {
        public function generate(): string
        {
            return "\xff\xff\xff\xff\xff\xff\xff\xff";
        }
    }
));

try {
    $randomizer->getInt(\PHP_INT_MIN, \PHP_INT_MAX);
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->getBytes(1);
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->shuffleArray(\range(1, 10));
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

try {
    $randomizer->shuffleString('foobar');
} catch (\RuntimeException $e) {
    echo "{$e->getMessage()}\n";
}

?>
--EXPECT--
Random number generate failed
Random number generate failed
Random number generate failed
Random number generate failed
Random number generate failed
Random number generate failed
