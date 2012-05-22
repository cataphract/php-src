--TEST--
array_part(): bad part specifications
--FILE--
<?php

$spe = [
	[[[]]],
    	[[1e50]],
    	["foobar"],
    	[["start"=>null, "step"=>'foobar']],
    	[["start"=>null, "step"=>0]],
    	[["start"=>null, "stepppp"=>0]],
    	[["start"=>[], "end"=>"1."]],
    	[["start"=>"1.", "end"=>[]]],
    	[[]],
    	[[-1=>1]],
    	[["foo"=>1]],
    	[["1", [], 2], 1],
    	[fopen('php://memory', 'r+')],
    	[],
    	[-1=>1],
    	['foo'=>1]
];
foreach ($spe as $s) {
var_dump(
	$s,
	array_part([], $s)
);
}
--EXPECTF--

Warning: array_part(): In part specification with index '0', expected only numeric values, but an incompatible data type has been found in %s on line %d
array(1) {
  [0]=>
  array(1) {
    [0]=>
    array(0) {
    }
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', found number out of bounds: '100000000000000007629769841091887003294964970946560.000000' in %s on line %d
array(1) {
  [0]=>
  array(1) {
    [0]=>
    float(1.0E+50)
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', expected only numeric values, but the string 'foobar' does not satisfy this requirement in %s on line %d
array(1) {
  [0]=>
  string(6) "foobar"
}
bool(false)

Warning: array_part(): In part specification with index '0', expected only numeric values, but the string 'foobar' does not satisfy this requirement in %s on line %d
array(1) {
  [0]=>
  array(2) {
    ["start"]=>
    NULL
    ["step"]=>
    string(6) "foobar"
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', a step of size 0 was specified in %s on line %d
array(1) {
  [0]=>
  array(2) {
    ["start"]=>
    NULL
    ["step"]=>
    int(0)
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', found span specification with extraneous elements in %s on line %d
array(1) {
  [0]=>
  array(2) {
    ["start"]=>
    NULL
    ["stepppp"]=>
    int(0)
  }
}
array(0) {
}

Warning: array_part(): In part specification with index '0', expected only numeric values, but an incompatible data type has been found in %s on line %d
array(1) {
  [0]=>
  array(2) {
    ["start"]=>
    array(0) {
    }
    ["end"]=>
    string(2) "1."
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', expected only numeric values, but an incompatible data type has been found in %s on line %d
array(1) {
  [0]=>
  array(2) {
    ["start"]=>
    string(2) "1."
    ["end"]=>
    array(0) {
    }
  }
}
bool(false)

Warning: array_part(): Part specification with index '0' is empty in %s on line %d
array(1) {
  [0]=>
  array(0) {
  }
}
bool(false)

Warning: array_part(): List of indexes in part specification with index '0' should be a numeric array, but either found non-sequential keys or the first key was not 0 (expected '0', got '-1') in %s on line %d
array(1) {
  [0]=>
  array(1) {
    [-1]=>
    int(1)
  }
}
bool(false)

Warning: array_part(): List of indexes in part specification with index '0' should be a numeric array, but found string index 'foo' in %s on line %d
array(1) {
  [0]=>
  array(1) {
    ["foo"]=>
    int(1)
  }
}
bool(false)

Warning: array_part(): In part specification with index '0', expected only numeric values, but an incompatible data type has been found in %s on line %d
array(2) {
  [0]=>
  array(3) {
    [0]=>
    string(1) "1"
    [1]=>
    array(0) {
    }
    [2]=>
    int(2)
  }
  [1]=>
  int(1)
}
bool(false)

Warning: array_part(): In part specification with index '0', expected only numeric values, but an incompatible data type has been found in %s on line %d
array(1) {
  [0]=>
  resource(5) of type (stream)
}
bool(false)

Warning: array_part(): Empty list of part specifications given in %s on line %d
array(0) {
}
bool(false)

Warning: array_part(): List of part specifications should be a numeric array, but found non-sequential keys or the first key was not 0 (expected '0', got '-1') in %s on line %d
array(1) {
  [-1]=>
  int(1)
}
bool(false)

Warning: array_part(): List of part specifications should be a numeric array, but found string index 'foo' in %s on line %d
array(1) {
  ["foo"]=>
  int(1)
}
bool(false)
