--TEST--
mbstring does not free a stale detect order list after a failed request startup
--EXTENSIONS--
mbstring
zend_test
--SKIPIF--
<?php
if (!file_exists(__DIR__ . "/../../../sapi/cli/tests/php_cli_server.inc")) {
    echo "skip sapi/cli/tests/php_cli_server.inc required but not found";
}
?>
--FILE--
<?php

include __DIR__ . "/../../../sapi/cli/tests/php_cli_server.inc";

// zend_test's RINIT runs after mbstring's. When it fails, no RSHUTDOWN runs,
// but the CLI server keeps serving requests.
$prefix = substr(PHP_OS, 0, 3) == "WIN" ? "php_" : "";
$args = ["-d", "extension_dir=" . ini_get("extension_dir")];
foreach (["mbstring", "zend_test"] as $extension) {
    $args[] = "-d";
    $args[] = "extension=" . $prefix . $extension . "." . PHP_SHLIB_SUFFIX;
}
array_push($args, "-d", "zend_test.rinit_error_query=fail");

php_cli_server_start('echo "ok\n";', null, $args);

function request($query) {
    $fp = php_cli_server_connect();
    fwrite($fp, "GET /?$query HTTP/1.0\r\nHost: " . PHP_CLI_SERVER_HOSTNAME . "\r\n\r\n");
    $response = stream_get_contents($fp);
    fclose($fp);
    if (str_contains($response, "Request startup failure requested")) {
        return "startup failed";
    }
    $parts = explode("\r\n\r\n", $response, 2);
    return isset($parts[1]) ? trim($parts[1]) : "no response";
}

for ($i = 0; $i < 2; $i++) {
    echo request("fail"), "\n";
    echo request(""), "\n";
}
?>
--EXPECT--
startup failed
ok
startup failed
ok
