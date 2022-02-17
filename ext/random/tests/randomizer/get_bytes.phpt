--TEST--
Random: Randomizer: getBytes
--FILE--
<?php

$randomizer = new \Random\Randomizer (
    new class () implements \Random\Engine
    {
        private int $count = 0;

        public function generate(): string
        {
            return match ($this->count++) {
                0 => 'H',
                1 => 'e',
                2 => 'l',
                3 => 'l',
                4 => 'o',
                5 => \random_bytes(32), // 128-bit
                default => \random_bytes(16),
            };
        }
    }
);

if ($randomizer->getBytes(5) !== 'Hello') {
    die('failure');
}

$randomizer->getBytes(6);

die('success');
?>
--EXPECTF--
success
