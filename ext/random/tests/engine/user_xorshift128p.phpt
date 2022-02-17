--TEST--
Random: Engine: User: XorShift128+
--SKIPIF--
<?php if (PHP_INT_SIZE != 8) die("skip: 64-bit only"); ?>
--FILE--
<?php

/*
 * Original code by Hiroaki Goto <deactivated@colopl.co.jp>
 */

class XorShift128Plus
{
    /* constants */
    protected const MASK_S5 = 0x07ffffffffffffff;
    protected const MASK_S18 = 0x00003fffffffffff;
    protected const MASK_S27 = 0x0000001fffffffff;
    protected const MASK_S30 = 0x00000003ffffffff;
    protected const MASK_S31 = 0x00000001ffffffff;
    protected const MASK_LO = 0x00000000ffffffff;

    protected const ADD_HI = 0x9e3779b9;
    protected const ADD_LO = 0x7f4a7c15;
    protected const MUL1_HILO = 0x476d;
    protected const MUL1_HIHI = 0xbf58;
    protected const MUL1_LO = 0x1ce4e5b9;
    protected const MUL2_HIHI = 0x94d0;
    protected const MUL2_HILO = 0x49bb;
    protected const MUL2_LO = 0x133111eb;

    /* states */
    protected int $s0;
    protected int $s1;

    public function __construct(int $seed)
    {
        $s = $seed;
        $this->s0 = $this->splitmix64($s);
        $this->s1 = $this->splitmix64($s);
    }

    public function generate(): string
    {
        $s1 = $this->s0;
        $s0 = $this->s1;

        $s0h = ($s0 >> 32) & self::MASK_LO;
        $s0l = $s0 & self::MASK_LO;
        $s1h = ($s1 >> 32) & self::MASK_LO;
        $s1l = $s1 & self::MASK_LO;
        $zl = $s0l + $s1l;
        $zh = $s0h + $s1h + ($zl >> 32);
        $z = ($zh << 32) | ($zl & self::MASK_LO);

        $this->s0 = $s0;
        $s1 ^= $s1 << 23;
        $this->s1 = $s1 ^ $s0 ^ (($s1 >> 18) & self::MASK_S18) ^ (($s0 >> 5) & self::MASK_S5);

        return \pack('q*', $z);
    }

    protected function splitmix64(int &$s): int
    {
        // $s + 0x9e3779b97f4a7c15
        $zl = $s & self::MASK_LO;
        $zh = ($s >> 32) & self::MASK_LO;
        $carry = $zl + self::ADD_LO;
        $z = $s = (($zh + self::ADD_HI + ($carry >> 32)) << 32) | ($carry & self::MASK_LO);

        $z ^= ($z >> 30) & self::MASK_S30;
        // $z = $z * 0xbf58476d1ce4e5b9 (mod 2^64)
        $zl = $z & self::MASK_LO;
        $zh = ($z >> 32) & self::MASK_LO;
        $lo = self::MUL1_LO * $zl;
        // $mul1 = 0xbf58476d * $zl (mod 2^32)
        $zll = $zl & 0xffff;
        $zlh = $zl >> 16;
        $mul1l = $zll * self::MUL1_HILO;
        $mul1h = $zll * self::MUL1_HIHI + $zlh * self::MUL1_HILO + (($mul1l >> 16) & 0xffff);
        $mul1 = (($mul1h & 0xffff) << 16) | ($mul1l & 0xffff);
        // $mul1 = ((self::MUL1_HI * $zl) & self::MASK_LO);
        $mul2 = ((self::MUL1_LO * $zh) & self::MASK_LO);
        $carry = (($lo >> 32) & self::MASK_LO);
        $hi = $mul1 + $mul2 + $carry;
        $z = ($hi << 32) | ($lo & self::MASK_LO);

        $z ^= ($z >> 27) & self::MASK_S27;
        // $z = $z * 0x94d049bb133111eb (mod 2^64)
        $zl = $z & self::MASK_LO;
        $zh = ($z >> 32) & self::MASK_LO;
        $lo = self::MUL2_LO * $zl;

        // $mul1 = 0x94d049bb * $zl (mod 2^32)
        $zll = $zl & 0xffff;
        $zlh = $zl >> 16;
        $mul1l = $zll * self::MUL2_HILO;
        $mul1h = $zll * self::MUL2_HIHI + $zlh * self::MUL2_HILO + (($mul1l >> 16) & 0xffff);
        $mul1 = (($mul1h & 0xffff) << 16) | ($mul1l & 0xffff);

        $mul2 = (self::MUL2_LO * $zh) & self::MASK_LO;
        $carry = ($lo >> 32) & self::MASK_LO;
        $hi = $mul1 + $mul2 + $carry;
        $z = ($hi << 32) | ($lo & self::MASK_LO);

        return $z ^ (($z >> 31) & self::MASK_S31);
    }
}

$g = new XorShift128Plus(1234);
$n = new \Random\Engine\XorShift128Plus(1234);

for ($i = 0; $i < 102400; $i++) {
    if ($g->generate() !== $n->generate()) {
        die('failure');
    }
}

die('success');

?>
--EXPECT--
success
