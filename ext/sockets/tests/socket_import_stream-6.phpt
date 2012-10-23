--TEST--
socket_import_stream: call on unbound stream
--SKIPIF--
<?php
if (!extension_loaded('sockets')) {
	die('SKIP sockets extension not available.');
}
--FILE--
<?php

$stream0 = stream_socket_server("udp://0.0.0.0:58380", $errno, $errstr, 0);
var_dump(socket_import_stream($stream0));

echo "Done.\n";
--EXPECT--
XXXXXXXXXX
Done.
