--TEST--
array_part(): basic tests in key mode
--FILE--
<?php
$array = [
    [1, 2, 'foo' => 3],
    ['a', 'foo' => 'b', 'c', 'd'],
];

print_r(
    array_part($array, [
        ['start'=>null, 'end'=>null],
        "foo",
    ], true)
);

print_r(
    array_part($array, [
        ['end'=>null],
        ['start' => "foo", 'step' => -1],
    ], true)
);

print_r(
    array_part($array, [
        ['end'=>null],
        ["0", "1"],
    ], true)
);
--EXPECT--
Array
(
    [0] => 3
    [1] => b
)
Array
(
    [0] => Array
        (
            [0] => 3
            [1] => 2
            [2] => 1
        )

    [1] => Array
        (
            [0] => b
            [1] => a
        )

)
Array
(
    [0] => Array
        (
            [0] => 1
            [1] => 2
        )

    [1] => Array
        (
            [0] => a
            [1] => c
        )

)
