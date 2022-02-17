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
            };
        }
    }
);

if ($randomizer->getBytes(5) !== 'Hello') {
    die('failure');
}

die('success');
?>
--EXPECTF--
success
