--TEST--
array_part(): basic tests in offset mode
--FILE--
<?php
$array = [
    [1, 2, 'foo' => 3],
    ['a', 'foo' => 'b', 'c', 'd'],
];

print_r(
    array_part($array, [
        ['start'=>0],
        ["-1"],
    ])
);

print_r(
    array_part($array, [
        ['start'=>0, 'end'=>-1],
        -1,
    ])
);

print_r(
    array_part($array, [
        0,
        -1,
    ])
);
echo "\n";

print_r(
    array_part($array, [
        [0],
        -1,
    ])
);

print_r(
    array_part($array, [
        ['start'=>0, 'end'=>-1],
        ['start'=>-2, 'end'=>-1],
    ])
);

print_r(
    array_part($array, [
        [1, 0],
        ['start'=>null, 'end'=>-2, 'step'=>-1],
    ])
);
--EXPECT--
Array
(
    [0] => Array
        (
            [0] => 3
        )

    [1] => Array
        (
            [0] => d
        )

)
Array
(
    [0] => 3
    [1] => d
)
3
Array
(
    [0] => 3
)
Array
(
    [0] => Array
        (
            [0] => 2
            [1] => 3
        )

    [1] => Array
        (
            [0] => c
            [1] => d
        )

)
Array
(
    [0] => Array
        (
            [0] => d
            [1] => c
        )

    [1] => Array
        (
            [0] => 3
            [1] => 2
        )

)
