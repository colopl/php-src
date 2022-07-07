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
    final class MersenneTwister implements \Random\Engine
    {
        public function __construct(int|null $seed = null, int $mode = MT_RAND_MT19937) {}

        public function generate(): string {}

        public function __serialize(): array {}

        public function __unserialize(array $data): void {}

        public function __debugInfo(): array {}
    }

    final class PCG64 implements \Random\Engine
    {
        public function __construct(string|int|null $seed = null, string|int $seequence = 0) {}

        /** @implementation-alias Random\Engine\MersenneTwister::generate */
        public function generate(): string {}

        public function jump(int $advance): void {}
        
        /** @implementation-alias Random\Engine\MersenneTwister::__serialize */
        public function __serialize(): array {}

        /** @implementation-alias Random\Engine\MersenneTwister::__unserialize */
        public function __unserialize(array $data): void {}

        /** @implementation-alias Random\Engine\MersenneTwister::__debugInfo */
        public function __debugInfo(): array {}
    }

    /** @not-serializable */
    final class Secure implements \Random\CryptoSafeEngine
    {
        /** @implementation-alias Random\Engine\MersenneTwister::generate */
        public function generate(): string {}
    }
}

namespace Random
{
    interface Engine
    {
        public function generate(): string;
    }

    interface CryptoSafeEngine extends Engine
    {
    }

    final class Randomizer
    {
        public readonly Engine $engine;

        public function __construct(?Engine $engine = null) {}

        public function getInt(int $min = UNKNOWN, int $max = UNKNOWN): int {}

        public function getBytes(int $length): string {}

        public function shuffleArray(array $array): array {}

        public function shuffleString(string $string): string {}

        public function __serialize(): array {}

        public function __unserialize(array $data): void {}
    }
}
