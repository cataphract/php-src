--TEST--
array_part(): bad offsets for input
--FILE--
<?php

var_dump(
array_part([],[-1]),
array_part([],[-1], true),
array_part([1],[1]),
array_part([1],[1],true),
array_part([1],['foo'],true),
array_part([1],[-2]),
array_part([1],[[0,1]]),
array_part([1],[[0,1]],true),
array_part([],[['start'=>1]]),
array_part([],[['start'=>1]],true),
array_part([1],[['start'=>1]]),
array_part([1],[['start'=>1]],true),
array_part([1],[['end'=>'foo']],true),
array_part([1],[0,1])
);
--EXPECTF--

Warning: array_part(): Tried to get part of empty array in %s on line %d

Warning: array_part(): Tried to get part of empty array in %s on line %d

Warning: array_part(): Tried to get offset '1' from array with only 1 elements in %s on line %d

Warning: array_part(): Tried to get element with key '1' from array that does not have it in %s on line %d

Warning: array_part(): Tried to get element with key 'foo' from array that does not have it in %s on line %d

Warning: array_part(): The offset '-2' is too large in absolute value when accessing an array with only 1 elements in %s on line %d

Warning: array_part(): Tried to get offset '1' from array with only 1 elements in %s on line %d

Warning: array_part(): Tried to get element with key '1' from array that does not have it in %s on line %d

Warning: array_part(): Tried to get part of empty array with span specification that does not include all elements in %s on line %d

Warning: array_part(): Tried to get part of empty array with span specification that does not include all elements in %s on line %d

Warning: array_part(): Tried to get offset '1' from array with only 1 elements in %s on line %d

Warning: array_part(): Tried to get element with key '1' from array that does not have it in %s on line %d

Warning: array_part(): Tried to get element with key 'foo' from array that does not have it in %s on line %d

Warning: array_part(): The depth of the part specification is too large; tried to get part of non-array with part of index '1' in %s on line %d
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
