<?php

/** @generate-class-entries */

namespace {
    function lcg_value(): float {}

    function mt_srand(int $seed = 0, int $mode = MT_RAND_MT19937): void {}

    /** @alias mt_srand */
    function srand(int $seed = 0, int $mode = MT_RAND_MT19937): void {}

    function rand(int $min = UNKNOWN, int $max = UNKNOWN): int {}

    function mt_rand(int $min = UNKNOWN, int $max = UNKNOWN): int {}

    function mt_getrandmax(): int {}

    /** @alias mt_getrandmax */
    function getrandmax(): int {}

    /** @refcount 1 */
    function random_bytes(int $length): string {}

    function random_int(int $min, int $max): int {}
}

namespace Random\Engine
{
    class CombinedLCG implements Random\Engine
    {
        public function __construct(int $seed) {}

        public function generate(): string {}

        public function __serialize(): array {}

        public function __unserialize(array $data): void {}

        public function __debugInfo(): array {}
    }

    class MersenneTwister implements Random\Engine
    {
        public function __construct(int $seed, int $mode = MT_RAND_MT19937) {}

        /** @implementation-alias Random\Engine\CombinedLCG::generate */
        public function generate(): string {}

        /** @implementation-alias Random\Engine\CombinedLCG::__serialize */
        public function __serialize(): array {}

        /** @implementation-alias Random\Engine\CombinedLCG::__unserialize */
        public function __unserialize(array $data): void {}

        /** @implementation-alias Random\Engine\CombinedLCG::__debugInfo */
        public function __debugInfo(): array {}
    }

    /** @not-serializable */
    class Secure implements Random\Engine
    {
        public function __construct() {}

        /** @implementation-alias Random\Engine\CombinedLCG::generate */
        public function generate(): string {}
    }

    class XorShift128Plus implements Random\Engine
    {
        public function __construct(string|int $seed) {}

        /** @implementation-alias Random\Engine\CombinedLCG::generate */
        public function generate(): string {}

        /** @implementation-alias Random\Engine\CombinedLCG::__serialize */
        public function __serialize(): array {}

        /** @implementation-alias Random\Engine\CombinedLCG::__unserialize */
        public function __unserialize(array $data): void {}

        /** @implementation-alias Random\Engine\CombinedLCG::__debugInfo */
        public function __debugInfo(): array {}
    }

    class Xoshiro256StarStar implements Random\Engine
    {
        public function __construct(string|int $seed) {}

        /** @implementation-alias Random\Engine\CombinedLCG::generate */
        public function generate(): string {}

        /** @implementation-alias Random\Engine\CombinedLCG::__serialize */
        public function __serialize(): array {}

        /** @implementation-alias Random\Engine\CombinedLCG::__unserialize */
        public function __unserialize(array $data): void {}

        /** @implementation-alias Random\Engine\CombinedLCG::__debugInfo */
        public function __debugInfo(): array {}
    }
}

namespace Random
{
    interface Engine
    {
        public function generate(): string;
    }

    final class Randomizer
    {
        public readonly Engine $engine;

        public function __construct(?Engine $engine = null) {}

        public function getInt(int $min = UNKNOWN, int $max = UNKNOWN): int {}

        public function getBytes(int $legnth): string {}

        public function shuffleArray(array $array): array {}

        public function shuffleString(string $string): string {}

        public function __serialize(): array {}

        public function __unserialize(array $data): void {}
    }
}
