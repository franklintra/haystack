#include <stdio.h>
#include <string.h>
#include "http_prot.h"

void test_http_parse_message() {
    struct http_message out;
    int content_len;
    int result;

    // Test case 1: Incomplete headers
    const char *test1 = "GET / HTTP/1.1\r\nHost: localhost:8000\r\nAc";
    result = http_parse_message(test1, strlen(test1), &out, &content_len);
    printf("Test 1: %d (expected 0)\n", result);

    // Test case 2: Complete headers, no body
    const char *test2 = "GET / HTTP/1.1\r\nHost: localhost:8000\r\nAccept: */*\r\n\r\n";
    result = http_parse_message(test2, strlen(test2), &out, &content_len);
    printf("Test 2: %d (expected 1)\n", result);

    // Test case 3: Incomplete body
    const char *test3 = "GET / HTTP/1.1\r\nHost: localhost:8000\r\nContent-Length: 10\r\n\r\n01234";
    result = http_parse_message(test3, strlen(test3), &out, &content_len);
    printf("Test 3: %d (expected 0)\n", result);

    // Test case 4: Complete body
    const char *test4 = "GET / HTTP/1.1\r\nHost: localhost:8000\r\nContent-Length: 10\r\n\r\n0123456789";
    result = http_parse_message(test4, strlen(test4), &out, &content_len);
    printf("Test 4: %d (expected 1)\n", result);
}

int main() {
    test_http_parse_message();
    return 0;
}