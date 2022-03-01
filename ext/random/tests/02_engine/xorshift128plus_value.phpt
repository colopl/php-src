--TEST--
Random: Engine: XorShift128Plus: value
--FILE--
<?php

$engine = new \Random\Engine\XorShift128Plus(1234);

for ($i = 0; $i < 10000; $i++) {
    $engine->generate();
}

$engine->jump();

echo \bin2hex($engine->generate());
?>
--EXPECT--
33c167aa7ff25bbb
