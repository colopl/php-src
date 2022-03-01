--TEST--
Random: Engine: PCG64: value
--FILE--
<?php

$engine = new \Random\Engine\PCG64(1234);

for ($i = 0; $i < 10000; $i++) {
    $engine->generate();
}

$engine->jump(1234567);

echo \bin2hex($engine->generate());
?>
--EXPECT--
17dc0f000c3d4967
