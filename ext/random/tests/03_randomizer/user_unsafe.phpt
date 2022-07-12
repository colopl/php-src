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
    var_dump($e);
}

try {
    $randomizer->getBytes(1);
} catch (\RuntimeException $e) {
    var_dump($e);
}

try {
    $randomizer->shuffleArray(\range(1, 10));
} catch (\RuntimeException $e) {
    var_dump($e);
}

try {
    $randomizer->shuffleBytes('foobar');
} catch (\RuntimeException $e) {
    var_dump($e);
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
    var_dump($e);
}

try {
    $randomizer->getBytes(1);
} catch (\RuntimeException $e) {
    var_dump($e);
}

try {
    $randomizer->shuffleArray(\range(1, 10));
} catch (\RuntimeException $e) {
    var_dump($e);
}

try {
    $randomizer->shuffleBytes('foobar');
} catch (\RuntimeException $e) {
    var_dump($e);
}

?>
--EXPECTF--
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(%d)
      ["function"]=>
      string(6) "getInt"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(2) {
        [0]=>
        int(%d)
        [1]=>
        int(%d)
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(%d)
      ["function"]=>
      string(8) "getBytes"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(1) {
        [0]=>
        int(1)
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(%d)
      ["function"]=>
      string(12) "shuffleArray"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(1) {
        [0]=>
        array(10) {
          [0]=>
          int(1)
          [1]=>
          int(2)
          [2]=>
          int(3)
          [3]=>
          int(4)
          [4]=>
          int(5)
          [5]=>
          int(6)
          [6]=>
          int(7)
          [7]=>
          int(8)
          [8]=>
          int(9)
          [9]=>
          int(10)
        }
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(32)
      ["function"]=>
      string(12) "shuffleBytes"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(1) {
        [0]=>
        string(6) "foobar"
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(%d)
      ["function"]=>
      string(12) "shuffleArray"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(1) {
        [0]=>
        array(10) {
          [0]=>
          int(1)
          [1]=>
          int(2)
          [2]=>
          int(3)
          [3]=>
          int(4)
          [4]=>
          int(5)
          [5]=>
          int(6)
          [6]=>
          int(7)
          [7]=>
          int(8)
          [8]=>
          int(9)
          [9]=>
          int(10)
        }
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
object(RuntimeException)#%d (%d) {
  ["message":protected]=>
  string(29) "Random number generate failed"
  ["string":"Exception":private]=>
  string(0) ""
  ["code":protected]=>
  int(0)
  ["file":protected]=>
  string(%d) "%s"
  ["line":protected]=>
  int(%d)
  ["trace":"Exception":private]=>
  array(1) {
    [0]=>
    array(6) {
      ["file"]=>
      string(%d) "%s"
      ["line"]=>
      int(%d)
      ["function"]=>
      string(12) "shuffleBytes"
      ["class"]=>
      string(17) "Random\Randomizer"
      ["type"]=>
      string(2) "->"
      ["args"]=>
      array(1) {
        [0]=>
        string(6) "foobar"
      }
    }
  }
  ["previous":"Exception":private]=>
  NULL
}
