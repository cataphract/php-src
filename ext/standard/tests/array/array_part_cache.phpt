--TEST--
array_part(): usage of cache does not result in wrong returns
--FILE--
<?php

$arr = ['a', 'b', 'c', 'foo'=>'d', 'e', 'f', 'g', 'h'];

print_r(
    array_part($arr, [
        [2,1,3,-1,-2]
    ])
);
--EXPECT--
Array
(
    [0] => c
    [1] => b
    [2] => d
    [3] => h
    [4] => g
)
