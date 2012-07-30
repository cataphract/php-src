--TEST--
Obtaining the value of inexistent dynamic properties is disallowed
--FILE--
<?php
class A {}

$a = new A;
$a->foo = "dynamic property";
$ro = new ReflectionObject($a);
$prop = $ro->getProperty('foo');
try {
	var_dump($prop->getValue($a));
	var_dump($prop->getValue(new A));
} catch (ReflectionException $ex) {
	echo "Caught!\n";
	try {
		var_dump($prop->getValue(new A));
	} catch (ReflectionException $ex) {
		var_dump($ex->getMessage());
	}
}

--EXPECT--
string(16) "dynamic property"
Caught!
string(49) "Dynamic property does not exist in given instance"
