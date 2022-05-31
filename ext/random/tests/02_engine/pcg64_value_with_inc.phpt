--TEST--
Random: Engine: PCG64: value with inc
--FILE--
<?php

$engine = new \Random\Engine\PCG64(1234, 114514);

for ($i = 0; $i < 10000; $i++) {
    $engine->generate();
}

$engine->jump(1234567);

echo \bin2hex($engine->generate());
?>
--EXPECT--
9eac7778c37b81f0
