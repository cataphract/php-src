--TEST--
array_part(): particular span cases
--FILE--
<?php
$arr = ['a'=>'a', 'b'=>'b', 'c'=>'c'];

var_dump(
	array_part(
		$arr,
		[['start'=>2, 'end'=>1], [2]]
	),
	array_part(
		$arr,
		[['start'=>'c', 'end'=>'a']],
		true
	),
	array_part(
		$arr,
		[['start'=>'b', 'step'=>2]],
		true
	),
	array_part(
		$arr,
		[['start'=>'c', 'step'=>-3]],
		true
	),
	array_part(
		['a'=>'a', 'b'=>'b', 'c'=>'c', 'd'=>'d'],
		[['end'=>null, 'step'=>-3]],
		true
	)
);
--EXPECT--
array(0) {
}
array(0) {
}
array(1) {
  [0]=>
  string(1) "b"
}
array(1) {
  [0]=>
  string(1) "c"
}
array(2) {
  [0]=>
  string(1) "d"
  [1]=>
  string(1) "a"
}
