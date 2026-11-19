<?php

class Node {
    public int $value;
    public function __construct(int $v) { $this->value = $v; }
}

$sum = 0;
for ($i = 0; $i < 1000000; ++$i) {
    $b = new Node($i);
    $sum += $b->value;
    unset($b);
}
echo $sum . "\n";

