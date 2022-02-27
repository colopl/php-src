--TEST--
Random: Randomizer: User: RNG unsafe
--FILE--
<?php

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
    echo "catched\n";
}

try {
    $randomizer->shuffleArray(\range(1, 10));
} catch (\RuntimeException $e) {
    echo "catched\n";
}

try {
    $randomizer->shuffleString('foobar');
} catch (\RuntimeException $e) {
    echo "catched\n";
}

?>
--EXPECT--
catched
catched
catched
