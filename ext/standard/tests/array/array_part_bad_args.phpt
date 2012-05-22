--TEST--
array_part(): bad arguments
--FILE--
<?php

var_dump(array_part());
var_dump(array_part(1,2,3,4));
var_dump(array_part(new stdclass,[1]));
var_dump(array_part([0, 1], new stdclass));
var_dump(array_part([0, 1], [1], array()));
--EXPECTF--

Warning: array_part() expects at least 2 parameters, 0 given in %s on line %d
bool(false)

Warning: array_part() expects at most 3 parameters, 4 given in %s on line %d
bool(false)

Warning: array_part() expects parameter 1 to be array, object given in %s on line %d
bool(false)

Warning: array_part() expects parameter 2 to be array, object given in %s on line %d
bool(false)

Warning: array_part() expects parameter 3 to be boolean, array given in %s on line %d
bool(false)
