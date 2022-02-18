--TEST--
Random: Engine: Xoshiro256StarStar: value
--FILE--
<?php

$engine = new \Random\Engine\Xoshiro256StarStar(1234);

for ($i = 0; $i < 10000; $i++) {
    $engine->generate();
}

echo \bin2hex($engine->generate());
?>
--EXPECT--
7857d0659ea58de6
