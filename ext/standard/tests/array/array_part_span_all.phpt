--TEST--
array_part(): spans that encompass all elements
--FILE--
<?php

print_r(
    array_part([], [
        ['start'=>0, 'end'=>-1],
        [-1],
    ])
);

print_r(
    array_part([[1,2], []], [
        [-1],
        ['start'=>-1, 'end'=>0, 'step'=>-1],
    ])
);

print_r(
    array_part([[[1,2]], []], [
        ['start'=>null],
        ['start'=>null],
        ['start'=>null],
    ], true)
);
--EXPECT--
Array
(
)
Array
(
    [0] => Array
        (
        )

)
Array
(
    [0] => Array
        (
            [0] => Array
                (
                    [0] => 1
                    [1] => 2
                )

        )

    [1] => Array
        (
        )

)
