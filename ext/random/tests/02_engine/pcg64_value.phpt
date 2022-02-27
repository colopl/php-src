--TEST--
Random: Engine: PCG64: value
--FILE--
<?php

$engine = new \Random\Engine\PCG64(1234);

for ($i = 0; $i < 10000; $i++) {
    $engine->generate();
}

echo \bin2hex($engine->generate());
?>
--EXPECT--
223f83060a787350
