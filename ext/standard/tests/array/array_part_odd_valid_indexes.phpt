--TEST--
array_part(): double and object valid indexes
--FILE--
<?php

var_dump(
    array_part(['a', 'b'], [1.])
);

class A {
function __toString() { return '1.'; }
}

var_dump(
    array_part(['a', 'b'], [new A])
);
--EXPECT--
string(1) "b"
string(1) "b"
